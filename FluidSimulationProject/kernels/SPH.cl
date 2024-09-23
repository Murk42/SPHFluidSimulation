#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

#define DEBUG_BUFFERS

struct __attribute__ ((packed)) Particle
{
	float4 positionAndPressure;
	float4 velocityAndHash;		

	float4 color;
};	
struct __attribute__ ((packed)) StaticParticle
{
	float4 positionAndPressure;		

	float4 color;
};

void kernel computeParticleHashes(global struct Particle* particles, global uint* hashMap, global uint* particleMap)
{
	float3 particlePosition = particles[get_global_id(0)].positionAndPressure.xyz;	

	int3 cell = GetCell(particlePosition, MAX_INTERACTION_DISTANCE);	
	uint particleHash = GetHash(cell) % HASH_MAP_SIZE;

	particles[get_global_id(0)].velocityAndHash.w = *(float*)&particleHash;
	atomic_inc(hashMap + particleHash);		
}

void kernel computeParticleMap(global const struct Particle* particles, global uint* hashMap, global uint* particleMap)
{	
	float particleHash_FLOAT = particles[get_global_id(0)].velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	uint index = atomic_dec(hashMap + particleHash) - 1;
		
	particleMap[index] = get_global_id(0);	
}

void kernel updateParticlesPressure(
	global struct Particle* particles,
	global uint* hashMap,
	global uint* particleMap,
	global const uint* staticParticleHashMap,
	global const struct StaticParticle* staticParticles
) {
	global struct Particle* particlePtr = particles + get_global_id(0);

#ifdef DEBUG_BUFFERS
	global struct Particle* firstPtr = particles;
	global struct Particle* behindPtr = particles + PARTICLE_COUNT;
	if (particlePtr < firstPtr || particlePtr >= behindPtr)
	{
		printf("Reading outside valid memory of particles at beginning");
		return;
	}
#endif

	float3 particlePosition = particlePtr->positionAndPressure.xyz;
	float particlePressure = particlePtr->positionAndPressure.w;
	float3 particleVelocity = particlePtr->velocityAndHash.xyz;
	float particleHash_FLOAT = particlePtr->velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;

	if (get_global_id(0) == 0)
		particlePtr->color.w = 1.0f;

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
					global const struct Particle* otherParticlePtr = particles + particleMap[i];

					if (particlePtr == otherParticlePtr)
						continue;

#ifdef DEBUG_BUFFERS
					if (particlePtr < firstPtr || particlePtr >= behindPtr)
					{
						printf("Reading outside valid memory of particles in neighbour loop");
						continue;
					}					
#endif					

					float3 dir = otherParticlePtr->positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;

					float dist = sqrt(distSqr);					

					if (particleMap[i] == 0)
						particlePtr->color.w = 0.5f;

					influenceSum += SmoothingKernelD0(dist, MAX_INTERACTION_DISTANCE);
				}

				
#if STATIC_PARTICLE_COUNT != 0		
				//Calculating static particle pressure
				otherHashMod = otherHash % STATIC_HASH_MAP_SIZE;
				beginIndex = staticParticleHashMap[otherHashMod];
				endIndex = staticParticleHashMap[otherHashMod + 1];				

				for (uint i = beginIndex; i < endIndex; ++i)
				{	
#ifdef DEBUG_BUFFERS
					if (i >= STATIC_PARTICLE_COUNT)
					{
						printf("Reading outside valid memory of staticParticleMap in neighbour loop. begin: %4d end: %4d hash: %4d", beginIndex, endIndex, otherHash);
						break;
					}
#endif

					global struct StaticParticle* otherParticlePtr = staticParticles + i;

#ifdef DEBUG_BUFFERS
//					if (particlePtr < staticParticles || particlePtr >= staticParticles + STATIC_PARTICLE_COUNT)
//					{
//						printf("Reading outside valid memory of static particles in neighbour loop");
//						continue;
//					}					
#endif

					float3 dir = otherParticlePtr->positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE)
						continue;


					float dist = sqrt(distSqr);	
					
					if (get_global_id(0) == 0)
						otherParticlePtr->color.w = 0.5f;
										
					influenceSum += SmoothingKernelD0(dist, MAX_INTERACTION_DISTANCE);																
				}
#endif					
			}			
			
	influenceSum *= SMOOTHING_KERNEL_CONSTANT;
	float particleDensity = SELF_DENSITY + influenceSum * PARTICLE_MASS;	
	particlePressure = GAS_CONSTANT * (particleDensity - REST_DENSITY);	
		
	particlePtr->positionAndPressure.w = particlePressure;		
}
void kernel updateParticlesDynamics(
	global struct Particle* particles, 
	global struct Particle* outParticlesPtr, 
	global uint* hashMap, 
	global uint* newHashMap, 
	global uint* particleMap, 
	global const struct StaticParticle* staticParticles,
	global const uint* staticParticleHashMap,
	float deltaTime,
	int moveParticles
) {			
	global struct Particle* particlePtr = particles;

	if (moveParticles)
		particlePtr += particleMap[get_global_id(0)];
	else
		particlePtr += get_global_id(0);

	float3 particlePosition = particlePtr->positionAndPressure.xyz;
	float particlePressure = particlePtr->positionAndPressure.w;
	float3 particleVelocity = particlePtr->velocityAndHash.xyz;
	float particleHash_FLOAT = particlePtr->velocityAndHash.w;
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
					global const struct Particle* otherParticlePtr = particles + particleMap[i];

					if (particlePtr == otherParticlePtr)
						continue;					

					struct Particle otherParticle = *otherParticlePtr;

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
					pressureForce += dir * (particlePressure + otherParticle.positionAndPressure.w) * SmoothingKernelD1(dist, MAX_INTERACTION_DISTANCE);

					//apply viscosity force					
					viscosityForce += (otherParticle.velocityAndHash.xyz - particleVelocity) * SmoothingKernelD2(dist, MAX_INTERACTION_DISTANCE);
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


	global struct Particle* outParticlePtr = outParticlesPtr + get_global_id(0);	
	outParticlePtr->positionAndPressure.xyz = particlePosition;
	outParticlePtr->positionAndPressure.w = particlePressure;
	outParticlePtr->velocityAndHash.xyz = particleVelocity;
	outParticlePtr->velocityAndHash.w = *(float*)&particleHash;		

}