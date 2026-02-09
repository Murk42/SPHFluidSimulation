#pragma once

namespace SPH
{
	struct DynamicParticle;
	struct StaticParticle;
	struct ParticleBehaviourParameters;
	struct Triangle;
}

namespace SPH::Details
{		
	void PrepareStaticParticlesHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, const StaticParticle* inParticles, float maxInteractionDistance, uint64 particleCount);
	void ReorderStaticParticlesAndFinishHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, const StaticParticle* inParticles, StaticParticle* outParticles, float maxInteractionDistance, uint64 particleCount);
	void ComputeDynamicParticlesHashAndPrepareHashMap(uint64 threadID, volatile std::atomic_uint32_t* hashMap, uintMem hashMapSize, DynamicParticle* particles, float maxInteractionDistance, uint64 particleCount);
	void ReorderDynamicParticlesAndFinishHashMap(uint64 threadID, uint32* particleMap, volatile std::atomic_uint32_t* hashMap, const DynamicParticle* inParticles, DynamicParticle* outParticles, uint64 particleCount);
	void FillDynamicParticleMapAndFinishHashMap(uint64 threadID, uint32* particleMap, volatile std::atomic_uint32_t* hashMap, const DynamicParticle* inParticles, uint64 particleCount);

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
		const ParticleBehaviourParameters* parameters,
		uint64 triangleCount,
		const Triangle* triangles
	);
}