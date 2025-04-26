#include "pch.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/kernels/SPHFunctions.h"

//#define DEBUG_BUFFERS_CPU

namespace SPH
{
	struct CalculateHashAndParticleMapTask
	{
		ParticleBufferManager& particleBufferManager;
		ParticleBehaviourParameters& particleBehaviourParameters;

		uintMem dynamicParticlesCount;
		uintMem dynamicParticlesHashMapSize;

		std::atomic_uint32_t* hashMap;
		uint32* particleMap;

		ResourceLockGuard particlesLockGuard; //Filled by the thread
		DynamicParticle* particles;		      //Filled by the thread			
	};
	struct SimulateParticlesTimeStepTask
	{
		ParticleBufferManager& particleBufferManager;

		uintMem dynamicParticlesCount;
		uintMem dynamicParticlesHashMapSize;
		uintMem staticParticlesCount;
		uintMem staticParticlesHashMapSize;

		std::atomic_uint32_t* dynamicParticlesHashMapBuffer;
		uint32* particleMap;

		//this is nullptr when parallelPartialSum is false
		std::atomic_uint32_t* hashMapGroupSums;

		uint32* staticParticlesHashMapBuffer;

		ParticleBehaviourParameters& particleBehaviourParameters;

		uint simulationSteps;
		float dt;

		bool reorderParticles;

		ResourceLockGuard inputParticlesLockGuard;
		DynamicParticle* inputParticles;
		ResourceLockGuard outputParticlesLockGuard;
		DynamicParticle* outputParticles;
		ResourceLockGuard orderedParticlesLockGuard;
		DynamicParticle* orderedParticles;
		ResourceLockGuard staticParticlesLockGuard;
		StaticParticle* staticParticles;
	};

	static void CalculateHashAndParticleMap(const ThreadContext& context, CalculateHashAndParticleMapTask& task)
	{
		if (context.GetThreadIndex() == 0)
		{
			task.particlesLockGuard = task.particleBufferManager.LockDynamicParticlesForWrite(nullptr);
			task.particles = (DynamicParticle*)task.particlesLockGuard.GetResource();
		}

		uintMem begin = task.particleBufferManager.GetDynamicParticleCount() * context.GetThreadIndex() / context.GetThreadCount();
		uintMem end = task.particleBufferManager.GetDynamicParticleCount() * (context.GetThreadIndex() + 1) / context.GetThreadCount();

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{
			Vec3u cell = System::GetCell(task.particles[i].position, task.particleBehaviourParameters.maxInteractionDistance);
			uint particleHash = System::GetHash(cell) % task.dynamicParticlesHashMapSize;

			task.particles[i].hash = particleHash;

			++task.hashMap[particleHash];
		}

		context.SyncThreads();

		if (context.GetThreadIndex() == 0)
		{
#ifdef DEBUG_BUFFERS_CPU			
			Array<uint> hashMap;
			hashMap.Resize(task.dynamicParticlesHashMapSize + 1);
			for (uintMem i = 0; i < task.dynamicParticlesHashMapSize + 1; ++i)
				hashMap[i] = task.hashMap[i].load();

			
			System::DebugPrePrefixSumHashes(
				ArrayView<DynamicParticle>(task.particles, task.dynamicParticlesCount),
				std::move(hashMap)
			);
#endif
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.dynamicParticlesHashMapSize; ++i)
			{
				valueSum += task.hashMap[i];
				task.hashMap[i] = valueSum;
			}
		}

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			task.particleMap[--task.hashMap[task.particles[i].hash]] = i;

		context.SyncThreads();

