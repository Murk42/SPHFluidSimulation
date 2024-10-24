#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

void kernel computeParticleHashes(global struct DynamicParticle* particles, volatile global uint* hashMap, float maxInteractionDistance, uint hashMapSize)
{
	float3 particlePosition = particles[get_global_id(0)].positionAndPressure.xyz;	

	int3 cell = GetCell(particlePosition, maxInteractionDistance);	
	uint particleHash = GetHash(cell) % hashMapSize;

	particles[get_global_id(0)].velocityAndHash.w = *(float*)&particleHash;
	
	atomic_inc(hashMap + particleHash);			
}

void kernel computeParticleMap(
	global const struct DynamicParticle* particles, 
	global struct DynamicParticle* orderedParticles, 
	global uint* hashMap, 
	global uint* particleMap, 
	uint reorderParticles
) {	
	ComputeParticleMap(
		get_global_id(0),
		particles,
		orderedParticles,
		hashMap,
		particleMap,
		reorderParticles
	);	
}

void kernel reorderParticles(global const struct DynamicParticle* inArray, global struct DynamicParticle* outArray, global uint* hashMap, global uint* map)
{
	outArray[get_global_id(0)] = inArray[map[get_global_id(0)]];
	map[get_global_id(0)] = get_global_id(0);	
}

void kernel updateParticlesPressure(
	global const struct DynamicParticle* inParticles, 
	global struct DynamicParticle* outParticles, 
	global const uint* hashMap,
	global const uint* particleMap,
	global const struct StaticParticle* staticParticles,
	global const uint* staticParticleHashMap,
	global struct ParticleSimulationParameters* simulationParameters
) {
	UpdateParticlePressure(
		get_global_id(0),
		inParticles, 
		outParticles,
		hashMap,
		particleMap,
		staticParticles,
		staticParticleHashMap,
		simulationParameters
		);
}
void kernel updateParticlesDynamics(
	global const struct DynamicParticle* inParticles, 
	global struct DynamicParticle* outParticles, 
	global const uint* hashMap, 
	global uint* newHashMap, 
	global const uint* particleMap, 
	global const struct StaticParticle* staticParticles,
	global const uint* staticParticlesHashMap,
	const float deltaTime,
	global struct ParticleSimulationParameters* simulationParameters
) {			
	UpdateParticleDynamics(
		get_global_id(0),
		inParticles,
		outParticles,
		hashMap,
		newHashMap,
		particleMap,
		staticParticles,
		staticParticlesHashMap,
		deltaTime,
		simulationParameters
	);
}