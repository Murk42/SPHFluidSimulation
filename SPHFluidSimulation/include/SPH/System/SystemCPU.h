#pragma once
#include "SPH/Core/System.h"
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/ThreadParallelTaskManager.h"
using namespace Blaze;

namespace SPH
{					
	class SystemCPU : public System
	{
	public:				
		SystemCPU(uintMem threadCount);
		~SystemCPU();
		
		void Clear() override;
		void Initialize(const SystemParameters& parameters, ParticleBufferManager& particleBufferManager, Array<DynamicParticle> dynamicParticles, Array<StaticParticle> staticParticles) override;
		void Update(float dt, uint simulationSteps) override;
				
		StringView SystemImplementationName() override { return "CPU"; };

		float GetSimulationTime() override { return simulationTime; }
	private:									
		ThreadParallelTaskManager threadManager;				
		
		ParticleBufferManager* particleBufferManager;

		bool parallelPartialSum;	
		Array<std::atomic_uint32_t> hashMapGroupSums;
		Array<std::atomic_uint32_t> dynamicParticlesHashMapBuffer;		
		Array<uint32> particleMap;
		
		Array<uint32> staticParticlesHashMapBuffer;
		
		ParticleBehaviourParameters particleBehaviourParameters;		

		float reorderParticlesElapsedTime;
		float reorderParticlesTimeInterval;
				
		float simulationTime;		

		void WaitForTasksToFinish();
	};		
}