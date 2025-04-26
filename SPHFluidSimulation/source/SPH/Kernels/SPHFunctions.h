#pragma once

namespace SPH
{
	struct DynamicParticle;
	struct StaticParticle;
	struct ParticleBehaviourParameters;
}

namespace SPH::Details
{	
	void UpdateParticlePressure(
		uint64 threadID,
		uint64 dynamicParticlesCount,
		uint64 dynamicParticlesHashMapSize,
		uint64 staticParticlesCount,
		uint64 staticParticlesHashMapSize,
		const DynamicParticle* inParticles,
		DynamicParticle* outParticles,
		const std::atomic_uint32_t* hashMap,
		const uint* particleMap,
		const StaticParticle* staticParticles,
		const uint* staticParticleHashMap,
		const ParticleBehaviourParameters* parameters
	);	

	void UpdateParticleDynamics(
		uint64 threadID,
		uint64 dynamicParticlesCount,
		uint64 dynamicParticlesHashMapSize,
		uint64 staticParticlesCount,
		uint64 staticParticlesHashMapSize,
		const DynamicParticle* inParticles,
		DynamicParticle* outParticles,
		const std::atomic_uint32_t* hashMap,
		const uint* particleMap,
		const StaticParticle* staticParticles,
		const uint* staticParticlesHashMap,
		const float deltaTime,
		const ParticleBehaviourParameters* parameters
	);

	void ComputeParticleMap(
		uint64 threadID,
		const DynamicParticle* particles,
		DynamicParticle* orderedParticles,
		std::atomic_uint32_t* hashMap,
		uint* particleMap,
		const uint reorderParticles
	);
}