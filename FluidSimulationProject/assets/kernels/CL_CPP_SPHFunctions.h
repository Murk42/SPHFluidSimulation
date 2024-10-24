
//#define DEBUG_BUFFERS_KERNEL

Vec3i GetCell(Vec3f position, float maxInteractionDistance)
{
    position = floor(position / maxInteractionDistance);
    return CONVERT_VEC3I(position);
}

//Expects value in range [0, 1024)
unsigned int ExpandBits(uint v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}

uint Morton3D(Vec3i value)
{
	//center so that 0, 0, 0 is in the center of the morton cube
	value = value + NEW_VEC3I(512, 512, 512);

	uint xx = ExpandBits(value.x & 0b1111111111);
	uint yy = ExpandBits(value.y & 0b1111111111);
	uint zz = ExpandBits(value.z & 0b1111111111);
	return xx * 4 + yy * 2 + zz;
}

//Random hash
//uint GetHash(Vec3i cell)
//{
//    return (
//        (((uint)cell.x) * 73856093) ^
//        (((uint)cell.y) * 19349663) ^
//        (((uint)cell.z) * 83492791)
//        );
//}

//Morton hash
uint GetHash(Vec3i cell)
{
	return Morton3D(cell);	
}

float SmoothingKernelConstant(float h)
{
    return 15.0f / (3.1415f * h * h * h * h * h * h);
}
float SmoothingKernelD0(float r, float h)
{
    if (r >= h)
        return 0;
    
    return (h - r) * (h - r) * (h - r);
}
float SmoothingKernelD1(float r, float h)
{
    if (r >= h)
        return 0;

    return -3 * (h - r) * (h - r);
}
float SmoothingKernelD2(float r, float h)
{
    if (r >= h)
        return 0;

    return 6 * (h - r);
}

float Noise(float x)
{    
    float ptr = 0.0f;
    return modf(sin(x * 112.9898f) * 43758.5453f, &ptr);
}

Vec3f RandomDirection(float x)
{
    //https://math.stackexchange.com/questions/44689/how-to-find-a-random-axis-or-unit-vector-in-3d
    float theta = Noise(x) * 2 * 3.1415;
    float z = Noise(x) * 2 - 1;

    float s = sin(theta);
    float c = cos(theta);
    float z2 = sqrt(1 - z * z);
    return NEW_VEC3F(z2 * c, z2 * s, z);
}

void ComputeParticleMap(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* particles,
	GLOBAL STRUCT DynamicParticle* orderedParticles,
	GLOBAL HASH_TYPE* hashMap,
	GLOBAL uint* particleMap,
	const uint reorderParticles
) {
	float particleHash_FLOAT = particles[threadID].velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	uint index = atomic_dec(hashMap + particleHash) - 1;

	if (reorderParticles == 1)
	{
		orderedParticles[index] = particles[threadID];
		particleMap[index] = index;
	}
	else
		particleMap[index] = threadID;
}

