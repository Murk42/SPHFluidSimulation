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

		uintMem ThreadCount() const { return threadPool.ThreadCount(); }
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
		const SystemProfilingData& GetProfilingData() override;
		float GetSimulationTime() override { return simulationTime; }
	private:									
		ThreadContext threadContext;				
		
		uintMem dynamicParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleCount;		
		uintMem staticParticleHashMapSize;		 
		
		Array<std::atomic_uint32_t> dynamicParticleHashMapBuffer;		
		Array<uint32> particleMap;

		Array<StaticParticle> staticParticles;
		Array<uint32> staticParticleHashMap;

		CPUParticleBufferSet* particleBufferSet;
		
		ParticleSimulationParameters simulationParameters;		

		float reorderElapsedTime;
		float reorderTimeInterval;
		
		Stopwatch executionStopwatch;
		float simulationTime;
		bool profiling;

		bool parallelPartialSum;

		SystemProfilingData systemProfilingData;

		void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance) override;
		void CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance) override;
		void InitializeInternal(const SystemInitParameters& initParams) override;

		struct CalculateHashAndParticleMapTask
		{
			uintMem particleCount;
			DynamicParticle* outputParticles;
			CPUSync& outputSync;
			ParticleSimulationParameters& simulationParameters;
			std::atomic_uint32_t* hashMap;
			uint32* particleMap;
		};
		struct SimulateParticlesTimeStepTask
		{
			const DynamicParticle* inputParticles;
			CPUSync& inputSync;
			DynamicParticle* outputParticles;
			CPUSync& outputSync;
			DynamicParticle* orderedParticles;
			CPUSync* orderedSync;
						
			std::atomic_uint32_t* dynamicParticleHashMapBuffer;
			uint32* particleMap;

			//this is nullptr when parallelPartialSum is false
			std::atomic_uint32_t* hashMapGroupSums;			

			const StaticParticle* staticParticles;
			uint32* staticParticleHashMap;

			ParticleSimulationParameters& simulationParameters;					

			float dt;
		};
				
		static void CalculateHashAndParticleMap(TaskThreadContext& context, uintMem threadID, uintMem threadCount, CalculateHashAndParticleMapTask task);
		static void SimulateParticlesTimeStep(TaskThreadContext& context, uintMem threadID, uintMem threadCount, SimulateParticlesTimeStepTask task);
	};		
}