#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

#define DEBUG_BUFFERS

struct __attribute__ ((packed)) DynamicParticle
{
	float4 positionAndPressure;
	float4 velocityAndHash;
};	
struct __attribute__ ((packed)) StaticParticle
{
	float4 positionAndPressure;		
};

//void kernel computeParticleHashes(
//	global char* positions, uint positionsStride, uint positionsOffset,
//	global char* hashes, uint hashesStride, uint hashesOffset,
//	global uint* particleMap)
//{
//	float3 position = *(float*)(positions + positionsStride * get_global_id(0) + positionsOffset);
//
//	int3 cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	
//	uint particleHash = GetHash(cell) % HASH_MAP_SIZE;
//
//	*(uint*)(hashes + hashesStride * get_global_id(0) + hashesOffset) = *(float*)&particleHash;	
//	atomic_inc(hashMap + particleHash);
//}
void kernel computeParticleHashes(global struct DynamicParticle* particles, global uint* hashMap)
{
	float3 particlePosition = particles[get_global_id(0)].positionAndPressure.xyz;	

	int3 cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	
	uint particleHash = GetHash(cell) % HASH_MAP_SIZE;

	particles[get_global_id(0)].velocityAndHash.w = *(float*)&particleHash;
	atomic_inc(hashMap + particleHash);		
}

void kernel computeParticleMap(global const struct DynamicParticle* particles, global uint* hashMap, global uint* particleMap)
{	
	float particleHash_FLOAT = particles[get_global_id(0)].velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	uint index = atomic_dec(hashMap + particleHash) - 1;
		
	particleMap[index] = get_global_id(0);	
}

