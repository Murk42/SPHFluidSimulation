#pragma once
#include "SPH/Core/ParticleSimulationEngine.h"
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/ThreadParallelTaskManager.h"
#include "SPH/Core/SceneBlueprint.h"
using namespace Blaze;

namespace SPH
{
	class CPUSimulationEngine : public SimulationEngine
	{
	public:
		CPUSimulationEngine(uintMem threadCount);
		~CPUSimulationEngine();

		void Clear() override;
		void Initialize(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager) override;
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
		Array<Graphics::BasicIndexedMesh::Triangle> triangles;

		ParticleBehaviourParameters particleBehaviourParameters;

		float reorderParticlesElapsedTime;
		float reorderParticlesTimeInterval;

		float simulationTime;

		void InitializeStaticParticles(SceneBlueprint& scene, ParticleBufferManager& staticParticlesBufferManager);
		void InitializeDynamicParticles(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager);
	};
}