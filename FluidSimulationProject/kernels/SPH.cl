#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

struct __attribute__ ((packed)) Particle
{
	float4 positionAndPressure;
	float4 velocityAndHash;	
	float4 color;
};	

int3 getCell(float3 position)
{
	return convert_int3(position / maxInteractionDistance);
}

//uint getHash(int3 cell)
//{
//	cell.x = (cell.x % 3 + 3) % 3;
//	cell.y = (cell.y % 3 + 3) % 3;
//	cell.z = (cell.z % 3 + 3) % 3;
//
//	return (cell.x + (cell.y + cell.z * 3) * 3) % hashMapSize;
//}
uint getHash(int3 cell)
{
	return (
		(uint)(cell.x * 73856093)
		^ (uint)(cell.y * 19349663)
		^ (uint)(cell.z * 83492791)) % hashMapSize;
}

float SmoothingKernelD0(float r)
{
	if (r >= maxInteractionDistance)
		return 0;
	
	float dist = maxInteractionDistance - r;
	return dist * dist * smoothingKernelConstant;
}
float SmoothingKernelD1(float r)
{
	if (r >= maxInteractionDistance)
		return 0;

	return 2 * (r - maxInteractionDistance) * smoothingKernelConstant;
}
float SmoothingKernelD2(float r)
{
	if (r >= maxInteractionDistance)
		return 0;
	return 2 * smoothingKernelConstant;
}

float noise(float x) {
	float ptr = 0.0f;
	return fract(sin(x*112.9898f) * 43758.5453f, &ptr);
}

void kernel computeParticleHashes(global struct Particle* particles, global uint* hashMap, global uint* particleMap)
{
	float3 particlePosition = particles[get_global_id(0)].positionAndPressure.xyz;	

	int3 cell = getCell(particlePosition);	
	uint particleHash = getHash(cell);	

	particles[get_global_id(0)].velocityAndHash.w = *(float*)&particleHash;
	atomic_inc(hashMap + particleHash);		
}

void kernel computeParticleMap(global struct Particle* particles, global uint* hashMap, global uint* particleMap)
{	
	float particleHash_FLOAT = particles[get_global_id(0)].velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;
		
	particleMap[atomic_dec(hashMap + particleHash) - 1] = get_global_id(0);
}