void kernel updateParticlesPressure(
	global const struct DynamicParticle* inParticles, 
	global struct DynamicParticle* outParticles, 
	global const uint* hashMap,
	global const uint* particleMap,
	global const struct StaticParticle* staticParticles,
	global const uint* staticParticleHashMap
#ifdef VISUALIZE_NEIGHBOURS
	, global uint* dynamicParticleColors,
	global uint* staticParticleColors
#endif
) {
	global const struct DynamicParticle* inParticlePtr = inParticles + get_global_id(0);
	global struct DynamicParticle* outParticlePtr = outParticles + get_global_id(0);	

#ifdef DEBUG_BUFFERS
	if (get_global_id(0) >= PARTICLE_COUNT)		
	{
		printf("Allocating more work items than particles");
		return;
	}
#endif
#ifdef VISUALIZE_NEIGHBOURS
//	if (get_global_id(0) == 0)
//		dynamicParticleColors[0] = 1.0f;
#endif

	float3 particlePosition = inParticlePtr->positionAndPressure.xyz;		

	int3 cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	

	int3 beginCell = cell - (int3)(1, 1, 1);
	int3 endCell = cell + (int3)(2, 2, 2);

	float influenceSum = 0;	
	
	int3 otherCell;	
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = GetHash(otherCell);

				//Calculating dynamic particle pressure
				uint otherHashMod = otherHash % HASH_MAP_SIZE;				
				uint beginIndex = hashMap[otherHashMod];
				uint endIndex = hashMap[otherHashMod + 1];	

#ifdef DEBUG_BUFFERS				
				if (beginIndex > endIndex)
				{
					printf("Begin index is bigger than end index. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > PARTICLE_COUNT)
				{
					printf("Invalid begin index: %u", beginIndex);
					break;
				}
				if (endIndex > PARTICLE_COUNT)
				{
					printf("Invalid end index: %u", endIndex);
					break;
				}
#endif

				for (uint i = beginIndex; i < endIndex; ++i)
				{	
					uint index = particleMap[i];

					if (index >= PARTICLE_COUNT)
					{
						printf("DynamicParticle map value outside valid range");
						continue;
					}			
					
#ifdef VISUALIZE_NEIGHBOURS
					//if (index == 0)
					//	dynamicParticleColors[get_global_id(0)] = 0.5f;
#endif

					global const struct DynamicParticle* otherParticlePtr = inParticles + index;

					if (inParticlePtr == otherParticlePtr)
						continue;	

					float3 dir = otherParticlePtr->positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;

					float dist = sqrt(distSqr);					

					influenceSum += SmoothingKernelD0(dist, MAX_INTERACTION_DISTANCE);
				}

				
#if STATIC_PARTICLE_COUNT != 0		
				//Calculating static particle pressure
				otherHashMod = otherHash % STATIC_HASH_MAP_SIZE;
				beginIndex = staticParticleHashMap[otherHashMod];
				endIndex = staticParticleHashMap[otherHashMod + 1];				

#ifdef DEBUG_BUFFERS				
				if (beginIndex > endIndex)
				{
					printf("Begin index is bigger than end index for static particles. Begin: %u End: %u", beginIndex, endIndex);
					break;
				}
				if (beginIndex > STATIC_PARTICLE_COUNT)
				{
					printf("Invalid begin index: %u", beginIndex);
					break;
				}
				if (endIndex > STATIC_PARTICLE_COUNT)
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

					global const struct StaticParticle* otherParticlePtr = staticParticles + i;

					float3 dir = otherParticlePtr->positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;


					float dist = sqrt(distSqr);					
										
					influenceSum += SmoothingKernelD0(dist, MAX_INTERACTION_DISTANCE);																
				}
#endif					
			}			
			
	influenceSum *= SMOOTHING_KERNEL_CONSTANT;
	float particleDensity = SELF_DENSITY + influenceSum * PARTICLE_MASS;	
	float particlePressure = GAS_CONSTANT * (particleDensity - REST_DENSITY);	
		
	outParticlePtr->positionAndPressure.w = particlePressure;			
}
void kernel updateParticlesDynamics(
	global const struct DynamicParticle* inParticles, 
	global struct DynamicParticle* outParticles, 
	global const uint* hashMap, 
	global uint* newHashMap, 
	global const uint* particleMap, 
	global const struct StaticParticle* staticParticles,
	global const uint* staticParticleHashMap,
	const float deltaTime	
) {			
	global const struct DynamicParticle* inParticlePtr = inParticles + get_global_id(0);
	global struct DynamicParticle* outParticlePtr = outParticles + get_global_id(0);	

	float3 particlePosition = inParticlePtr->positionAndPressure.xyz;
	float particlePressure = outParticlePtr->positionAndPressure.w;
	float3 particleVelocity = inParticlePtr->velocityAndHash.xyz;
	float particleHash_FLOAT = inParticlePtr->velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	int3 cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	

	int3 beginCell = cell - (int3)(1, 1, 1);
	int3 endCell = cell + (int3)(2, 2, 2);

	float3 pressureForce = (float3)(0, 0, 0);
	float3 viscosityForce = (float3)(0, 0, 0);

	float particleDensity = particlePressure / GAS_CONSTANT + REST_DENSITY;	
		
	int3 otherCell;	
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = GetHash(otherCell);

				uint otherHashMod = otherHash % HASH_MAP_SIZE;				
				uint beginIndex = hashMap[otherHashMod];
				uint endIndex = hashMap[otherHashMod + 1];

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					uint index = particleMap[i];

					float otherParticlePressure = outParticlePtr[index].positionAndPressure.w;
					float3 otherParticlePosition = inParticlePtr[index].positionAndPressure.xyz;
					float3 otherParticleVelocity = inParticlePtr[index].velocityAndHash.xyz;					

					if (index == get_global_id(0))
						continue;					
					

					float3 dir = otherParticlePosition - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;

					float dist = sqrt(distSqr);
					
					if (distSqr == 0 || dist == 0)					
						dir = RandomDirection(get_global_id(0));		
					else											
						dir /= dist;															

					//apply pressure force					
					pressureForce += dir * (particlePressure + otherParticlePressure) * SmoothingKernelD1(dist, MAX_INTERACTION_DISTANCE);

					//apply viscosity force					
					viscosityForce += (otherParticleVelocity - particleVelocity) * SmoothingKernelD2(dist, MAX_INTERACTION_DISTANCE);
				}

