#include "pch.h"
#include "SPH/Core/SceneBlueprint.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/kernels/SPHFunctions.h"

#define DEBUG_BUFFERS_CPU

namespace SPH
{
	struct CalculateHashAndParticleMapTask
	{
		ParticleBufferManager& particleBufferManager;
		ParticleBehaviourParameters& particleBehaviourParameters;

		uintMem particleSize;

		Array<std::atomic_uint32_t>& hashMap;
		uint32* particleMap;

		ResourceLockGuard initialParticlesLockGuard; //Filled by the thread
		DynamicParticle* initialParticles;		      //Filled by the thread
		ResourceLockGuard finalParticlesLockGuard; //Filled by the thread
		DynamicParticle* finalParticles;		      //Filled by the thread
	};
	struct SimulateParticlesTimeStepTask
	{
		ParticleBufferManager& dynamicParticlesBufferManager;
		ParticleBufferManager& staticParticlesBufferManager;
		ParticleBehaviourParameters& particleBehaviourParameters;

		Array<std::atomic_uint32_t>& dynamicParticlesHashMap;
		Array<std::atomic_uint32_t>& staticParticlesHashMap;
		uint32* particleMap;

		Array<Graphics::BasicIndexedMesh::Triangle>& triangles;

		//this is nullptr when parallelPartialSum is false
		std::atomic_uint32_t* hashMapGroupSums;

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
			task.initialParticlesLockGuard = task.particleBufferManager.LockWrite(nullptr);
			task.initialParticles = (DynamicParticle*)task.initialParticlesLockGuard.GetResource();
		}

		uintMem particleCount = task.particleBufferManager.GetParticleCount();
		uintMem begin = particleCount * context.GetThreadIndex() / context.GetThreadCount();
		uintMem end = particleCount * (context.GetThreadIndex() + 1) / context.GetThreadCount();

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			Details::ComputeDynamicParticlesHashAndPrepareHashMap(i, task.hashMap.Ptr(), task.hashMap.Count() - 1, task.initialParticles, task.particleBehaviourParameters.maxInteractionDistance, particleCount);

		//for (uintMem i = begin; i < end; ++i)
		//{
		//	Vec3u cell = SimulationEngine::GetCell(task.particles[i].position, task.particleBehaviourParameters.maxInteractionDistance);
		//	uint particleHash = SimulationEngine::GetHash(cell) % (task.hashMap.Count() - 1);
		//
		//	task.particles[i].hash = particleHash;
		//
		//	++task.hashMap[particleHash];
		//}

		context.SyncThreads();

		if (context.GetThreadIndex() == 0)
		{
#ifdef DEBUG_BUFFERS_CPU
			Array<uint> hashMap;
			hashMap.Resize(task.hashMap.Count());
			for (uintMem i = 0; i < task.hashMap.Count(); ++i)
				hashMap[i] = task.hashMap[i].load();


			SimulationEngine::DebugPrePrefixSumHashes(
				ArrayView<DynamicParticle>(task.initialParticles, particleCount),
				std::move(hashMap)
			);
#endif
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.hashMap.Count() - 1; ++i)
			{
				valueSum += task.hashMap[i];
				task.hashMap[i] = valueSum;
			}

			task.particleBufferManager.Advance();

