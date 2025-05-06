#pragma once

namespace SPH
{
	struct DynamicParticle;
	struct StaticParticle;
	struct ParticleBehaviourParameters;
}

namespace SPH::Details
{	
	void InitializeParticleMap(uint64 threadID, uint32* particleMap);
	void PrepareStaticParticlesHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, const StaticParticle* inParticles, float maxInteractionDistance);
	void ReorderStaticParticlesAndFinishHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, const StaticParticle* inParticles, StaticParticle* outParticles, float maxInteractionDistance);
	void ComputeDynamicParticlesHashAndPrepareHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, DynamicParticle* particles, float maxInteractionDistance);
	void ReorderDynamicParticlesAndFinishHashMap(uint64 threadID, uint32* particleMap, volatile std::atomic_uint32_t* hashMap, const DynamicParticle* inParticles, DynamicParticle* outParticles);
	void FillDynamicParticleMapAndFinishHashMap(uint64 threadID, uint32* particleMap, volatile std::atomic_uint32_t* hashMap, const DynamicParticle* inParticles);

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
		const std::atomic_uint32_t* staticParticleHashMap,
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
		const std::atomic_uint32_t* staticParticlesHashMap,
		const float deltaTime,
		const ParticleBehaviourParameters* parameters
	);
}