#if STATIC_PARTICLE_COUNT != 0				
				otherHashMod = otherHash % STATIC_HASH_MAP_SIZE;								
				beginIndex = staticParticleHashMap[otherHashMod];
				endIndex = staticParticleHashMap[otherHashMod + 1];

				for (uint i = beginIndex; i < endIndex; ++i)
				{	
					global const struct StaticParticle* otherParticlePtr = staticParticles + i;
					struct StaticParticle otherParticle = *otherParticlePtr;

					float3 dir = otherParticle.positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);					

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;

					float dist = sqrt(distSqr);					

					if (distSqr == 0 || dist == 0)							
						dir = RandomDirection(get_global_id(0));													
					else											
						dir /= dist;										

					//apply pressure force					
					pressureForce += dir * (fabs(particlePressure) * 4) * SmoothingKernelD1(dist, MAX_INTERACTION_DISTANCE);					

					//apply viscosity force					
					viscosityForce += -particleVelocity * SmoothingKernelD2(dist, MAX_INTERACTION_DISTANCE);
				}
#endif
			}
		
	pressureForce *= PARTICLE_MASS / (2 * particleDensity) * SMOOTHING_KERNEL_CONSTANT;
	viscosityForce *= VISCOSITY * PARTICLE_MASS * SMOOTHING_KERNEL_CONSTANT;	

	float3 particleForce = pressureForce + viscosityForce;
	float3 acceleration = particleForce / particleDensity + GRAVITY;	
				
	//Integrate
	particleVelocity += acceleration * deltaTime;	
	particlePosition += particleVelocity * deltaTime;	

#ifdef BOUND_PARTICLES
#ifdef BOUND_WALLS
	if (particlePosition.x < BOUNDING_BOX_POINT_1.x) {
		particlePosition.x = BOUNDING_BOX_POINT_1.x;
		particleVelocity.x = -particleVelocity.x * ELASTICITY;
	}
	
	if (particlePosition.x >= BOUNDING_BOX_POINT_2.x) {
		particlePosition.x = BOUNDING_BOX_POINT_2.x - FLT_EPSILON;
		particleVelocity.x = -particleVelocity.x * ELASTICITY;
	}
#endif
	
	if (particlePosition.y < BOUNDING_BOX_POINT_1.y) 
	{
		particlePosition.y = BOUNDING_BOX_POINT_1.y;
		particleVelocity.y = -particleVelocity.y * ELASTICITY;
	}
	
#ifdef BOUND_TOP
	if (particlePosition.y >= BOUNDING_BOX_POINT_2.y)
	{
		particlePosition.y = BOUNDING_BOX_POINT_2.y - FLT_EPSILON;
		particleVelocity.y = -particleVelocity.y * ELASTICITY;
	}
#endif
	
#ifdef BOUND_WALLS
	if (particlePosition.z < BOUNDING_BOX_POINT_1.z) {
		particlePosition.z = BOUNDING_BOX_POINT_1.z;
		particleVelocity.z = -particleVelocity.z * ELASTICITY;
	}
	
	if (particlePosition.z >= BOUNDING_BOX_POINT_2.z) {
		particlePosition.z = BOUNDING_BOX_POINT_2.z - FLT_EPSILON;
		particleVelocity.z = -particleVelocity.z * ELASTICITY;
	}	
#endif
#endif
	cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	
	particleHash = GetHash(cell) % HASH_MAP_SIZE;

	atomic_inc(newHashMap + particleHash);	 
	
	outParticlePtr->positionAndPressure.xyz = particlePosition;	
	outParticlePtr->velocityAndHash.xyz = particleVelocity;
	outParticlePtr->velocityAndHash.w = *(float*)&particleHash;		

}