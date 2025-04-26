#ifndef CL_COMPILER
#include "CompatibilityHeaderC++.h"
#include "SPHFunctions.h"

namespace SPH
{
#endif


	struct PACKED DynamicParticle
	{
		Vec4f positionAndPressure;
		Vec4f velocityAndHash;
	};

	struct PACKED StaticParticle
	{
		Vec4f positionAndPressure;
	};

	struct PACKED ParticleBehaviourParameters
	{
		//Particle dynamics constants
		float particleMass;
		float gasConstant;
		float elasticity;
		float viscosity;
		float gravityX;
		float gravityY;
		float gravityZ;

		//Particle simulation parameters		
		float restDensity;
		float maxInteractionDistance;

		//Constants
		float selfDensity;
		float smoothingKernelConstant;
	};

#ifndef CL_COMPILER
}

namespace SPH::Details
{
#endif

//#define DEBUG_BUFFERS_KERNEL

	Vec3u Floor(Vec3f value)
	{
#ifdef CL_COMPILER
		return as_uint3(convert_int3_rtn(value));
#else
		return Vec3u(Vec3i(std::floorf(value.x), std::floorf(value.y), std::floorf(value.z)));
#endif
	}

	//Expects value in range [0, 1024)
	uint32 ExpandBits(uint32 v)
	{
		v = (v * 0x00010001u) & 0xFF0000FFu;
		v = (v * 0x00000101u) & 0x0F00F00Fu;
		v = (v * 0x00000011u) & 0xC30C30C3u;
		v = (v * 0x00000005u) & 0x49249249u;
		return v;	
	}

	uint32 Morton3D(Vec3u value)
	{
		//center so that 0, 0, 0 is in the center of the morton cube
		//value = value + NEW_VEC3I(512, 512, 512);

		uint32 xx = ExpandBits(value.x & 0x000003FFu);
		uint32 yy = ExpandBits(value.y & 0x000003FFu);
		uint32 zz = ExpandBits(value.z & 0x000003FFu);
		return xx + yy * 2 + zz * 4;
	}

	inline Vec3u GetCell(Vec3f position, float maxInteractionDistance)
	{		
		return Floor(position / maxInteractionDistance);
	}	
	inline uint32 GetHash(Vec3u cell)
	{
		//Morton hash
		return Morton3D(cell);

		//Random hash
		//return (
		//	(((uint32)cell.x) * 73856093) ^
		//	(((uint32)cell.y) * 19349663) ^
		//	(((uint32)cell.z) * 83492791)
		//	);
	}
	float SmoothingKernelConstant(float h)
	{
		return 15.0f / (3.1415f * h * h * h * h * h * h);
	}
	inline float SmoothingKernelD0(float r, float h)
	{
		if (r >= h)
			return 0;

		return (h - r) * (h - r) * (h - r);
	}
	inline float SmoothingKernelD1(float r, float h)
	{
		if (r >= h)
			return 0;

		return -3 * (h - r) * (h - r);
	}
	inline float SmoothingKernelD2(float r, float h)
	{
		if (r >= h)
			return 0;

		return 6 * (h - r);
	}
	inline float Noise(float x)
	{
		float ptr = 0.0f;
		return modf(sin(x * 112.9898f) * 43758.5453f, &ptr);
	}
	inline Vec3f RandomDirection(float x)
	{
		//https://math.stackexchange.com/questions/44689/how-to-find-a-random-axis-or-unit-vector-in-3d
		float theta = Noise(x) * 2 * 3.1415f;
		float z = Noise(x) * 2 - 1;

		float s = sin(theta);
		float c = cos(theta);
		float z2 = sqrt(1 - z * z);
		return NEW_VEC3F(z2 * c, z2 * s, z);
	}

#ifdef CL_COMPILER
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

	void kernel computeParticleHashes(global struct DynamicParticle* particles, volatile global uint32* hashMap, float maxInteractionDistance, uint64 hashMapSize)
	{
		float3 particlePosition = particles[get_global_id(0)].positionAndPressure.xyz;

		uint3 cell = GetCell(particlePosition, maxInteractionDistance);
		uint32 particleHash = GetHash(cell) % hashMapSize;

		particles[get_global_id(0)].velocityAndHash.w = *(float*)&particleHash;

		atomic_inc(hashMap + particleHash);
	}

	void kernel incrementHashMap(global struct DynamicParticle* particles, volatile global uint32* hashMap)
	{
		float particleHash_FLOAT = particles[get_global_id(0)].velocityAndHash.w;
		uint32 particleHash = *(uint32*)&particleHash_FLOAT;

		atomic_inc(hashMap + particleHash);
	}
#endif

