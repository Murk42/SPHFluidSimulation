#include "pch.h"
#include "SystemCPU.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{
	SystemCPU::SystemCPU(ThreadPool& threadPool) :
		threadPool(threadPool), threadIdleCount(threadPool.ThreadCount())
	{
	}
	SystemCPU::~SystemCPU()
	{
		StopThreads();
	}

	void SystemCPU::Initialize(const SystemInitParameters& initParams)
	{
		behaviourParameters = initParams.particleBehaviourParameters;
		boundParameters = initParams.particleBoundParameters;
		smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		selfDensity = behaviourParameters.particleMass * SmoothingKernelD0(0, behaviourParameters.maxInteractionDistance) * smoothingKernelConstant;

		hashesPerDynamicParticle = initParams.hashesPerParticle;

		Array<DynamicParticle> dynamicParticles;
		initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);
		initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		dynamicParticleCount = dynamicParticles.Count();
		staticParticleCount = staticParticles.Count();

		bufferSets.Clear();
		bufferSets.Resize(initParams.bufferCount);
		writeBufferSetIndex = 0;
		readBufferSetIndex = 0;

		for (auto& bufferSet : bufferSets)
		{
			bufferSet.dynamicParticlesBuffer.Allocate(
				dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count(),
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentCoherent
			);
			bufferSet.staticParticlesBuffer.Allocate(
				staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count()
			);
			bufferSet.dynamicParticleMap = (DynamicParticle*)bufferSet.dynamicParticlesBuffer.MapBufferRange(
				0, sizeof(DynamicParticle) * dynamicParticles.Count(),
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::None
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush |
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::Unsynchronized |
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::InvalidateBuffer 
			);
		}

		dynamicHashMap.Resize(initParams.hashesPerParticle * dynamicParticleCount);
		staticHashMap.Resize(initParams.hashesPerStaticParticle * dynamicParticleCount);
		particleMap.Resize(dynamicParticleCount);
		GenerateHashMap(staticParticles, staticHashMap, [gridSize = behaviourParameters.maxInteractionDistance, mod = staticHashMap.Count()](const StaticParticle& particle) {
			return GetHash(Vec3i(particle.position / gridSize)) % mod;
			});

		StartThreads();

		{
			std::unique_lock<std::mutex> lock{ mutex };
			RunThreadWork(lock, WorkType::CalculateHashAndParticleMap);
		}
		writeBufferSetIndex = (writeBufferSetIndex + 1) % bufferSets.Count();
	}
	void SystemCPU::Update(float deltaTime)
	{
		if (threadPool.ThreadCount() != 0)
		{
			bufferSets[writeBufferSetIndex].readFence.BlockClient(10);
			std::unique_lock<std::mutex> lock{ mutex };
			idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
			this->deltaTime = deltaTime;
			writeBufferSetIndex = (writeBufferSetIndex + 1) % bufferSets.Count();
			threadWorkType = WorkType::SimulateParticlesTimeStep;
			threadIdleCount = 0;
			idleCV.notify_all();
		}
		else
			SimulateParticlesTimeStep(0, dynamicParticleCount);
	}
	void SystemCPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{
		bufferSets[readBufferSetIndex].writeFence.BlockServer();
	}
	Graphics::OpenGLWrapper::VertexArray& SystemCPU::GetDynamicParticlesVertexArray()
	{
		return bufferSets[readBufferSetIndex].dynamicParticleVA;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemCPU::GetStaticParticlesVertexArray()
	{
		return bufferSets[readBufferSetIndex].staticParticleVA;
	}
	void SystemCPU::EndRender()
	{
		bufferSets[readBufferSetIndex].readFence.SetFence();
		readBufferSetIndex = (readBufferSetIndex + 1) % bufferSets.Count();
	}

	void SystemCPU::CalculateHashAndParticleMap(uintMem begin, uintMem end)
	{
		DynamicParticle* dynamicParticles = bufferSets[writeBufferSetIndex].dynamicParticleMap;

		for (uintMem i = begin; i < end; ++i)
		{
			Vec3f particlePosition = dynamicParticles[i].position;

			Vec3i cell = GetCell(particlePosition, behaviourParameters.maxInteractionDistance);
			uint particleHash = GetHash(cell) % dynamicHashMap.Count();

			dynamicParticles[i].hash = particleHash;			
			++dynamicHashMap[particleHash];
		}

		SyncThreads();

		if (begin == 0)
		{
			uint32 hashSum = 0;
			for (auto& hash : dynamicHashMap)
			{
				hashSum += hash;
				hash = hashSum;
			}
		}

		SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{
			float particleHash_FLOAT = dynamicParticles[i].hash;
			uint particleHash = *(uint*)&particleHash_FLOAT;

			uint index = --dynamicHashMap[particleHash];

			particleMap[index] = i;
		}
	}
	void SystemCPU::SimulateParticlesTimeStep(uintMem begin, uintMem end)
	{
		DynamicParticle* dynamicParticles = bufferSets[writeBufferSetIndex].dynamicParticleMap;

		for (uintMem i = begin; i < end; ++i)
		{
			auto* particlePtr = dynamicParticles + i;

			Vec3i cell = GetCell(particlePtr->position, behaviourParameters.maxInteractionDistance);

			Vec3i beginCell = cell - Vec3i(1, 1, 1);
			Vec3i endCell = cell + Vec3i(2, 2, 2);

			float influenceSum = 0.0f;

			Vec3i otherCell;
			for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
				for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
					for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
					{
						uint otherHash = GetHash(otherCell);

						//Calculating dynamic particle pressure
						uint otherHashMod = otherHash % dynamicHashMap.Count();
						uint beginIndex = dynamicHashMap[otherHashMod];
						uint endIndex = dynamicHashMap[otherHashMod + 1];

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
							const DynamicParticle* otherParticlePtr = dynamicParticles + particleMap[i];

							if (particlePtr == otherParticlePtr)
								continue;

#ifdef DEBUG_BUFFERS
							if (particlePtr < firstPtr || particlePtr >= behindPtr)
							{
								printf("Reading outside valid memory of particles in neighbour loop");
								continue;
							}
#endif					

							Vec3f dir = otherParticlePtr->position - particlePtr->position;
							float distSqr = dir.SqrLenght();

							if (distSqr > behaviourParameters.maxInteractionDistance * behaviourParameters.maxInteractionDistance)
								continue;

							float dist = sqrt(distSqr);

							influenceSum += SmoothingKernelD0(dist, behaviourParameters.maxInteractionDistance);
						}

						if (staticParticleCount == 0)
							continue;

						//Calculating static particle pressure
						otherHashMod = otherHash % staticHashMap.Count();
						beginIndex = staticHashMap[otherHashMod];
						endIndex = staticHashMap[otherHashMod + 1];

						for (uint i = beginIndex; i < endIndex; ++i)
						{
#ifdef DEBUG_BUFFERS
							if (i >= STATIC_PARTICLE_COUNT)
							{
								printf("Reading outside valid memory of staticParticleMap in neighbour loop. begin: %4d end: %4d hash: %4d", beginIndex, endIndex, otherHash);
								break;
							}
#endif

							StaticParticle* otherParticlePtr = staticParticles.Ptr() + i;

#ifdef DEBUG_BUFFERS
							//					if (particlePtr < staticParticles || particlePtr >= staticParticles + STATIC_PARTICLE_COUNT)
							//					{
							//						printf("Reading outside valid memory of static particles in neighbour loop");
							//						continue;
							//					}					
#endif

							Vec3f dir = otherParticlePtr->position - particlePtr->position;
							float distSqr = dir.SqrLenght();

							if (distSqr > behaviourParameters.maxInteractionDistance * behaviourParameters.maxInteractionDistance)
								continue;


							float dist = sqrt(distSqr);

							influenceSum += SmoothingKernelD0(dist, behaviourParameters.maxInteractionDistance);
						}
					}

			influenceSum *= smoothingKernelConstant;

			float particleDensity = selfDensity + influenceSum * behaviourParameters.particleMass;
			particlePtr->pressure = behaviourParameters.gasConstant * (particleDensity - behaviourParameters.restDensity);
		}

		SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{
			auto* particlePtr = dynamicParticles + i;

			Vec3i cell = GetCell(particlePtr->position, behaviourParameters.maxInteractionDistance);

			Vec3i beginCell = cell - Vec3i(1, 1, 1);
			Vec3i endCell = cell + Vec3i(2, 2, 2);

			Vec3f pressureForce = Vec3f(0, 0, 0);
			Vec3f viscosityForce = Vec3f(0, 0, 0);

			float particleDensity = particlePtr->pressure / behaviourParameters.gasConstant + behaviourParameters.restDensity;

			Vec3i otherCell;
			for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
				for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
					for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
					{
						uint otherHash = GetHash(otherCell);

						uint otherHashMod = otherHash % dynamicHashMap.Count();
						uint beginIndex = dynamicHashMap[otherHashMod];
						uint endIndex = dynamicHashMap[otherHashMod + 1];

						for (uint i = beginIndex; i < endIndex; ++i)
						{
							DynamicParticle* otherParticlePtr = dynamicParticles + particleMap[i];

							if (particlePtr == otherParticlePtr)
								continue;

							const DynamicParticle otherParticle = *otherParticlePtr;

							Vec3f dir = otherParticle.position - particlePtr->position;
							float distSqr = dir.SqrLenght();

							if (distSqr > behaviourParameters.maxInteractionDistance * behaviourParameters.maxInteractionDistance)
								continue;

							float dist = sqrt(distSqr);

							if (distSqr == 0 || dist == 0)
								dir = RandomDirection(i);
							else
								dir /= dist;

							//apply pressure force					
							pressureForce += dir * (particlePtr->pressure + otherParticle.pressure) * SmoothingKernelD1(dist, behaviourParameters.maxInteractionDistance);

							//apply viscosity force					
							viscosityForce += (otherParticle.velocity - particlePtr->velocity) * SmoothingKernelD2(dist, behaviourParameters.maxInteractionDistance);
						}

						if (staticParticleCount == 0)
							continue;

						otherHashMod = otherHash % staticHashMap.Count();
						beginIndex = staticHashMap[otherHashMod];
						endIndex = staticHashMap[otherHashMod + 1];

						for (uint i = beginIndex; i < endIndex; ++i)
						{
							const StaticParticle* otherParticlePtr = staticParticles.Ptr() + i;
							struct StaticParticle otherParticle = *otherParticlePtr;

							Vec3f dir = otherParticle.position - particlePtr->position;
							float distSqr = dir.SqrLenght();

							if (distSqr > behaviourParameters.maxInteractionDistance * behaviourParameters.maxInteractionDistance)
								continue;

							float dist = sqrt(distSqr);

							if (distSqr == 0 || dist == 0)
								dir = RandomDirection(i);
							else
								dir /= dist;

							//apply pressure force					
							pressureForce += dir * (fabs(particlePtr->pressure) * 4) * SmoothingKernelD1(dist, behaviourParameters.maxInteractionDistance);

							//apply viscosity force					
							viscosityForce += -particlePtr->velocity * SmoothingKernelD2(dist, behaviourParameters.maxInteractionDistance);
						}
					}

			pressureForce *= behaviourParameters.particleMass / (2 * particleDensity) * smoothingKernelConstant;
			viscosityForce *= behaviourParameters.viscosity * behaviourParameters.particleMass * smoothingKernelConstant;

			Vec3f particleForce = pressureForce + viscosityForce;
			Vec3f acceleration = particleForce / particleDensity + behaviourParameters.gravity;

			//Integrate
			particlePtr->velocity += acceleration * deltaTime;
			particlePtr->position += particlePtr->velocity * deltaTime;

			if (boundParameters.bounded)
			{
				if (boundParameters.boundedByWalls)
				{

					if (particlePtr->position.x < boundParameters.boxOffset.x) {
						particlePtr->position.x = boundParameters.boxOffset.x;
						particlePtr->velocity.x = -particlePtr->velocity.x * behaviourParameters.elasticity;
					}

					if (particlePtr->position.x >= boundParameters.boxOffset.x) {
						particlePtr->position.x = boundParameters.boxOffset.x - FLT_EPSILON;
						particlePtr->velocity.x = -particlePtr->velocity.x * behaviourParameters.elasticity;
					}

					if (particlePtr->position.z < boundParameters.boxOffset.z) {
						particlePtr->position.z = boundParameters.boxOffset.z;
						particlePtr->velocity.z = -particlePtr->velocity.z * behaviourParameters.elasticity;
					}

					if (particlePtr->position.z >= boundParameters.boxOffset.z + boundParameters.boxSize.z) {
						particlePtr->position.z = boundParameters.boxOffset.z + boundParameters.boxSize.z - FLT_EPSILON;
						particlePtr->velocity.z = -particlePtr->velocity.z * behaviourParameters.elasticity;
					}
				}

				if (boundParameters.boundedByRoof)
				{
					if (particlePtr->position.y >= boundParameters.boxOffset.y + boundParameters.boxSize.y)
					{
						particlePtr->position.y = boundParameters.boxOffset.y + boundParameters.boxSize.y - FLT_EPSILON;
						particlePtr->velocity.y = -particlePtr->velocity.y * behaviourParameters.elasticity;
					}
				}

				if (particlePtr->position.y < boundParameters.boxOffset.y)
				{
					particlePtr->position.y = boundParameters.boxOffset.y;
					particlePtr->velocity.y = -particlePtr->velocity.y * behaviourParameters.elasticity;
				}
			}
			cell = GetCell(particlePtr->position, behaviourParameters.maxInteractionDistance);
			particlePtr->hash = GetHash(cell) % dynamicHashMap.Count();
		}

		SyncThreads();

		if (begin == 0)
			bufferSets[writeBufferSetIndex].writeFence.SetFence();
		
		memset(dynamicHashMap.Ptr() + begin * hashesPerDynamicParticle, 0, (end - begin) * hashesPerDynamicParticle * sizeof(uint32));

		SyncThreads();

		for (uintMem i = begin; i < end; ++i)		
			++dynamicHashMap[dynamicParticles[i].hash];		

		SyncThreads();

		if (begin == 0)
		{
			uint32 hashSum = 0;
			for (auto& hash : dynamicHashMap)
			{
				hashSum += hash;
				hash = hashSum;
			}
		}

		SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{						
			uint index = --dynamicHashMap[dynamicParticles[i].hash];

			particleMap[index] = i;
		}

	}
	void SystemCPU::RunThreadWork(std::unique_lock<std::mutex>& lock, WorkType workType)
	{        
		idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
		threadWorkType = workType;
		threadIdleCount = 0;
		idleCV.notify_all();
	}
	void SystemCPU::WaitForAllThreadsIdle(std::unique_lock<std::mutex>& lock)
	{        
		idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
	}
	void SystemCPU::StopThreads()
	{
		if (threadPool.ThreadCount() == 0)
			return;

		if (!threadPool.IsAnyRunning())
			return;

		{
			std::unique_lock<std::mutex> lock{ mutex };
			idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
			threadWorkType = WorkType::Exit;
			threadIdleCount = 0;
			idleCV.notify_all();
		}
		
		if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
			Debug::Logger::LogWarning("Client", "Threads didn't exit on time");        

		threadIdleCount = threadPool.ThreadCount();
	}
	void SystemCPU::StartThreads()
	{
		if (threadPool.ThreadCount() == 0)
			return;

		StopThreads();

		threadIdleCount = 0;
		
		threadPool.RunTask(0, dynamicParticleCount, [this](uintMem begin, uintMem end) -> uint {
			SimulationThreadFunc(begin, end);
			return 0;
		});
	}
	void SystemCPU::SimulationThreadFunc(uintMem begin, uintMem end)           
	{
		bool exit = false;
		while (!exit)
		{
			{
				std::unique_lock<std::mutex> lock{ mutex };
				++threadIdleCount;
				idleCV.notify_all();
				idleCV.wait(lock, [&]() { return threadIdleCount == 0; });                
			}

			switch (threadWorkType)
			{
			case SPH::SystemCPU::WorkType::CalculateHashAndParticleMap:
				CalculateHashAndParticleMap(begin, end);
				break;
			case SPH::SystemCPU::WorkType::SimulateParticlesTimeStep:
				SimulateParticlesTimeStep(begin, end);				
				break;						
			case SPH::SystemCPU::WorkType::Exit:
				exit = true;
				break;
			}
		}
	}
	void SystemCPU::SyncThreads()
	{
		if (threadPool.ThreadCount() == 0)
			return;

		std::unique_lock<std::mutex> lock{ mutex };
		++threadSyncCount;		

		if (threadSyncCount == threadPool.ThreadCount())
		{ 
			threadSyncCount = 0;
			syncCV.notify_all();
		}
		else
		{
			syncCV.notify_all();
			syncCV.wait(lock, [&]() { return threadSyncCount == 0; });
		}
	}
}