#pragma once

#include "SPH/System/System.h"
#include "SPH/ParticleBufferSet/CPUParticleBufferSet.h"
#include "ThreadPool.h"
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
using namespace Blaze;

//#define DEBUG_BUFFERS_CPU

namespace SPH
{			
	class ThreadContext;

	class TaskThreadContext
	{
	public:
		void SyncThreads();
	private:
		TaskThreadContext(ThreadContext& context);
		ThreadContext& context;

		friend class ThreadContext;
	};

	class ThreadContext
	{
	public:
		using TaskFunction = std::function<void(TaskThreadContext&, uintMem, uintMem)>;
		ThreadContext(ThreadPool& threadPool);
		~ThreadContext();

		void FinishTasks();		
		void EnqueueTask(TaskFunction function);
	private:
		TaskThreadContext taskThreadContext;
		ThreadPool& threadPool;
		std::condition_variable idleCV;
		std::condition_variable syncCV;
		std::condition_variable taskTakenCV;
		std::mutex mutex;
		std::mutex syncMutex;
		uintMem threadIdleCount;
		uintMem threadSyncCount1;
		uintMem threadSyncCount2;		
		
		bool exit;		
		std::queue<TaskFunction> tasks;
		
		void SimulationThreadFunc(TaskThreadContext&, uintMem begin, uintMem end);

		friend class TaskThreadContext;
	};

	class SystemCPU : public System
	{
	public:				
		SystemCPU(ThreadPool& threadPool);
		~SystemCPU();
		
		void Clear() override;
		
		void Update(float dt, uint simulationSteps) override;
		
		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "CPU"; };

		void EnableProfiling(bool enable) override;
		SystemProfilingData GetProfilingData() override;
		float GetSimulationTime() override { return simulationTime; }
	private:									
		ThreadContext threadContext;				
		
		uintMem dynamicParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleCount;		
		uintMem staticParticleHashMapSize;		 
		
		Array<std::atomic_uint32_t> dynamicParticleReadHashMapBuffer;
		Array<std::atomic_uint32_t> dynamicParticleWriteHashMapBuffer;
		Array<uint32> particleMap;
		Array<StaticParticle> staticParticles;
		Array<uint32> staticParticleHashMap;

		CPUParticleBufferSet* particleBufferSet;
		
		ParticleSimulationParameters simulationParameters;		
		
		Stopwatch executionStopwatch;
		float simulationTime;
		double lastTimePerStep_s;
		bool profiling;

		void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance) override;
		void CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance) override;
		void InitializeInternal(const SystemInitParameters& initParams) override;

		struct CalculateHashAndParticleMapTask
		{
			uintMem particleCount;
			CPUParticleWriteBufferHandle& particleWriteBufferHandle;			
			ParticleSimulationParameters& simulationParameters;
			std::atomic_uint32_t* hashMap;
			uint32* particleMap;
		};
		struct SimulateParticlesTimeStepTask
		{
			CPUParticleReadBufferHandle& particleReadBufferHandle;
			CPUParticleWriteBufferHandle& particleWriteBufferHandle;
			
			std::atomic_uint32_t* dynamicParticleReadHashMapBuffer;
			std::atomic_uint32_t* dynamicParticleWriteHashMapBuffer;
			uint32* particleMap;
			StaticParticle* staticParticles;
			uint32* staticParticleHashMap;
			ParticleSimulationParameters& simulationParameters;		
			bool reorderParticles;

			float dt;
		};
				
		static void CalculateHashAndParticleMap(TaskThreadContext& context, uintMem begin, uintMem end, CalculateHashAndParticleMapTask task);
		static void SimulateParticlesTimeStep(TaskThreadContext& context, uintMem begin, uintMem end, SimulateParticlesTimeStepTask task);
	};		
}