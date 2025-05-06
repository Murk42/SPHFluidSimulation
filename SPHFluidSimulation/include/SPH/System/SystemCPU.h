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
		void Initialize(Scene& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager) override;		
		void Update(float dt, uint simulationSteps) override;
				
		StringView SystemImplementationName() override { return "CPU"; };

		float GetSimulationTime() override { return simulationTime; }
	private:									
		ThreadParallelTaskManager threadManager;				
		
		ParticleBufferManager* dynamicParticlesBufferManager;
		ParticleBufferManager* staticParticlesBufferManager;

		bool parallelPartialSum;	
		Array<std::atomic_uint32_t> hashMapGroupSums;
		Array<std::atomic_uint32_t> dynamicParticlesHashMap;		
		Array<std::atomic_uint32_t> staticParticlesHashMap;

		Array<uint32> particleMap;
		
		
		ParticleBehaviourParameters particleBehaviourParameters;		

		float reorderParticlesElapsedTime;
		float reorderParticlesTimeInterval;
				
		float simulationTime;		

		void InitializeStaticParticles(Scene& scene, ParticleBufferManager& staticParticlesBufferManager);
		void InitializeDynamicParticles(Scene& scene, ParticleBufferManager& dynamicParticlesBufferManager);
	};		
}