		if (context.GetThreadIndex() == 0)
		{
#ifdef DEBUG_BUFFERS_CPU
			System::DebugHashAndParticleMap(
				ArrayView<DynamicParticle>(task.particles, task.dynamicParticlesCount),
				ArrayView<std::atomic_uint32_t>(task.hashMap, task.dynamicParticlesHashMapSize + 1),
				ArrayView<uint32>(task.particleMap, task.dynamicParticlesCount)
			);
#endif
			task.particles = nullptr;
			task.particlesLockGuard.Unlock({});
		}
	}
	static void SimulateParticlesTimeStep(const ThreadContext& context, SimulateParticlesTimeStepTask& task)
	{
		if (context.GetThreadIndex() == 0)
		{
			task.staticParticlesLockGuard = task.particleBufferManager.LockStaticParticlesForRead(nullptr);
			task.staticParticles = (StaticParticle*)task.staticParticlesLockGuard.GetResource();
		}

		for (uint i = 0; i < task.simulationSteps; ++i)
		{
			if (context.GetThreadIndex() == 0)
			{
				task.inputParticlesLockGuard = task.particleBufferManager.LockDynamicParticlesForRead(nullptr);
				task.inputParticles = (DynamicParticle*)task.inputParticlesLockGuard.GetResource();

				task.particleBufferManager.Advance();

				task.outputParticlesLockGuard = task.particleBufferManager.LockDynamicParticlesForWrite(nullptr);
				task.outputParticles = (DynamicParticle*)task.outputParticlesLockGuard.GetResource();
			}

			uintMem begin = task.dynamicParticlesCount * context.GetThreadIndex() / context.GetThreadCount();
			uintMem end = task.dynamicParticlesCount * (context.GetThreadIndex() + 1) / context.GetThreadCount();
			uintMem hashBegin = task.dynamicParticlesHashMapSize * context.GetThreadIndex() / context.GetThreadCount();
			uintMem hashEnd = task.dynamicParticlesHashMapSize * (context.GetThreadIndex() + 1) / context.GetThreadCount();

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
			{
				Details::UpdateParticlePressure(
					i,
					task.dynamicParticlesCount,
					task.dynamicParticlesHashMapSize,
					task.staticParticlesCount,
					task.staticParticlesHashMapSize,
					task.inputParticles,
					task.outputParticles,
					task.dynamicParticlesHashMapBuffer,
					task.particleMap,
					task.staticParticles,
					task.staticParticlesHashMapBuffer,
					&task.particleBehaviourParameters
				);
			}

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
				Details::UpdateParticleDynamics(
					i,
					task.dynamicParticlesCount,
					task.dynamicParticlesHashMapSize,
					task.staticParticlesCount,
					task.staticParticlesHashMapSize,
					task.inputParticles,
					task.outputParticles,
					task.dynamicParticlesHashMapBuffer,
					task.particleMap,
					task.staticParticles,
					task.staticParticlesHashMapBuffer,
					task.dt,
					&task.particleBehaviourParameters
				);

			context.SyncThreads();

			for (uintMem i = hashBegin; i < hashEnd; ++i)
				task.dynamicParticlesHashMapBuffer[i].store(0);

			if (task.hashMapGroupSums != nullptr)
				task.hashMapGroupSums[context.GetThreadIndex()] = 0;

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
				++task.dynamicParticlesHashMapBuffer[task.outputParticles[i].hash];

			context.SyncThreads();

			if (context.GetThreadIndex() == 0)
			{
				task.inputParticles = nullptr;
				task.inputParticlesLockGuard.Unlock({});

#ifdef DEBUG_BUFFERS_CPU
				System::DebugParticles(
					ArrayView<DynamicParticle>(task.outputParticles, task.dynamicParticlesCount),
					task.particleBehaviourParameters.maxInteractionDistance,
					task.dynamicParticlesHashMapSize
				);

				Array<uint> hashMap;
				hashMap.Resize(task.dynamicParticlesHashMapSize + 1);
				for (uintMem i = 0; i < task.dynamicParticlesHashMapSize + 1; ++i)
					hashMap[i] = task.dynamicParticlesHashMapBuffer[i].load();

				System::DebugPrePrefixSumHashes(
					ArrayView<DynamicParticle>(task.outputParticles, task.dynamicParticlesCount),
					std::move(hashMap)
				);
			}

			context.SyncThreads();

			if (context.GetThreadIndex() == 0)
			{
#endif

				if (task.hashMapGroupSums == nullptr)
				{
					uint32 valueSum = 0;
					for (uintMem i = 0; i < task.dynamicParticlesHashMapSize; ++i)
					{
						valueSum += task.dynamicParticlesHashMapBuffer[i];
						task.dynamicParticlesHashMapBuffer[i] = valueSum;
					}
				}
			}

			if (task.hashMapGroupSums != nullptr)
			{
				uint32 valueSum = 0;
				for (uintMem i = hashBegin; i < hashEnd; ++i)
				{
					valueSum += task.dynamicParticlesHashMapBuffer[i];
					task.dynamicParticlesHashMapBuffer[i] = valueSum;
				}

				for (uintMem i = context.GetThreadIndex(); i < context.GetThreadCount() - 1; ++i)
					task.hashMapGroupSums[i] += valueSum;

				context.SyncThreads();

				if (context.GetThreadIndex() != 0)
				{
					uintMem addition = task.hashMapGroupSums[context.GetThreadIndex() - 1];
					for (uintMem i = hashBegin; i < hashEnd; ++i)
						task.dynamicParticlesHashMapBuffer[i] += addition;
				}
			}

			if (context.GetThreadIndex() == 0)
			{
				if (task.reorderParticles)
				{
					task.particleBufferManager.Advance();
					task.orderedParticlesLockGuard = task.particleBufferManager.LockDynamicParticlesForWrite(nullptr);
					task.orderedParticles = (DynamicParticle*)task.orderedParticlesLockGuard.GetResource();
				}
			}

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
				Details::ComputeParticleMap(
					i,
					task.outputParticles,
					task.orderedParticles,
					task.dynamicParticlesHashMapBuffer,
					task.particleMap,
					task.orderedParticles != nullptr
				);

			context.SyncThreads();

			if (context.GetThreadIndex() == 0)
			{
#ifdef DEBUG_BUFFERS_CPU

				
				System::DebugHashAndParticleMap(
					ArrayView<DynamicParticle>(task.orderedParticles ? task.orderedParticles : task.outputParticles, task.dynamicParticlesCount),
					ArrayView<std::atomic_uint32_t>(task.dynamicParticlesHashMapBuffer, task.dynamicParticlesHashMapSize + 1),
					ArrayView<uint32>(task.particleMap, task.dynamicParticlesCount)
				);
#endif

				task.outputParticles = nullptr;
				task.outputParticlesLockGuard.Unlock({});

				if (task.reorderParticles)
				{
					task.orderedParticles = nullptr;
					task.orderedParticlesLockGuard.Unlock({});
				}

			}
		}

		if (context.GetThreadIndex() == 0)
		{
			task.staticParticles = nullptr;
			task.staticParticlesLockGuard.Unlock({});
		}
	}

	SystemCPU::SystemCPU(uintMem threadCount) : 		
		particleBufferManager(nullptr), 
		reorderParticlesElapsedTime(0), 
		reorderParticlesTimeInterval(FLT_MAX), 
		parallelPartialSum(true),
		simulationTime(0)
	{				
		threadManager.AllocateThreads(threadCount);
	}
	SystemCPU::~SystemCPU()
	{
		Clear();
	}	
	void SystemCPU::Clear()
	{
		WaitForTasksToFinish();
		
		hashMapGroupSums.Clear();
		dynamicParticlesHashMapBuffer.Clear();		
		particleMap.Clear();
		
		staticParticlesHashMapBuffer.Clear();		

		particleBehaviourParameters = { };

		reorderParticlesElapsedTime = 0.0f;
		reorderParticlesTimeInterval = FLT_MAX;

		parallelPartialSum = false;

		simulationTime = 0;		
	}
	void SystemCPU::Initialize(const SystemParameters& parameters, ParticleBufferManager& particleBufferManager, Array<DynamicParticle> dynamicParticles, Array<StaticParticle> staticParticles)
	{
		Clear();	

		parameters.ParseParameter("reorderTimeInterval", reorderParticlesTimeInterval);
		parameters.ParseParameter("parallelPartialSum", parallelPartialSum);


		particleBehaviourParameters = parameters.particleBehaviourParameters;
		//TODO calculate this somewhere else
		particleBehaviourParameters.smoothingKernelConstant = SmoothingKernelConstant(parameters.particleBehaviourParameters.maxInteractionDistance);
		particleBehaviourParameters.selfDensity = parameters.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, parameters.particleBehaviourParameters.maxInteractionDistance) * particleBehaviourParameters.smoothingKernelConstant;

		this->particleBufferManager = &particleBufferManager;

		if (parallelPartialSum)
			hashMapGroupSums = Array<std::atomic_uint32_t>(threadManager.ThreadCount());

		if (!staticParticles.Empty())
		{
			staticParticlesHashMapBuffer.Resize(staticParticles.Count() + 1);
			staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticlesHashMapBuffer, parameters.particleBehaviourParameters.maxInteractionDistance);

			particleBufferManager.AllocateStaticParticles(staticParticles.Count(), staticParticles.Ptr());