			task.finalParticlesLockGuard = task.particleBufferManager.LockWrite(nullptr);
			task.finalParticles = (DynamicParticle*)task.finalParticlesLockGuard.GetResource();
		}

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			Details::ReorderDynamicParticlesAndFinishHashMap(i, task.particleMap, task.hashMap.Ptr(), task.initialParticles, task.finalParticles, particleCount);

		//for (uintMem i = begin; i < end; ++i)
		//	task.particleMap[--task.hashMap[task.particles[i].hash]] = i;

		context.SyncThreads();

		if (context.GetThreadIndex() == 0)
		{
#ifdef DEBUG_BUFFERS_CPU
			SimulationEngine::DebugHashAndParticleMap<std::atomic_uint32_t>({ task.finalParticles, particleCount }, task.hashMap, { task.particleMap, particleCount });
#endif
			task.initialParticles = nullptr;
			task.initialParticlesLockGuard.Unlock({});
			task.finalParticles = nullptr;
			task.finalParticlesLockGuard.Unlock({});
		}
	}
	static void SimulateParticlesTimeStep(const ThreadContext& context, SimulateParticlesTimeStepTask& task)
	{
		if (context.GetThreadIndex() == 0)
		{
			task.staticParticlesLockGuard = task.staticParticlesBufferManager.LockRead(nullptr);
			task.staticParticles = (StaticParticle*)task.staticParticlesLockGuard.GetResource();
		}

		for (uint i = 0; i < task.simulationSteps; ++i)
		{
			if (context.GetThreadIndex() == 0)
			{
				task.inputParticlesLockGuard = task.dynamicParticlesBufferManager.LockRead(nullptr);
				task.inputParticles = (DynamicParticle*)task.inputParticlesLockGuard.GetResource();

				task.dynamicParticlesBufferManager.Advance();

				task.outputParticlesLockGuard = task.dynamicParticlesBufferManager.LockWrite(nullptr);
				task.outputParticles = (DynamicParticle*)task.outputParticlesLockGuard.GetResource();
			}

			uintMem dynamicParticleCount = task.dynamicParticlesBufferManager.GetParticleCount();
			uintMem staticParticleCount = task.staticParticlesBufferManager.GetParticleCount();
			uintMem begin = dynamicParticleCount * context.GetThreadIndex() / context.GetThreadCount();
			uintMem end = dynamicParticleCount * (context.GetThreadIndex() + 1) / context.GetThreadCount();
			uintMem hashBegin = (task.dynamicParticlesHashMap.Count() - 1) * context.GetThreadIndex() / context.GetThreadCount();
			uintMem hashEnd = (task.dynamicParticlesHashMap.Count() - 1) * (context.GetThreadIndex() + 1) / context.GetThreadCount();

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
			{
				Details::UpdateParticlePressure(
					i,
					dynamicParticleCount,
					task.dynamicParticlesHashMap.Count() - 1,
					staticParticleCount,
					task.staticParticlesHashMap.Count() - 1,
					task.inputParticles,
					task.outputParticles,
					task.dynamicParticlesHashMap.Ptr(),
					task.particleMap,
					task.staticParticles,
					task.staticParticlesHashMap.Ptr(),
					&task.particleBehaviourParameters
				);
			}

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
				Details::UpdateParticleDynamics(
					i,
					dynamicParticleCount,
					task.dynamicParticlesHashMap.Count() - 1,
					staticParticleCount,
					task.staticParticlesHashMap.Count() - 1,
					task.inputParticles,
					task.outputParticles,
					task.dynamicParticlesHashMap.Ptr(),
					task.particleMap,
					task.staticParticles,
					task.staticParticlesHashMap.Ptr(),
					task.dt,
					&task.particleBehaviourParameters,
					task.triangles.Count(),
					(Triangle*)task.triangles.Ptr()
				);

			context.SyncThreads();

			for (uintMem i = hashBegin; i < hashEnd; ++i)
				task.dynamicParticlesHashMap[i].store(0);

			if (task.hashMapGroupSums != nullptr)
				task.hashMapGroupSums[context.GetThreadIndex()] = 0;

			context.SyncThreads();

			for (uintMem i = begin; i < end; ++i)
				++task.dynamicParticlesHashMap[task.outputParticles[i].hash];

			context.SyncThreads();

			if (context.GetThreadIndex() == 0)
			{
				task.inputParticles = nullptr;
				task.inputParticlesLockGuard.Unlock({});

#ifdef DEBUG_BUFFERS_CPU
				SimulationEngine::DebugParticles(
					ArrayView<DynamicParticle>(task.outputParticles, dynamicParticleCount),
					task.particleBehaviourParameters.maxInteractionDistance,
					task.dynamicParticlesHashMap.Count() - 1
				);

				Array<uint> hashMap;
				hashMap.Resize(task.dynamicParticlesHashMap.Count());
				for (uintMem i = 0; i < task.dynamicParticlesHashMap.Count(); ++i)
					hashMap[i] = task.dynamicParticlesHashMap[i].load();

				SimulationEngine::DebugPrePrefixSumHashes(
					ArrayView<DynamicParticle>(task.outputParticles, dynamicParticleCount),
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
					for (uintMem i = 0; i < task.dynamicParticlesHashMap.Count() - 1; ++i)
					{
						valueSum += task.dynamicParticlesHashMap[i];
						task.dynamicParticlesHashMap[i] = valueSum;
					}
				}
			}

			if (task.hashMapGroupSums != nullptr)
			{
				uint32 valueSum = 0;
				for (uintMem i = hashBegin; i < hashEnd; ++i)
				{
					valueSum += task.dynamicParticlesHashMap[i];
					task.dynamicParticlesHashMap[i] = valueSum;
				}

				for (uintMem i = context.GetThreadIndex(); i < context.GetThreadCount() - 1; ++i)
					task.hashMapGroupSums[i] += valueSum;

				context.SyncThreads();

				if (context.GetThreadIndex() != 0)
				{
					uintMem addition = task.hashMapGroupSums[context.GetThreadIndex() - 1];
					for (uintMem i = hashBegin; i < hashEnd; ++i)
						task.dynamicParticlesHashMap[i] += addition;
				}
			}

			if (context.GetThreadIndex() == 0)
			{
				if (task.reorderParticles)
				{
					task.dynamicParticlesBufferManager.Advance();
					task.orderedParticlesLockGuard = task.dynamicParticlesBufferManager.LockWrite(nullptr);
					task.orderedParticles = (DynamicParticle*)task.orderedParticlesLockGuard.GetResource();
				}
			}

			context.SyncThreads();

			if (task.reorderParticles)
			{
				for (uintMem i = begin; i < end; ++i)
				{
					Details::ReorderDynamicParticlesAndFinishHashMap(
						i,
						task.particleMap,
						task.dynamicParticlesHashMap.Ptr(),
						task.outputParticles,
						task.orderedParticles,
						dynamicParticleCount
					);
				}
			}
			else
			{
				for (uintMem i = begin; i < end; ++i)
				{
					Details::FillDynamicParticleMapAndFinishHashMap(
						i,
						task.particleMap,
						task.dynamicParticlesHashMap.Ptr(),
						task.outputParticles,
						dynamicParticleCount
					);
				}
			}

			context.SyncThreads();

			if (context.GetThreadIndex() == 0)
			{
#ifdef DEBUG_BUFFERS_CPU


				SimulationEngine::DebugHashAndParticleMap<std::atomic_uint32_t>(
					ArrayView<DynamicParticle>(task.orderedParticles ? task.orderedParticles : task.outputParticles, dynamicParticleCount),
					task.dynamicParticlesHashMap,
					ArrayView<uint32>(task.particleMap, dynamicParticleCount)
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

	CPUSimulationEngine::CPUSimulationEngine(uintMem threadCount) :
		dynamicParticlesBufferManager(nullptr),
		staticParticlesBufferManager(nullptr),
		reorderParticlesElapsedTime(0),
		reorderParticlesTimeInterval(FLT_MAX),
		parallelPartialSum(true),
		simulationTime(0)
	{
		threadManager.AllocateThreads(threadCount);
	}
	CPUSimulationEngine::~CPUSimulationEngine()
	{
		Clear();
	}
	void CPUSimulationEngine::Clear()
	{
		if (dynamicParticlesBufferManager != nullptr)
		{
			dynamicParticlesBufferManager->FlushAllOperations();
			dynamicParticlesBufferManager = nullptr;
		}

		if (staticParticlesBufferManager != nullptr)
		{
			staticParticlesBufferManager->FlushAllOperations();
			staticParticlesBufferManager = nullptr;
		}

		threadManager.FinishTasks();

		hashMapGroupSums.Clear();
		dynamicParticlesHashMap.Clear();
		particleMap.Clear();

		staticParticlesHashMap.Clear();

		particleBehaviourParameters = { };

		reorderParticlesElapsedTime = 0.0f;
		reorderParticlesTimeInterval = FLT_MAX;

		parallelPartialSum = false;

		simulationTime = 0;
	}
	void CPUSimulationEngine::Initialize(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager)
	{
		Clear();

		auto parameters = scene.GetSystemParameters();
		parameters.ParseParameter("reorderTimeInterval", reorderParticlesTimeInterval);
		parameters.ParseParameter("parallelPartialSum", parallelPartialSum);

		particleBehaviourParameters = parameters.particleBehaviourParameters;
		//TODO calculate this somewhere else
		particleBehaviourParameters.smoothingKernelConstant = SmoothingKernelConstant(parameters.particleBehaviourParameters.maxInteractionDistance);
		particleBehaviourParameters.selfDensity = parameters.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, parameters.particleBehaviourParameters.maxInteractionDistance) * particleBehaviourParameters.smoothingKernelConstant;

		this->dynamicParticlesBufferManager = &dynamicParticlesBufferManager;
		this->staticParticlesBufferManager = &staticParticlesBufferManager;

		if (parallelPartialSum)
			hashMapGroupSums = Array<std::atomic_uint32_t>(threadManager.ThreadCount());

		InitializeStaticParticles(scene, staticParticlesBufferManager);
		InitializeDynamicParticles(scene, dynamicParticlesBufferManager);

		triangles = scene.GetMesh().CreateTriangleArray();
	}
	void CPUSimulationEngine::Update(float deltaTime, uint simulationSteps)
	{
		if (dynamicParticlesBufferManager == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (dynamicParticlesBufferManager->GetParticleCount() == 0)
			return;

		if (threadManager.TryEnqueueTask(SimulateParticlesTimeStep, SimulateParticlesTimeStepTask{
			.dynamicParticlesBufferManager = *dynamicParticlesBufferManager,
			.staticParticlesBufferManager = *staticParticlesBufferManager,
			.particleBehaviourParameters = particleBehaviourParameters,
			.dynamicParticlesHashMap = dynamicParticlesHashMap,
			.staticParticlesHashMap = staticParticlesHashMap,
			.particleMap = particleMap.Ptr(),
			.triangles = triangles,
			.hashMapGroupSums = hashMapGroupSums.Ptr(),
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
			.staticParticles = nullptr
			}))
		{
			simulationTime += deltaTime * simulationSteps;

			reorderParticlesElapsedTime += deltaTime;
			if (reorderParticlesElapsedTime > reorderParticlesTimeInterval)
				reorderParticlesElapsedTime = 0;
		}
	}
	void CPUSimulationEngine::InitializeStaticParticles(SceneBlueprint& scene, ParticleBufferManager& staticParticlesBufferManager)
	{
		Array<StaticParticle> staticParticles;
		scene.GenerateLayerParticles("static", staticParticles);

		if (staticParticles.Empty())
			return;

		staticParticlesHashMap = Array<std::atomic_uint32_t>(staticParticles.Count() + 1);
		staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticlesHashMap, particleBehaviourParameters.maxInteractionDistance);

		staticParticlesBufferManager.Allocate(sizeof(StaticParticle), staticParticles.Count(), staticParticles.Ptr(), 1);

#ifdef DEBUG_BUFFERS_CPU
		DebugParticles<StaticParticle>(staticParticles, particleBehaviourParameters.maxInteractionDistance, staticParticlesHashMap.Count() - 1);
		DebugHashAndParticleMap<std::atomic_uint32_t>(staticParticles, staticParticlesHashMap, particleBehaviourParameters.maxInteractionDistance);
#endif
	}
	void CPUSimulationEngine::InitializeDynamicParticles(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager)
	{
		Array<DynamicParticle> dynamicParticles;
		scene.GenerateLayerParticles("dynamic", dynamicParticles);

		if (dynamicParticles.Empty())
			return;

		dynamicParticlesHashMap = Array<std::atomic_uint32_t>(2 * dynamicParticles.Count() + 1);
		dynamicParticlesHashMap.Last() = (uint)dynamicParticles.Count();

		particleMap.Resize(dynamicParticles.Count());

		dynamicParticlesBufferManager.Allocate(sizeof(DynamicParticle), dynamicParticles.Count(), dynamicParticles.Ptr(), 3);

		threadManager.EnqueueTask(CalculateHashAndParticleMap, CalculateHashAndParticleMapTask {
				.particleBufferManager = dynamicParticlesBufferManager,
				.particleBehaviourParameters = particleBehaviourParameters,
				.particleSize = sizeof(DynamicParticle),
				.hashMap = dynamicParticlesHashMap,
				.particleMap = particleMap.Ptr(),
				.initialParticlesLockGuard = ResourceLockGuard(),
				.initialParticles = nullptr,
				.finalParticlesLockGuard = ResourceLockGuard(),
				.finalParticles = nullptr,
			});
	}
}