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
		
		dynamicParticleHashMapBuffer.Clear();		
		particleMap.Clear();

		staticParticles.Clear();
		staticParticleHashMap.Clear();		

		simulationParameters = { };

		reorderElapsedTime = 0.0f;
		reorderTimeInterval = 2.0;

		parallelPartialSum = false;

		simulationTime = 0;		
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
			auto& inputParticleBufferHandle = particleBufferSet->GetReadBufferHandle();
			auto& outputParticleBufferHandle = particleBufferSet->GetWriteBufferHandle();
			CPUParticleWriteBufferHandle* orderedParticleBufferHandle = nullptr;
			
			if (reorderElapsedTime > reorderTimeInterval)
			{
				particleBufferSet->Advance();
				orderedParticleBufferHandle = &particleBufferSet->GetWriteBufferHandle();
				reorderElapsedTime -= reorderTimeInterval;
			}

			SimulateParticlesTimeStepTask task{
				.inputParticles = inputParticleBufferHandle.GetReadBuffer(),
				.inputSync = inputParticleBufferHandle.GetReadSync(),
				.outputParticles = outputParticleBufferHandle.GetWriteBuffer(),
				.outputSync = outputParticleBufferHandle.GetWriteSync(),
				.orderedParticles = orderedParticleBufferHandle ? orderedParticleBufferHandle->GetWriteBuffer() : nullptr,
				.orderedSync = orderedParticleBufferHandle ? &orderedParticleBufferHandle->GetWriteSync() : nullptr,
				.dynamicParticleHashMapBuffer = dynamicParticleHashMapBuffer.Ptr(),				
				.particleMap = particleMap.Ptr(),
				.hashMapGroupSums = parallelPartialSum ? new std::atomic_uint32_t[threadContext.ThreadCount()] : nullptr,
				.staticParticles = staticParticles.Ptr(),
				.staticParticleHashMap = staticParticleHashMap.Ptr(),
				.simulationParameters = simulationParameters,				
				.dt = deltaTime,
			};
			threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem threadID, uintMem threadCount) {				
				SimulateParticlesTimeStep(context, threadID, threadCount, task);
				});			
			
			particleBufferSet->Advance();

			reorderElapsedTime += deltaTime;			
		}

		if (profiling)
		{
			threadContext.FinishTasks();

			systemProfilingData.timePerStep_s = executionStopwatch.GetTime() / simulationSteps;
		}

		simulationTime += deltaTime * simulationSteps;
	}		
	void SystemCPU::EnableProfiling(bool enable)
	{
		this->profiling = enable;
	}
	const SystemProfilingData& SystemCPU::GetProfilingData()
	{ 
		return systemProfilingData;
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

		dynamicParticleHashMapBuffer = Array<std::atomic_uint32_t>(dynamicParticleHashMapSize + 1);		
		for (uintMem i = 0; i < dynamicParticleHashMapSize - 1; ++i)
			dynamicParticleHashMapBuffer[i].store(0);		
		dynamicParticleHashMapBuffer.Last() = dynamicParticleCount;		

		particleMap.Resize(dynamicParticleCount);

		auto& outputParticleBufferHandle = particleBufferSet->GetWriteBufferHandle();

		CalculateHashAndParticleMapTask task{
			.particleCount = dynamicParticleCount,
			.outputParticles = outputParticleBufferHandle.GetWriteBuffer(),
			.outputSync = outputParticleBufferHandle.GetWriteSync(),
			.simulationParameters = simulationParameters,
			.hashMap = dynamicParticleHashMapBuffer.Ptr(),
			.particleMap = particleMap.Ptr(),
		};
		threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem threadID, uintMem threadCount) {						
			CalculateHashAndParticleMap(context, threadID, threadCount, task);
			});

		particleBufferSet->Advance();
	}
	void SystemCPU::InitializeInternal(const SystemInitParameters& initParams)
	{ 				
		initParams.ParseImplementationSpecifics((StringView)"CPU",
			SystemInitParameters::ImplementationSpecificsEntry{ "reorderTimeInterval", reorderTimeInterval },
			SystemInitParameters::ImplementationSpecificsEntry{ "parallelPartialSum", parallelPartialSum }
		);

		simulationParameters.behaviour = initParams.particleBehaviourParameters;
		simulationParameters.bounds = initParams.particleBoundParameters;
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleHashMapSize;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMapSize;
	}
	void SystemCPU::CalculateHashAndParticleMap(TaskThreadContext& context, uintMem threadID, uintMem threadCount, CalculateHashAndParticleMapTask task)
	{		
		uintMem begin = task.particleCount * threadID / threadCount;
		uintMem end = task.particleCount * (threadID + 1) / threadCount;

		auto particles = task.outputParticles;

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
				ArrayView<DynamicParticle>(task.outputParticles, task.simulationParameters.dynamicParticleCount),
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
				ArrayView<DynamicParticle>(task.outputParticles, task.simulationParameters.dynamicParticleCount),
				ArrayView<std::atomic_uint32_t>(task.hashMap, task.simulationParameters.dynamicParticleHashMapSize + 1),
				ArrayView<uint32>(task.particleMap, task.simulationParameters.dynamicParticleCount)
			);