void UpdateParticlePressure(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* inParticles,
	GLOBAL STRUCT DynamicParticle* outParticles,
	GLOBAL const HASH_TYPE* hashMap,
	GLOBAL const uint* particleMap,
	GLOBAL STRUCT StaticParticle* staticParticles,
	GLOBAL const uint* staticParticlesHashMap,
	GLOBAL STRUCT ParticleSimulationParameters* parameters
) {
	GLOBAL const STRUCT DynamicParticle* inParticlePtr = inParticles + threadID;
	GLOBAL STRUCT DynamicParticle* outParticlePtr = outParticles + threadID;

#ifdef DEBUG_BUFFERS_KERNEL
	if (threadID >= parameters->dynamicParticleCount)
	{
		printf("Allocating more work items than particles");
		return;
	}
#endif

	Vec3f particlePosition = inParticlePtr->positionAndPressure.xyz();

	Vec3i cell = GetCell(particlePosition, parameters->behaviour.maxInteractionDistance);

	Vec3i beginCell = cell - NEW_VEC3I(1, 1, 1);
	Vec3i endCell = cell + NEW_VEC3I(2, 2, 2);

	float dynamicParticleInfluenceSum = 0;
	float staticParticleInfluenceSum = 0;

	Vec3i otherCell;
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = GetHash(otherCell);

				//Calculating dynamic particle pressure
				uint otherHashMod = otherHash % parameters->dynamicParticleHashMapSize;
				uint beginIndex = hashMap[otherHashMod];
				uint endIndex = hashMap[otherHashMod + 1];

#ifdef DEBUG_BUFFERS_KERNEL				
				if (beginIndex > endIndex)
				{
					printf("Dynamic particle map begin index is bigger than end index. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > parameters->dynamicParticleCount)
				{
					printf("Invalid dynamic particle map begin index: %u", beginIndex);
					break;
				}
				if (endIndex > parameters->dynamicParticleCount)
				{
					printf("Invalid dynamic particle map end index: %u", endIndex);
					break;
				}
#endif								

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					uint index = particleMap[i];

#ifdef DEBUG_BUFFERS_KERNEL
					if (index >= parameters->dynamicParticleCount)
					{
						printf("DynamicParticle map value outside valid range");
						continue;
					}
#endif


					GLOBAL const STRUCT DynamicParticle* otherParticlePtr = inParticles + index;

					if (inParticlePtr == otherParticlePtr)
						continue;

					Vec3f dir = otherParticlePtr->positionAndPressure.xyz() - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > parameters->behaviour.maxInteractionDistance * parameters->behaviour.maxInteractionDistance)
						continue;

					float dist = sqrt(distSqr);

					dynamicParticleInfluenceSum += SmoothingKernelD0(dist, parameters->behaviour.maxInteractionDistance);
				}

				//Calculating static particle pressure
				otherHashMod = otherHash % parameters->staticParticleHashMapSize;
				beginIndex = staticParticlesHashMap[otherHashMod];
				endIndex = staticParticlesHashMap[otherHashMod + 1];

#ifdef DEBUG_BUFFERS_KERNEL				
				if (beginIndex > endIndex)
				{
					printf("Static begin index is bigger than end index for static particles. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > parameters->staticParticleCount)
				{
					printf("Invalid static begin index: %u", beginIndex);
					break;
				}
				if (endIndex > parameters->staticParticleCount)
				{
					printf("Invalid static end index: %u", endIndex);
					break;
				}
#endif

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					GLOBAL const STRUCT StaticParticle* otherParticlePtr = staticParticles + i;

					Vec3f dir = otherParticlePtr->positionAndPressure.xyz() - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > parameters->behaviour.maxInteractionDistance * parameters->behaviour.maxInteractionDistance)
						continue;

					float dist = sqrt(distSqr);					

					staticParticleInfluenceSum += SmoothingKernelD0(dist, parameters->behaviour.maxInteractionDistance);
				}	
			}					

	float particleDensity = parameters->selfDensity + (dynamicParticleInfluenceSum * parameters->behaviour.particleMass + staticParticleInfluenceSum * parameters->behaviour.particleMass) * parameters->smoothingKernelConstant;
	float particlePressure = parameters->behaviour.gasConstant * (particleDensity - parameters->behaviour.restDensity);

	outParticlePtr->positionAndPressure.w = particlePressure;
}

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
) {
	uint inParticleIndex = threadID;
	GLOBAL const STRUCT DynamicParticle* inParticlePtr = inParticles + inParticleIndex;
	uint outParticleIndex = threadID;
	GLOBAL STRUCT DynamicParticle* outParticlePtr = outParticles + outParticleIndex;

	Vec3f particlePosition = inParticlePtr->positionAndPressure.xyz();
	float particlePressure = outParticlePtr->positionAndPressure.w;
	Vec3f particleVelocity = inParticlePtr->velocityAndHash.xyz();
	float particleHash_FLOAT = inParticlePtr->velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	Vec3i cell = GetCell(particlePosition, parameters->behaviour.maxInteractionDistance);

	Vec3i beginCell = cell - (Vec3i)(1, 1, 1);
	Vec3i endCell = cell + (Vec3i)(2, 2, 2);

	Vec3f dynamicParticlePressureForce = (Vec3f)(0, 0, 0);
	Vec3f dynamicParticleViscosityForce = (Vec3f)(0, 0, 0);
	Vec3f staticParticlePressureForce = (Vec3f)(0, 0, 0);
	Vec3f staticParticleViscosityForce = (Vec3f)(0, 0, 0);

	float particleDensity = particlePressure / parameters->behaviour.gasConstant + parameters->behaviour.restDensity;

	Vec3i otherCell;
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = GetHash(otherCell);

				uint otherHashMod = otherHash % parameters->dynamicParticleHashMapSize;
				uint beginIndex = hashMap[otherHashMod];
				uint endIndex = hashMap[otherHashMod + 1];

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					uint index = particleMap[i];

					if (index == inParticleIndex)
						continue;

					float otherParticlePressure = outParticles[index].positionAndPressure.w;
					Vec3f otherParticlePosition = inParticles[index].positionAndPressure.xyz();
					Vec3f otherParticleVelocity = inParticles[index].velocityAndHash.xyz();

					Vec3f dir = otherParticlePosition - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > parameters->behaviour.maxInteractionDistance * parameters->behaviour.maxInteractionDistance)
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
					dynamicParticlePressureForce += dir * (particlePressure + otherParticlePressure) * SmoothingKernelD1(dist, parameters->behaviour.maxInteractionDistance);

					//apply viscosity force					
					dynamicParticleViscosityForce += (otherParticleVelocity - particleVelocity) * SmoothingKernelD2(dist, parameters->behaviour.maxInteractionDistance);
				}
	
				otherHashMod = otherHash % parameters->staticParticleHashMapSize;
				beginIndex = staticParticlesHashMap[otherHashMod];
				endIndex = staticParticlesHashMap[otherHashMod + 1];

				for (uint i = beginIndex; i < endIndex; ++i)
				{					
					STRUCT StaticParticle otherParticle = staticParticles[i];

					Vec3f dir = otherParticle.positionAndPressure.xyz() - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > parameters->behaviour.maxInteractionDistance * parameters->behaviour.maxInteractionDistance)
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
					staticParticlePressureForce += dir * fabs(particlePressure) * SmoothingKernelD1(dist, parameters->behaviour.maxInteractionDistance);

					//apply viscosity force					
					staticParticleViscosityForce += -particleVelocity * SmoothingKernelD2(dist, parameters->behaviour.maxInteractionDistance);
				}
			}

	dynamicParticlePressureForce *= parameters->behaviour.particleMass / (2 * particleDensity) * parameters->smoothingKernelConstant;
	dynamicParticleViscosityForce *= parameters->behaviour.viscosity * parameters->behaviour.particleMass * parameters->smoothingKernelConstant;
	staticParticlePressureForce *= parameters->behaviour.particleMass / (2 * particleDensity) * parameters->smoothingKernelConstant * 1.0f;
	staticParticleViscosityForce *= parameters->behaviour.viscosity * parameters->behaviour.particleMass * parameters->smoothingKernelConstant * 0.0f;

	Vec3f particleForce = dynamicParticlePressureForce + dynamicParticleViscosityForce + staticParticlePressureForce + staticParticleViscosityForce;
	Vec3f acceleration = particleForce / particleDensity + NEW_VEC3F(parameters->behaviour.gravityX, parameters->behaviour.gravityY, parameters->behaviour.gravityZ);

	//Integrate
	particleVelocity += acceleration * deltaTime;
	particlePosition += particleVelocity * deltaTime;

	if (parameters->bounds.bounded)
	{
		if (parameters->bounds.boundedByWalls)
		{
			if (particlePosition.x < parameters->bounds.boxOffset.x) {
				particlePosition.x = parameters->bounds.boxOffset.x;
				particleVelocity.x = -particleVelocity.x * parameters->bounds.wallElasticity;
			}

			if (particlePosition.x >= parameters->bounds.boxOffset.x + parameters->bounds.boxSize.x) {
				particlePosition.x = parameters->bounds.boxOffset.x + parameters->bounds.boxSize.x - FLT_EPSILON;
				particleVelocity.x = -particleVelocity.x * parameters->bounds.wallElasticity;
			}

			if (particlePosition.z < parameters->bounds.boxOffset.z) {
				particlePosition.z = parameters->bounds.boxOffset.z;
				particleVelocity.z = -particleVelocity.z * parameters->bounds.wallElasticity;
			}

			if (particlePosition.z >= parameters->bounds.boxOffset.z + parameters->bounds.boxSize.z) {
				particlePosition.z = parameters->bounds.boxOffset.z + parameters->bounds.boxSize.z - FLT_EPSILON;
				particleVelocity.z = -particleVelocity.z * parameters->bounds.wallElasticity;
			}
		}

		if (particlePosition.y < parameters->bounds.boxOffset.y)
		{
			particlePosition.y = parameters->bounds.boxOffset.y;
			particleVelocity.y = -particleVelocity.y * parameters->bounds.wallElasticity;
		}

		if (parameters->bounds.boundedByRoof)
		{
			if (particlePosition.y >= parameters->bounds.boxOffset.y + parameters->bounds.boxSize.y)
			{
				particlePosition.y = parameters->bounds.boxOffset.y + parameters->bounds.boxSize.y - FLT_EPSILON;
				particleVelocity.y = -particleVelocity.y * parameters->bounds.wallElasticity;
			}
		}
	}
	cell = GetCell(particlePosition, parameters->behaviour.maxInteractionDistance);
	particleHash = GetHash(cell) % parameters->dynamicParticleHashMapSize;

	atomic_inc(newHashMap + particleHash);

#ifdef CL_COMPILER
	outParticlePtr->positionAndPressure.xyz = particlePosition;
	outParticlePtr->velocityAndHash.xyz = particleVelocity;
	outParticlePtr->velocityAndHash.w = *(float*)&particleHash;
#else
	outParticlePtr->position = particlePosition;
	outParticlePtr->velocity = particleVelocity;
	outParticlePtr->hash = particleHash;
#endif

}

#ifndef CL_COMPILER
#undef Vec3f
#undef Vec3i
#undef  modf
#undef GLOBAL
#undef STRUCT
#undef xyz
#endif
