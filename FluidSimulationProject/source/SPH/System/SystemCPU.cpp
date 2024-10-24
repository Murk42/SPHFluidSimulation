#include "pch.h"
#include "SystemCPU.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{
	void TaskThreadContext::SyncThreads()
	{
		if (context.threadPool.ThreadCount() == 0)
			return;

		std::unique_lock<std::mutex> lock{ context.mutex };
		
		++context.threadSyncCount1;
		context.syncCV.notify_all();				
		context.syncCV.wait(lock, [&]() { return context.threadSyncCount1 % context.threadPool.ThreadCount() == 0; });				

		++context.threadSyncCount2;
		context.syncCV.notify_all();
		context.syncCV.wait(lock, [&]() { return context.threadSyncCount2 % context.threadPool.ThreadCount() == 0; });
	}
	TaskThreadContext::TaskThreadContext(ThreadContext& context) : context(context)
	{
	}
	ThreadContext::ThreadContext(ThreadPool& threadPool) :
		threadPool(threadPool), threadIdleCount(0), threadSyncCount1(0), exit(false), taskThreadContext(*this), threadSyncCount2(0)
	{		
		if (threadPool.IsAnyRunning())
		{
			{
				std::lock_guard lock{ mutex };
				exit = true;
				idleCV.notify_all();
			}

			if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
				Debug::Logger::LogWarning("Client", "Threads didn't exit on time");
		}

		threadIdleCount = 0;
		threadSyncCount1 = 0;
		threadSyncCount2 = 0;
		exit = false;

		if (threadPool.ThreadCount() == 0)
			return;

		threadPool.RunTask([this](uintMem threadID, uintMem threadCount) -> uint {
			SimulationThreadFunc(taskThreadContext, threadID, threadCount);
			return 0;
			});
	}
	ThreadContext::~ThreadContext()
	{		
		if (threadPool.ThreadCount() == 0)
			return;

		if (!threadPool.IsAnyRunning())
			return;

		{
			std::lock_guard lock{ mutex };			
			exit = true;			
			idleCV.notify_all();
		}

		if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
			Debug::Logger::LogWarning("Client", "Threads didn't exit on time");		
	}
	void ThreadContext::FinishTasks()
	{
		if (!threadPool.IsAnyRunning())
			return;

		std::unique_lock<std::mutex> lock{ mutex };
		idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
	}	
	void ThreadContext::EnqueueTask(TaskFunction taskFunction)
	{
		if (threadPool.ThreadCount() == 0)		
			taskFunction(taskThreadContext, 0, 1);		
		else
		{
			std::unique_lock lk{ mutex };						
			taskTakenCV.wait(lk, [&]() { return tasks.size() < 5; });			

			tasks.push(taskFunction);			
			idleCV.notify_all();
		}
	}	
	void ThreadContext::SimulationThreadFunc(TaskThreadContext& taskThreadContext, uintMem threadID, uintMem threadCount)
	{		
		while (true)
		{			
			std::unique_lock<std::mutex> lock{ mutex };
			
			++threadIdleCount;
			idleCV.notify_all();
			idleCV.wait(lock, [&]() { return exit || !tasks.empty(); });			
			--threadIdleCount;
			
			if (exit)
				break;

			TaskFunction task = tasks.front();
			
			lock.unlock();

			taskThreadContext.SyncThreads();			

			if (threadID == 0)
			{
				std::lock_guard lg{ lock };				

				taskTakenCV.notify_all();
				tasks.pop();
			}

			task(taskThreadContext, threadID, threadCount);
		}
	}

	SystemCPU::SystemCPU(ThreadPool& threadPool) :
		threadContext(threadPool)
	{				
	}
	SystemCPU::~SystemCPU()
	{
		Clear();
	}	
	void SystemCPU::Clear()
	{
		threadContext.FinishTasks();

		dynamicParticleCount = 0;
		dynamicParticleHashMapSize = 0;
		staticParticleCount = 0;
		staticParticleHashMapSize = 0;
		
		dynamicParticleReadHashMapBuffer.Clear();
		dynamicParticleWriteHashMapBuffer.Clear();
		particleMap.Clear();
		staticParticles.Clear();
		staticParticleHashMap.Clear();		

		simulationParameters = { };

		simulationTime = 0;
		lastTimePerStep_s = 0;
	}
	void SystemCPU::Update(float deltaTime, uint simulationSteps)
	{
		if (particleBufferSet == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (profiling)
			executionStopwatch.Reset();

		for (uint i = 0; i < simulationSteps; ++i)
		{
			SimulateParticlesTimeStepTask task{				
				.particleReadBufferHandle = particleBufferSet->GetReadBufferHandle(),
				.particleWriteBufferHandle = particleBufferSet->GetWriteBufferHandle(),
				.dynamicParticleReadHashMapBuffer = dynamicParticleReadHashMapBuffer.Ptr(),
				.dynamicParticleWriteHashMapBuffer = dynamicParticleWriteHashMapBuffer.Ptr(),
				.particleMap = particleMap.Ptr(),
				.staticParticles = staticParticles.Ptr(),
				.staticParticleHashMap = staticParticleHashMap.Ptr(),
				.simulationParameters = simulationParameters,
				.reorderParticles = false,
				.dt = deltaTime,
			};
			threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem threadID, uintMem threadCount) {
				uintMem begin = task.simulationParameters.dynamicParticleCount * threadID / threadCount;
				uintMem end = task.simulationParameters.dynamicParticleCount * (threadID + 1) / threadCount;
				SimulateParticlesTimeStep(context, begin, end, task);
				});

			std::swap(dynamicParticleReadHashMapBuffer, dynamicParticleWriteHashMapBuffer);
			
			this->particleBufferSet->Advance();
		}

		if (profiling)
		{
			threadContext.FinishTasks();

			lastTimePerStep_s = executionStopwatch.GetTime() / simulationSteps;
		}

		simulationTime += deltaTime * simulationSteps;
	}		
	void SystemCPU::EnableProfiling(bool enable)
	{
		this->profiling = enable;
	}
	SystemProfilingData SystemCPU::GetProfilingData()
	{ 
		SystemProfilingData out;
		out.timePerStep_s = lastTimePerStep_s;
		return out;		
	}	
	void SystemCPU::CreateStaticParticlesBuffers(Array<StaticParticle>& inStaticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance)
	{
		staticParticleCount = inStaticParticles.Count();
		staticParticleHashMapSize = hashesPerStaticParticle * staticParticleCount;
				
		staticParticleHashMap.Resize(staticParticleHashMapSize + 1);		

		auto GetStaticParticleHash = [maxInteractionDistance = maxInteractionDistance, mod = staticParticleHashMapSize](const StaticParticle& particle) {
			return GetHash(GetCell(particle.position, maxInteractionDistance)) % mod;
			};

		staticParticles = GenerateHashMapAndReorderParticles(inStaticParticles, staticParticleHashMap, GetStaticParticleHash);

#ifdef DEBUG_BUFFERS_CPU
		DebugParticles<StaticParticle>(staticParticles, maxInteractionDistance, staticParticleHashMapSize);
		DebugHashAndParticleMap<StaticParticle, uint32>(staticParticles, staticParticleHashMap, {}, GetStaticParticleHash);
#endif		
	}
	void SystemCPU::CreateDynamicParticlesBuffers(ParticleBufferSet& bufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance)
	{
		particleBufferSet = &dynamic_cast<CPUParticleBufferSet&>(bufferSet);
		dynamicParticleCount = particleBufferSet->GetDynamicParticleCount();
		dynamicParticleHashMapSize = hashesPerDynamicParticle * dynamicParticleCount;		

		dynamicParticleReadHashMapBuffer = Array<std::atomic_uint32_t>(dynamicParticleHashMapSize + 1);
		dynamicParticleWriteHashMapBuffer = Array<std::atomic_uint32_t>(dynamicParticleHashMapSize + 1);
		for (uintMem i = 0; i < dynamicParticleHashMapSize - 1; ++i)
			dynamicParticleReadHashMapBuffer[i].store(0);		
		dynamicParticleReadHashMapBuffer.Last() = dynamicParticleCount;
		for (uintMem i = 0; i < dynamicParticleHashMapSize - 1; ++i)
			dynamicParticleWriteHashMapBuffer[i].store(0);		
		dynamicParticleWriteHashMapBuffer.Last() = dynamicParticleCount;

		particleMap.Resize(dynamicParticleCount);

		CalculateHashAndParticleMapTask task{
			.particleCount = dynamicParticleCount,
			.particleWriteBufferHandle = particleBufferSet->GetWriteBufferHandle(),
			.simulationParameters = simulationParameters,
			.hashMap = dynamicParticleReadHashMapBuffer.Ptr(),
			.particleMap = particleMap.Ptr(),
		};
		threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem threadID, uintMem threadCount) {			
			uintMem begin = task.particleCount * threadID / threadCount;
			uintMem end = task.particleCount * (threadID + 1) / threadCount;
			CalculateHashAndParticleMap(context, begin, end, task);
			});

		particleBufferSet->Advance();
	}
	void SystemCPU::InitializeInternal(const SystemInitParameters& initParams)
	{ 				
		simulationParameters.behaviour = initParams.particleBehaviourParameters;
		simulationParameters.bounds = initParams.particleBoundParameters;
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleHashMapSize;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMapSize;
	}
	void SystemCPU::CalculateHashAndParticleMap(TaskThreadContext& context, uintMem begin, uintMem end, CalculateHashAndParticleMapTask task)
	{
		if (begin == 0)
		{
			task.particleWriteBufferHandle.StartWrite();
		}

		auto particles = task.particleWriteBufferHandle.GetWriteBuffer();

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{
			Vec3i cell = GetCell(particles[i].position, task.simulationParameters.behaviour.maxInteractionDistance);
			uint particleHash = GetHash(cell) % task.simulationParameters.dynamicParticleHashMapSize;

			particles[i].hash = particleHash;
			
			++task.hashMap[particleHash];
		}

		context.SyncThreads();

		if (begin == 0)
		{
#ifdef DEBUG_BUFFERS_CPU			
			Array<uint> hashMap;
			hashMap.Resize(task.simulationParameters.dynamicParticleHashMapSize + 1);
			for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize + 1; ++i)
				hashMap[i] = task.hashMap[i].load();

			DebugPrePrefixSumHashes(
				ArrayView<DynamicParticle>(task.particles, task.simulationParameters.dynamicParticleCount),
				std::move(hashMap)
			);
#endif
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize; ++i)
			{
				valueSum += task.hashMap[i];
				task.hashMap[i] = valueSum;
			}
		}

		context.SyncThreads();		

		for (uintMem i = begin; i < end; ++i)		
			task.particleMap[--task.hashMap[particles[i].hash]] = i;

		context.SyncThreads();

		if (begin == 0)
		{
#ifdef DEBUG_BUFFERS_CPU
			DebugHashAndParticleMap(
				ArrayView<DynamicParticle>(task.particles, task.simulationParameters.dynamicParticleCount),
				ArrayView<std::atomic_uint32_t>(task.hashMap, task.simulationParameters.dynamicParticleHashMapSize + 1),
				ArrayView<uint32>(task.particleMap, task.simulationParameters.dynamicParticleCount)
			);
#endif
			task.particleWriteBufferHandle.FinishWrite();
		}
	}
	void SystemCPU::SimulateParticlesTimeStep(TaskThreadContext& context, uintMem begin, uintMem end, SimulateParticlesTimeStepTask task)
	{		
		{			
			uintMem hashBegin = task.simulationParameters.dynamicParticleHashMapSize * begin / task.simulationParameters.dynamicParticleCount;
			uintMem hashEnd = task.simulationParameters.dynamicParticleHashMapSize * end / task.simulationParameters.dynamicParticleCount;

			for (uintMem i = hashBegin; i < hashEnd; ++i)
				task.dynamicParticleWriteHashMapBuffer[i].store(0);
		}

		if (begin == 0)
		{
			task.particleReadBufferHandle.StartRead();
			task.particleWriteBufferHandle.StartWrite();
		}

		context.SyncThreads();
		
		for (uintMem i = begin; i < end; ++i)
			UpdateParticlePressure(
				i,
				task.particleReadBufferHandle.GetReadBuffer(),
				task.particleWriteBufferHandle.GetWriteBuffer(),
				task.dynamicParticleReadHashMapBuffer,
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				&task.simulationParameters
			);

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			UpdateParticleDynamics(
				i,
				task.particleReadBufferHandle.GetReadBuffer(),
				task.particleWriteBufferHandle.GetWriteBuffer(),
				task.dynamicParticleReadHashMapBuffer,
				task.dynamicParticleWriteHashMapBuffer,
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				task.dt,
				&task.simulationParameters
			);		


		context.SyncThreads();		

		if (begin == 0)
		{			

#ifdef DEBUG_BUFFERS_CPU
			DebugParticles(
				ArrayView<DynamicParticle>(task.particlesWriteBuffer, task.simulationParameters.dynamicParticleCount),
				task.simulationParameters.behaviour.maxInteractionDistance, 
				task.simulationParameters.dynamicParticleHashMapSize
			);

			Array<uint> hashMap;
			hashMap.Resize(task.simulationParameters.dynamicParticleHashMapSize + 1);
			for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize + 1; ++i)
				hashMap[i] = task.dynamicParticleWriteHashMapBuffer[i].load();
			
			DebugPrePrefixSumHashes(
				ArrayView<DynamicParticle>(task.particlesWriteBuffer, task.simulationParameters.dynamicParticleCount),
				std::move(hashMap)
			);
			std::atomic_uint32_t s;			
#endif

			task.particleReadBufferHandle.FinishRead();
					
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize; ++i)
			{
				valueSum += task.dynamicParticleWriteHashMapBuffer[i];
				task.dynamicParticleWriteHashMapBuffer[i] = valueSum;
			}
		}

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			ComputeParticleMap(
				i,
				task.particleWriteBufferHandle.GetWriteBuffer(),
				nullptr,
				task.dynamicParticleWriteHashMapBuffer,
				task.particleMap,
				0
			);

		context.SyncThreads();

		if (begin == 0)
		{
			task.particleWriteBufferHandle.FinishWrite();			

#ifdef DEBUG_BUFFERS_CPU

			DebugHashAndParticleMap(
				ArrayView<DynamicParticle>(task.writeParticleBufferSet.GetDynamicParticleBuffer(), task.simulationParameters.dynamicParticleCount),
				ArrayView<std::atomic_uint32_t>(task.dynamicParticleWriteHashMapBuffer, task.simulationParameters.dynamicParticleHashMapSize + 1),
				ArrayView<uint32>(task.particleMap, task.simulationParameters.dynamicParticleCount)
			);
#endif
		}
	}			
}