	void KERNEL ComputeParticleMap(
		uint64 threadID,
		CONSTANT STRUCT DynamicParticle* particles,
		GLOBAL STRUCT DynamicParticle* orderedParticles,
		GLOBAL HASH_TYPE* hashMap,
		GLOBAL uint32* particleMap,
		const uint32 reorderParticles
	) {
#ifdef CL_COMPILER
		threadID = get_global_id(0);
#endif

		float particleHash_FLOAT = particles[threadID].velocityAndHash.w;
		uint32 particleHash = *(uint32*)&particleHash_FLOAT;

		uint32 index = atomic_dec(hashMap + particleHash) - 1;

		if (reorderParticles == 1)
		{
			orderedParticles[index] = particles[threadID];
			particleMap[index] = index;
		}
		else
			particleMap[index] = threadID;
	}

	void KERNEL UpdateParticlePressure(
		uint64 threadID,
		uint64 dynamicParticlesCount,
		uint64 dynamicParticlesHashMapSize,
		uint64 staticParticlesCount,
		uint64 staticParticlesHashMapSize,
		CONSTANT STRUCT DynamicParticle* inParticles,
		GLOBAL STRUCT DynamicParticle* outParticles,
		CONSTANT HASH_TYPE* hashMap,
		CONSTANT uint32* particleMap,
		CONSTANT STRUCT StaticParticle* staticParticles,
		CONSTANT uint32* staticParticlesHashMap,
		CONSTANT STRUCT ParticleBehaviourParameters* parameters
	) {
#ifdef CL_COMPILER
		threadID = get_global_id(0);
#endif

		CONSTANT STRUCT DynamicParticle* inParticlePtr = inParticles + threadID;
		GLOBAL STRUCT DynamicParticle* outParticlePtr = outParticles + threadID;

#ifdef DEBUG_BUFFERS_KERNEL
		if (threadID >= dynamicParticlesCount)
		{
			printf("Allocating more work items than particles");
			return;
		}
#endif		

		Vec3f particlePosition = inParticlePtr->positionAndPressure.xyz();

		Vec3u cell = GetCell(particlePosition, parameters->maxInteractionDistance);

		Vec3u beginCell = cell - NEW_VEC3U(1, 1, 1);
		Vec3u endCell = cell + NEW_VEC3U(2, 2, 2);

		float dynamicParticleInfluenceSum = 0;
		float staticParticleInfluenceSum = 0;

		Vec3u otherCell;
		for (otherCell.x = beginCell.x; otherCell.x != endCell.x; ++otherCell.x)
			for (otherCell.y = beginCell.y; otherCell.y != endCell.y; ++otherCell.y)
				for (otherCell.z = beginCell.z; otherCell.z != endCell.z; ++otherCell.z)
				{
					uint32 otherHash = GetHash(otherCell);

					//Calculating dynamic particle pressure
					uint32 otherHashMod = otherHash % dynamicParticlesHashMapSize;
					uint32 beginIndex = hashMap[otherHashMod];
					uint32 endIndex = hashMap[otherHashMod + 1];

#ifdef DEBUG_BUFFERS_KERNEL				
					if (beginIndex > endIndex)
					{
						printf("Dynamic particle map begin index is bigger than end index. Begin: %u End: %u", beginIndex, endIndex);
						break;
					}
					if (beginIndex > dynamicParticlesCount)
					{
						printf("Invalid dynamic particle map begin index: %u", beginIndex);
						break;
					}
					if (endIndex > dynamicParticlesCount)
					{
						printf("Invalid dynamic particle map end index: %u", endIndex);
						break;
					}
#endif								

					for (uint32 i = beginIndex; i < endIndex; ++i)
					{
						uint32 index = particleMap[i];

#ifdef DEBUG_BUFFERS_KERNEL
						if (index >= dynamicParticlesCount)
						{
							printf("DynamicParticle map value outside valid range");
							continue;
						}
#endif


						CONSTANT STRUCT DynamicParticle* otherParticlePtr = inParticles + index;

						if (inParticlePtr == otherParticlePtr)
							continue;

						Vec3f dir = otherParticlePtr->positionAndPressure.xyz() - particlePosition;
						float distSqr = dot(dir, dir);

						if (distSqr > parameters->maxInteractionDistance * parameters->maxInteractionDistance)
							continue;

						float dist = sqrt(distSqr);

						dynamicParticleInfluenceSum += SmoothingKernelD0(dist, parameters->maxInteractionDistance);
					}

					if (staticParticlesCount == 0)
						continue;

					//Calculating static particle pressure
					otherHashMod = otherHash % staticParticlesHashMapSize;
					beginIndex = staticParticlesHashMap[otherHashMod];
					endIndex = staticParticlesHashMap[otherHashMod + 1];

#ifdef DEBUG_BUFFERS_KERNEL				
					if (beginIndex > endIndex)
					{
						printf("Static begin index is bigger than end index for static particles. Begin: %u End: %u", beginIndex, endIndex);
						break;
					}
					if (beginIndex > staticParticlesCount)
					{
						printf("Invalid static begin index: %u", beginIndex);
						break;
					}
					if (endIndex > staticParticlesCount)
					{
						printf("Invalid static end index: %u", endIndex);
						break;
					}
#endif

					for (uint32 i = beginIndex; i < endIndex; ++i)
					{
						CONSTANT STRUCT StaticParticle* otherParticlePtr = staticParticles + i;

						Vec3f dir = otherParticlePtr->positionAndPressure.xyz() - particlePosition;
						float distSqr = dot(dir, dir);

						if (distSqr > parameters->maxInteractionDistance * parameters->maxInteractionDistance)
							continue;

						float dist = sqrt(distSqr);

						staticParticleInfluenceSum += SmoothingKernelD0(dist, parameters->maxInteractionDistance);
					}
				}

		float particleDensity = parameters->selfDensity + (dynamicParticleInfluenceSum * parameters->particleMass + staticParticleInfluenceSum * parameters->particleMass) * parameters->smoothingKernelConstant;
		float particlePressure = parameters->gasConstant * (particleDensity - parameters->restDensity);

		outParticlePtr->positionAndPressure.w = particlePressure;
	}