void kernel updateParticlesPressure(global struct Particle* particles, global uint* hashMap, global uint* particleMap)
{
	global struct Particle* firstPtr = particles;
	global struct Particle* behindPtr = particles + particleCount;

	global struct Particle* particlePtr = particles + get_global_id(0);

	if (particlePtr < firstPtr || particlePtr >= behindPtr)
	{
		printf("Reading outside valid memory of particles at beginning");
		return;
	}

	float3 particlePosition = particlePtr->positionAndPressure.xyz;
	float particlePressure = particlePtr->positionAndPressure.w;
	float3 particleVelocity = particlePtr->velocityAndHash.xyz;
	float particleHash_FLOAT = particlePtr->velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;		

	int3 cell = getCell(particlePosition);	

	int3 beginCell = cell - (int3)(1, 1, 1);
	int3 endCell = cell + (int3)(2, 2, 2);

	float influenceSum = 0;	
	
	int3 otherCell;	
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = getHash(otherCell);

				if (otherHash >= hashMapSize)
				{
					printf("Reading outside valid memory of hashMap in neighbour loop");
					continue;
				}

				uint beginIndex = hashMap[otherHash];
				uint endIndex = hashMap[otherHash + 1];				

				for (uint i = beginIndex; i < endIndex; ++i)
				{			
					if (i >= particleCount)
					{
						printf("Reading outside valid memory of particleMap in neighbour loop");
						continue;
					}

					global struct Particle* otherParticlePtr = particles + particleMap[i];

					if (particlePtr == otherParticlePtr)
						continue;					

					if (particlePtr < firstPtr || particlePtr >= behindPtr)
					{
						printf("Reading outside valid memory of particles in neighbour loop");
						continue;
					}					

					float3 dir = otherParticlePtr->positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > maxInteractionDistance * maxInteractionDistance)
						continue;

					float dist = sqrt(distSqr);					
					
					influenceSum += SmoothingKernelD0(dist);																
				}
			}
			
	
	float particleDensity = selfDensity + influenceSum * particleMass;	
	particlePressure = gasConstant * (particleDensity - restDensity);	
		
	particlePtr->positionAndPressure.w = particlePressure;	
}
void kernel updateParticlesDynamics(global struct Particle* particles, global struct Particle* outParticlesPtr, global uint* hashMap, global uint* newHashMap, global uint* particleMap, float deltaTime)
{		
	global struct Particle* particlePtr = particles + get_global_id(0);
	float3 particlePosition = particlePtr->positionAndPressure.xyz;
	float particlePressure = particlePtr->positionAndPressure.w;
	float3 particleVelocity = particlePtr->velocityAndHash.xyz;
	float particleHash_FLOAT = particlePtr->velocityAndHash.w;
	uint particleHash = *(uint*)&particleHash_FLOAT;	

	int3 cell = getCell(particlePosition);	

	int3 beginCell = cell - (int3)(1, 1, 1);
	int3 endCell = cell + (int3)(2, 2, 2);

	float3 pressureForce = (float3)(0, 0, 0);
	float3 viscosityForce = (float3)(0, 0, 0);

	float particleDensity = particlePressure / gasConstant + restDensity;
	
	uint nc = 0;
	int3 otherCell;	
	for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
		for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
			for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
			{
				uint otherHash = getHash(otherCell);

				uint beginIndex = hashMap[otherHash];
				uint endIndex = hashMap[otherHash + 1];

				for (uint i = beginIndex; i < endIndex; ++i)
				{
					uint index = particleMap[i];
					if (get_global_id(0) == index)
						continue;

					struct Particle otherParticle = particles[index];

					float3 dir = otherParticle.positionAndPressure.xyz - particlePosition;
					float distSqr = dot(dir, dir);

					if (distSqr > maxInteractionDistance * maxInteractionDistance)
						continue;

					float dist = sqrt(distSqr);

					if (distSqr == 0 || dist == 0)
					{							
						//https://math.stackexchange.com/questions/44689/how-to-find-a-random-axis-or-unit-vector-in-3d
						float theta = noise(get_global_id(0)) * 2 * 3.1415;
						float z = noise(get_global_id(0)) * 2 - 1;

						float s = sin(theta);
						float c = cos(theta);
						float z2 = sqrt(1 - z * z);
						dir = (float3)(z2 * c, z2 * s, z);											
						dist = 0;
					}
					else											
						dir /= dist;										

					//printf("[%d] hash: %d index: %d neighbourPosition: % 10.2v4f", get_global_id(0), i, index, otherParticle.positionAndPressure.xyz);

					//apply pressure force
					float3 dPressureForce;
					dPressureForce = dir * (particlePressure + otherParticle.positionAndPressure.w);
					dPressureForce *= SmoothingKernelD1(dist);
					pressureForce += dPressureForce;					

					//apply viscosity force
					float3 dViscosityForce;
					dViscosityForce = (otherParticle.velocityAndHash.xyz - particleVelocity);
					dViscosityForce *= SmoothingKernelD2(dist);
					viscosityForce += dViscosityForce;		
					
					++nc;
				}
			}

	pressureForce *= particleMass / (2 * particleDensity);
	viscosityForce *= viscosity * particleMass;	

	float3 particleForce = pressureForce + viscosityForce;
	float3 acceleration = particleForce / particleDensity + (float3)(0, -9.81, 0);
				
	particleVelocity += acceleration * deltaTime;


	//float speed = length(particleVelocity);
	//particleVelocity /= speed;
	//speed = min(speed, maxInteractionDistance * 0.5f / deltaTime);
	//particleVelocity *= speed;
	
	particlePosition += particleVelocity * deltaTime;	

	float3 minPos = (float3)(0, 0, 0);
	float3 maxPos = boundingBoxSize;
		
	//if (particlePosition.x < minPos.x) {
	//	particlePosition.x = minPos.x;
	//	particleVelocity.x = -particleVelocity.x * elasticity;
	//}
	//
	//if (particlePosition.x >= maxPos.x) {
	//	particlePosition.x = maxPos.x - FLT_EPSILON;
	//	particleVelocity.x = -particleVelocity.x * elasticity;
	//}
	
	if (particlePosition.y < minPos.y) 
	{
		particlePosition.y = minPos.y;
		particleVelocity.y = -particleVelocity.y * elasticity;
	}
	
	//if (particlePosition.y >= maxPos.y)
	//{
	//	particlePosition.y = maxPos.y - FLT_EPSILON;
	//	particleVelocity.y = -particleVelocity.y * elasticity;
	//}
	//
	//if (particlePosition.z < minPos.z) {
	//	particlePosition.z = minPos.z;
	//	particleVelocity.z = -particleVelocity.z * elasticity;
	//}
	//
	//if (particlePosition.z >= maxPos.z) {
	//	particlePosition.z = maxPos.z - FLT_EPSILON;
	//	particleVelocity.z = -particleVelocity.z * elasticity;
	//}	


	cell = getCell(particlePosition);	
	particleHash = getHash(cell);		
	
	//printf("[%d] pos: % 10.2v3f dens: % 8.2f hash: % 5d nc: % 5d", get_global_id(0), particlePosition, particleDensity, particleHash, nc);

	atomic_inc(newHashMap + particleHash);	 

	global struct Particle* outParticlePtr = outParticlesPtr + get_global_id(0);
	outParticlePtr->positionAndPressure.xyz = particlePosition;
	outParticlePtr->positionAndPressure.w = particlePressure;
	outParticlePtr->velocityAndHash.xyz = particleVelocity;
	outParticlePtr->velocityAndHash.w = *(float*)&particleHash;		

}