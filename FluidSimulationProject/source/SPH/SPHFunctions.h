#pragma once

#include "../assets/kernels/CL_CPP_SPHDeclarations.h"

Vec3i GetCell(Vec3f position, float maxInteractionDistance);
uint GetHash(Vec3i cell);
float SmoothingKernelConstant(float h);
float SmoothingKernelD0(float r, float maxInteractionDistance);
float SmoothingKernelD1(float r, float maxInteractionDistance);
float SmoothingKernelD2(float r, float maxInteractionDistance);
float Noise(float x);
Vec3f RandomDirection(float x);

void UpdateParticlePressure(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* inParticles,
	GLOBAL STRUCT DynamicParticle* outParticles,
	GLOBAL const HASH_TYPE* hashMap,
	GLOBAL const uint* particleMap,
	GLOBAL STRUCT StaticParticle* staticParticles,
	GLOBAL const uint* staticParticleHashMap,
	GLOBAL STRUCT ParticleSimulationParameters* parameters
);

void UpdateParticleDynamics(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* inParticles,
	GLOBAL STRUCT DynamicParticle* outParticles,
	GLOBAL const HASH_TYPE* hashMap,
	GLOBAL HASH_TYPE* newHashMap,
	GLOBAL const uint* particleMap,
	GLOBAL const STRUCT StaticParticle* staticParticles,
	GLOBAL const uint* staticParticlesHashMap,
	const float deltaTime,
	GLOBAL STRUCT ParticleSimulationParameters* parameters
);

void ComputeParticleMap(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* particles,
	GLOBAL STRUCT DynamicParticle* orderedParticles,
	GLOBAL HASH_TYPE* hashMap,
	GLOBAL uint* particleMap,
	const uint reorderParticles
);