	void KERNEL UpdateParticleDynamics(
		uint64 threadID,
		uint64 dynamicParticlesCount,
		uint64 dynamicParticlesHashMapSize,
		uint64 staticParticlesCount,
		uint64 staticParticlesHashMapSize,
		CONSTANT STRUCT DynamicParticle* inParticles,
		GLOBAL STRUCT DynamicParticle* outParticles,
		CONSTANT HASH_TYPE* hashMap,
		CONSTANT uint32* particleMap,
		CONSTANT STRUCT StaticParticle* staticParticles,
		CONSTANT uint32* staticParticlesHashMap,
		const float deltaTime,
		CONSTANT STRUCT ParticleBehaviourParameters* parameters
	) {
#ifdef CL_COMPILER
		threadID = get_global_id(0);
#endif

		uint64 inParticleIndex = threadID;
		CONSTANT STRUCT DynamicParticle* inParticlePtr = inParticles + inParticleIndex;
		uint64 outParticleIndex = threadID;
		GLOBAL STRUCT DynamicParticle* outParticlePtr = outParticles + outParticleIndex;

		Vec3f particlePosition = inParticlePtr->positionAndPressure.xyz();
		float particlePressure = outParticlePtr->positionAndPressure.w;
		Vec3f particleVelocity = inParticlePtr->velocityAndHash.xyz();
		float particleHash_FLOAT = inParticlePtr->velocityAndHash.w;
		uint32 particleHash = *(uint32*)&particleHash_FLOAT;

		Vec3u cell = GetCell(particlePosition, parameters->maxInteractionDistance);

		Vec3u beginCell = cell - NEW_VEC3U(1, 1, 1);
		Vec3u endCell = cell + NEW_VEC3U(2, 2, 2);

		Vec3f dynamicParticlePressureForce = NEW_VEC3F(0.0f, 0.0f, 0.0f);
		Vec3f dynamicParticleViscosityForce = NEW_VEC3F(0.0f, 0.0f, 0.0f);
		Vec3f staticParticlePressureForce = NEW_VEC3F(0.0f, 0.0f, 0.0f);
		Vec3f staticParticleViscosityForce = NEW_VEC3F(0.0f, 0.0f, 0.0f);

		float particleDensity = particlePressure / parameters->gasConstant + parameters->restDensity;

		Vec3u otherCell;
		for (otherCell.x = beginCell.x; otherCell.x != endCell.x; ++otherCell.x)
			for (otherCell.y = beginCell.y; otherCell.y != endCell.y; ++otherCell.y)
				for (otherCell.z = beginCell.z; otherCell.z != endCell.z; ++otherCell.z)
				{
					uint32 otherHash = GetHash(otherCell);

					uint32 otherHashMod = otherHash % dynamicParticlesHashMapSize;
					uint32 beginIndex = hashMap[otherHashMod];
					uint32 endIndex = hashMap[otherHashMod + 1];

					for (uint32 i = beginIndex; i < endIndex; ++i)
					{
						uint32 index = particleMap[i];

						if (index == inParticleIndex)
							continue;

						float otherParticlePressure = outParticles[index].positionAndPressure.w;
						Vec3f otherParticlePosition = inParticles[index].positionAndPressure.xyz();
						Vec3f otherParticleVelocity = inParticles[index].velocityAndHash.xyz();

						Vec3f dir = otherParticlePosition - particlePosition;
						float distSqr = dot(dir, dir);

						if (distSqr > parameters->maxInteractionDistance * parameters->maxInteractionDistance)
							continue;

						float dist = sqrt(distSqr);

						if (distSqr == 0 || dist == 0)
						{
							dir = RandomDirection(threadID);
							printf("Two dynamic particles have the same position. Simulation wont be deterministic. First position: % 3.3v3f; second position: % 3.3v3f; i1: %u; i2: %u", particlePosition, otherParticlePosition, inParticleIndex, index);
						}
						else
							dir /= dist;

						//apply pressure force					
						dynamicParticlePressureForce += dir * (particlePressure + otherParticlePressure) * SmoothingKernelD1(dist, parameters->maxInteractionDistance);

						//apply viscosity force					
						dynamicParticleViscosityForce += (otherParticleVelocity - particleVelocity) * SmoothingKernelD2(dist, parameters->maxInteractionDistance);
					}

					if (staticParticlesCount == 0)
						continue;

					otherHashMod = otherHash % staticParticlesHashMapSize;
					beginIndex = staticParticlesHashMap[otherHashMod];
					endIndex = staticParticlesHashMap[otherHashMod + 1];

					for (uint32 i = beginIndex; i < endIndex; ++i)
					{
						STRUCT StaticParticle otherParticle = staticParticles[i];

						Vec3f dir = otherParticle.positionAndPressure.xyz() - particlePosition;
						float distSqr = dot(dir, dir);

						if (distSqr > parameters->maxInteractionDistance * parameters->maxInteractionDistance)
							continue;

						float dist = sqrt(distSqr);

						if (distSqr == 0 || dist == 0)
						{
							dir = RandomDirection(threadID);
							printf("A dynamic particle and a static particle have the same position. Simulation wont be deterministic. First position: % 3.3v3f; second position: % 3.3v3f", particlePosition, otherParticle.positionAndPressure.xyz());
						}
						else
							dir /= dist;

						//apply pressure force					
						staticParticlePressureForce += dir * fabs(particlePressure) * SmoothingKernelD1(dist, parameters->maxInteractionDistance);

						//apply viscosity force					
						staticParticleViscosityForce += -particleVelocity * SmoothingKernelD2(dist, parameters->maxInteractionDistance);
					}
				}

		dynamicParticlePressureForce *= parameters->particleMass / (2 * particleDensity) * parameters->smoothingKernelConstant;
		dynamicParticleViscosityForce *= parameters->viscosity * parameters->particleMass * parameters->smoothingKernelConstant;
		staticParticlePressureForce *= parameters->particleMass / (2 * particleDensity) * parameters->smoothingKernelConstant * 1.0f;
		staticParticleViscosityForce *= parameters->viscosity * parameters->particleMass * parameters->smoothingKernelConstant * 0.0f;

		Vec3f particleForce = dynamicParticlePressureForce + dynamicParticleViscosityForce + staticParticlePressureForce + staticParticleViscosityForce;
		Vec3f acceleration = particleForce / particleDensity + NEW_VEC3F(parameters->gravityX, parameters->gravityY, parameters->gravityZ);

		//Integrate
		particleVelocity += acceleration * deltaTime;
		particlePosition += particleVelocity * deltaTime;

		cell = GetCell(particlePosition, parameters->maxInteractionDistance);
		particleHash = GetHash(cell) % dynamicParticlesHashMapSize;

#ifdef CL_COMPILER
		outParticlePtr->positionAndPressure.xyz = particlePosition;
		outParticlePtr->velocityAndHash.xyz = particleVelocity;
		outParticlePtr->velocityAndHash.w = as_float(particleHash);
#else
		outParticlePtr->positionAndPressure = Vec4f(particlePosition, particlePressure);
		outParticlePtr->velocityAndHash = Vec4f(particleVelocity, *(float*)&particleHash);
#endif

	}

#ifndef CL_COMPILER
}
#endif
