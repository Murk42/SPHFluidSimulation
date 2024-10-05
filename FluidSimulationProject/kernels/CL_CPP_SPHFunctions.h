
//#define DEBUG_BUFFERS

Vec3i GetCell(Vec3f position, float maxInteractionDistance)
{
    position = floor(position / maxInteractionDistance);
    return CONVERT_VEC3I(position);
}

uint GetHash(Vec3i cell)
{
    return (
        (((uint)cell.x) * 73856093) ^
        (((uint)cell.y) * 19349663) ^
        (((uint)cell.z) * 83492791)
        );
}

//float SmoothingKernelConstant(float h)
//{
//    return 30.0f / (3.1415 * pow(h, 5));
//}
//float SmoothingKernelD0(float r, float maxInteractionDistance)
//{
//    if (r >= maxInteractionDistance)
//        return 0;
//
//    float distance = maxInteractionDistance - r;
//    return distance * distance;
//}
//float SmoothingKernelD1(float r, float maxInteractionDistance)
//{
//    if (r >= maxInteractionDistance)
//        return 0;
//
//    return 2 * (r - maxInteractionDistance);
//}
//float SmoothingKernelD2(float r, float maxInteractionDistance)
//{
//    if (r >= maxInteractionDistance)
//        return 0;
//
//    return 2;
//}

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

void UpdateParticlePressure(
	uint threadID,
	GLOBAL const STRUCT DynamicParticle* inParticles,
	GLOBAL STRUCT DynamicParticle* outParticles,
	GLOBAL const uint* hashMap,
	GLOBAL const uint* particleMap,
	GLOBAL STRUCT StaticParticle* staticParticles,
	GLOBAL const uint* staticParticlesHashMap,
	GLOBAL STRUCT ParticleSimulationParameters* parameters
) {
	GLOBAL const STRUCT DynamicParticle* inParticlePtr = inParticles + threadID;
	GLOBAL STRUCT DynamicParticle* outParticlePtr = outParticles + threadID;

#ifdef DEBUG_BUFFERS
	if (threadID >= parameters->dynamicParticleCount)
	{
		printf("Allocating more work items than particles");
		return;
	}
#endif
#ifdef VISUALIZE_NEIGHBOURS
	//	if (get_global_id(0) == 0)
	//		dynamicParticleColors[0] = 1.0f;
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

#ifdef DEBUG_BUFFERS				
				if (beginIndex > endIndex)
				{
					printf("Begin index is bigger than end index. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > parameters->dynamicParticleCount)
				{
					printf("Invalid begin index: %u", beginIndex);
					break;
				}
				if (endIndex > parameters->dynamicParticleCount)
				{
					printf("Invalid end index: %u", endIndex);
					break;
				}
#endif

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					uint index = particleMap[i];

					if (index >= parameters->dynamicParticleCount)
					{
						printf("DynamicParticle map value outside valid range");
						continue;
					}

#ifdef VISUALIZE_NEIGHBOURS
					//if (index == 0)
					//	dynamicParticleColors[get_global_id(0)] = 0.5f;
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

#ifdef DEBUG_BUFFERS				
				if (beginIndex > endIndex)
				{
					printf("Begin index is bigger than end index for static particles. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > parameters->staticParticleCount)
				{
					printf("Invalid begin index: %u", beginIndex);
					break;
				}
				if (endIndex > parameters->staticParticleCount)
				{
					printf("Invalid end index: %u", endIndex);
					break;
				}
#endif

				for (uint i = beginIndex; i < endIndex; ++i)
				{
#ifdef VISUALIZE_NEIGHBOURS
					//if (get_global_id(0) == 0)
					//	staticParticleColors[i] = 0.5f;
#endif

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
	GLOBAL const uint* hashMap,
	GLOBAL uint* newHashMap,
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