#ifdef DEBUG_BUFFERS_CPU
			DebugParticles<StaticParticle>(staticParticles, parameters.particleBehaviourParameters.maxInteractionDistance, staticParticlesHashMapBuffer.Count() - 1);			
			DebugHashAndParticleMap<uint32>(staticParticles, staticParticlesHashMapBuffer, parameters.particleBehaviourParameters.maxInteractionDistance);
#endif
		}

		if (!dynamicParticles.Empty())		
		{
			dynamicParticlesHashMapBuffer = Array<std::atomic_uint32_t>(2 * dynamicParticles.Count() + 1, 0);
			dynamicParticlesHashMapBuffer.Last() = (uint)dynamicParticles.Count();

			particleMap.Resize(dynamicParticles.Count());

			particleBufferManager.AllocateDynamicParticles(dynamicParticles.Count(), dynamicParticles.Ptr());

			threadManager.EnqueueTask(CalculateHashAndParticleMap, {
					.particleBufferManager = particleBufferManager,
					.particleBehaviourParameters = particleBehaviourParameters,
					.dynamicParticlesCount = particleBufferManager.GetDynamicParticleCount(),
					.dynamicParticlesHashMapSize = dynamicParticlesHashMapBuffer.Count() - 1,
					.hashMap = dynamicParticlesHashMapBuffer.Ptr(),
					.particleMap = particleMap.Ptr(),
					.particlesLockGuard = ResourceLockGuard(),
					.particles = nullptr,
				});
		}				
	}
	void SystemCPU::Update(float deltaTime, uint simulationSteps)
	{
		if (particleBufferManager == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (particleBufferManager->GetDynamicParticleCount() == 0)
			return;		
 		
		if (threadManager.TryEnqueueTask(SimulateParticlesTimeStep, SimulateParticlesTimeStepTask{
			.particleBufferManager = *particleBufferManager,
			.dynamicParticlesCount = particleBufferManager->GetDynamicParticleCount(),
			.dynamicParticlesHashMapSize = dynamicParticlesHashMapBuffer.Count() - 1,
			.staticParticlesCount = particleBufferManager->GetStaticParticleCount(),
			.staticParticlesHashMapSize = staticParticlesHashMapBuffer.Count() - 1,
			.dynamicParticlesHashMapBuffer = dynamicParticlesHashMapBuffer.Ptr(),
			.particleMap = particleMap.Ptr(),
			.hashMapGroupSums = hashMapGroupSums.Ptr(),
			.staticParticlesHashMapBuffer = staticParticlesHashMapBuffer.Ptr(),
			.particleBehaviourParameters = particleBehaviourParameters,
			.simulationSteps = simulationSteps,
			.dt = deltaTime,
			.reorderParticles = reorderParticlesElapsedTime > reorderParticlesTimeInterval,
			.inputParticlesLockGuard = ResourceLockGuard(),
			.inputParticles = nullptr,
			.outputParticlesLockGuard = ResourceLockGuard(),
			.outputParticles = nullptr,
			.orderedParticlesLockGuard = ResourceLockGuard(),
			.orderedParticles = nullptr,
			.staticParticlesLockGuard = ResourceLockGuard(),
			.staticParticles = nullptr,
			}))
		{
			simulationTime += deltaTime * simulationSteps;

			reorderParticlesElapsedTime += deltaTime;
			if (reorderParticlesElapsedTime > reorderParticlesTimeInterval)
				reorderParticlesElapsedTime = 0;
		}		
	}		
	void SystemCPU::WaitForTasksToFinish()
	{
		if (particleBufferManager != nullptr)
		{
			particleBufferManager->FlushAllOperations();
			particleBufferManager = nullptr;
		}

		threadManager.FinishTasks();
	}	
}