#endif
			task.outputSync.MarkEnd();
		}
	}
	void SystemCPU::SimulateParticlesTimeStep(TaskThreadContext& context, uintMem threadID, uintMem threadCount, SimulateParticlesTimeStepTask task)
	{		
		uintMem begin = task.simulationParameters.dynamicParticleCount * threadID / threadCount;
		uintMem end = task.simulationParameters.dynamicParticleCount * (threadID + 1) / threadCount;
		uintMem hashBegin = task.simulationParameters.dynamicParticleHashMapSize * threadID / threadCount;
		uintMem hashEnd = task.simulationParameters.dynamicParticleHashMapSize * (threadID + 1) / threadCount;				
		
		for (uintMem i = begin; i < end; ++i)
			UpdateParticlePressure(
				i,
				task.inputParticles,
				task.outputParticles,
				task.dynamicParticleHashMapBuffer,
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				&task.simulationParameters
			);

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			UpdateParticleDynamics(
				i,
				task.inputParticles,
				task.outputParticles,
				task.dynamicParticleHashMapBuffer,			
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				task.dt,
				&task.simulationParameters
			);				

		context.SyncThreads();				
		
		for (uintMem i = hashBegin; i < hashEnd; ++i)
			task.dynamicParticleHashMapBuffer[i].store(0);

		if (task.hashMapGroupSums != nullptr)
			task.hashMapGroupSums[threadID] = 0;

		context.SyncThreads();
		
		for (uintMem i = begin; i < end; ++i)
			++task.dynamicParticleHashMapBuffer[task.outputParticles[i].hash];

		context.SyncThreads();

		if (threadID == 0)
		{			

			task.inputSync.MarkEnd();


#ifdef DEBUG_BUFFERS_CPU
			DebugParticles(
				ArrayView<DynamicParticle>(task.outputParticles, task.simulationParameters.dynamicParticleCount),
				task.simulationParameters.behaviour.maxInteractionDistance,
				task.simulationParameters.dynamicParticleHashMapSize
			);

			Array<uint> hashMap;
			hashMap.Resize(task.simulationParameters.dynamicParticleHashMapSize + 1);
			for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize + 1; ++i)
				hashMap[i] = task.dynamicParticleWriteHashMapBuffer[i].load();

			DebugPrePrefixSumHashes(
				ArrayView<DynamicParticle>(task.outputParticles, task.simulationParameters.dynamicParticleCount),
				std::move(hashMap)
			);
		}

		context.SyncThreads();

		if (threadID == 0)
		{
#endif
			
			if (task.hashMapGroupSums == nullptr)
			{
				uint32 valueSum = 0;
				for (uintMem i = 0; i < task.simulationParameters.dynamicParticleHashMapSize; ++i)
				{
					valueSum += task.dynamicParticleHashMapBuffer[i];
					task.dynamicParticleHashMapBuffer[i] = valueSum;
				}
			}
		}
				
		if (task.hashMapGroupSums != nullptr)
		{
			uint32 valueSum = 0;
			for (uintMem i = hashBegin; i < hashEnd; ++i)
			{
				valueSum += task.dynamicParticleHashMapBuffer[i];
				task.dynamicParticleHashMapBuffer[i] = valueSum;
			}
			
			for (uintMem i = threadID; i < threadCount - 1; ++i)
				task.hashMapGroupSums[i] += valueSum;
			
			context.SyncThreads();
			
			if (threadID != 0)
			{
				uintMem addition = task.hashMapGroupSums[threadID - 1];
				for (uintMem i = hashBegin; i < hashEnd; ++i)
					task.dynamicParticleHashMapBuffer[i] += addition;
			}
		}

		context.SyncThreads();		
		
		for (uintMem i = begin; i < end; ++i)
			ComputeParticleMap(
				i,
				task.outputParticles,
				task.orderedParticles,
				task.dynamicParticleHashMapBuffer,
				task.particleMap,
				task.orderedParticles != nullptr
			);

		context.SyncThreads();

		if (begin == 0)
		{			
			if (task.hashMapGroupSums != nullptr)
				delete[] task.hashMapGroupSums;

			task.outputSync.MarkEnd();			

			if (task.orderedParticles != nullptr)
				task.orderedSync->MarkEnd();

#ifdef DEBUG_BUFFERS_CPU

			DebugHashAndParticleMap(
				ArrayView<DynamicParticle>(task.orderedParticles ? task.orderedParticles : task.outputParticles, task.simulationParameters.dynamicParticleCount),
				ArrayView<std::atomic_uint32_t>(task.dynamicParticleWriteHashMapBuffer, task.simulationParameters.dynamicParticleHashMapSize + 1),
				ArrayView<uint32>(task.particleMap, task.simulationParameters.dynamicParticleCount)
			);
#endif
		}
	}			
}