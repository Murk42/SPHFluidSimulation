#include "pch.h"
#include "SPHSystemCPU.h"

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
		particleMass = initParams.particleMass;
		gasConstant = initParams.gasConstant;
		viscosity = initParams.viscosity;
		elasticity = initParams.elasticity;

		restDensity = initParams.restDensity;
		boundingBoxSize = initParams.boundingBoxSize;
		maxInteractionDistance = initParams.maxInteractionDistance;                

		smoothingKernelConstant = SmoothingKernelConstant(this->maxInteractionDistance);                
		selfDensity = particleMass * SmoothingKernelD0(0, maxInteractionDistance, smoothingKernelConstant);                   

		GeneratePointsInGrid(initParams.dynamicParticleGenerationParameters);        
		CalculateParticleHashes();
		GenerateHashMap();

		StartThreads();        		

		{
			std::unique_lock<std::mutex> lock{ mutex };
			RunThreadWork(lock, WorkType::CalculateDensityAndPressure);
			WaitForAllThreadsIdle(lock);
		}
	}
	void SystemCPU::Update(float dt)
	{
		UpdateParticles(dt);
	}
	void SystemCPU::ApplyForceToArea(Vec3f position, Vec3f force)
	{
		//neighborTable.ForEachposition, maxInteractionDistance, [&](const NeighborData& data) {            
		//	data.particle->force += force * SmoothingKernelD0(std::sqrt(data.sqrDistance), maxInteractionDistance, smoothingKernelConstant);
		//	});
	}
	void SystemCPU::CalculateDensityAndPressure(uintMem begin, uintMem end)
	{
		for (uintMem i = begin; i < end; ++i)
		{
			auto& particle = particleData[i];

			float influenceSum = 0.0f;

			ForEachNeighbor(particle.position, [&](NeighborData data) {

				if (data.particle == &particle)
					return 0;

				float distance = std::sqrt(data.sqrDistance);
				influenceSum += SmoothingKernelD0(distance, maxInteractionDistance, smoothingKernelConstant);
				});

			particle.density = selfDensity + influenceSum * particleMass;
			particle.pressure = gasConstant * (particle.density - restDensity);
		}
	}
	void SystemCPU::CalculateForces(uintMem begin, uintMem end)
	{
		for (uintMem i = begin; i < end; ++i)
		{
			auto& particle = particleData[i];

			Vec3f pressureForce = Vec3f();
			Vec3f viscosityForce = Vec3f();
		
			ForEachNeighbor(particle.position, [&](NeighborData data) {

				if (data.particle == &particle)
					return;

				//unit direction and length
				Vec3f dir = data.particle->position - particle.position;
				float distSqr = dir.SqrLenght();

				float dist = 0;
				if (distSqr == 0)
				{
					float theta = Random::Float() * 2 * 3.1415;
					float z = Random::Float() * 2 - 1;

					float s = sin(theta);
					float c = cos(theta);
					float z2 = sqrt(1 - z * z);
					dir = Vec3f(z2 * c, z2 * s, z);
				}
				else
				{
					dist = sqrt(distSqr);
					dir /= dist;
				}

				//apply pressure force
				Vec3f dPressureForce;
				dPressureForce = dir * (particle.pressure + data.particle->pressure);
				dPressureForce *= SmoothingKernelD1(dist, maxInteractionDistance, smoothingKernelConstant);
				pressureForce += dPressureForce;

				//apply viscosity force                
				Vec3f dViscosityForce;
				dViscosityForce = (data.particle->velocity - particle.velocity) / data.particle->density;
				dViscosityForce *= SmoothingKernelD2(dist, maxInteractionDistance, smoothingKernelConstant);
				viscosityForce += dViscosityForce;				
				});

			pressureForce *= particleMass / (2 * particle.density);

			viscosityForce *= viscosity * particleMass;

			particle.force = pressureForce + viscosityForce;

		}
	}

	void SystemCPU::CalculatePositions(uintMem begin, uintMem end)
	{
		for (uintMem i = begin; i < end; ++i)
		{
			auto& particle = particleData[i];

			//calculate acceleration and velocity
			Vec3f acceleration = particle.force / particle.density +Vec3f(0, -9.81, 0);			
			particle.velocity += acceleration * deltaTime;

			// Update position
			particle.position += particle.velocity * deltaTime;            


			Vec3f minPos = Vec3f();
			Vec3f maxPos = boundingBoxSize;

			// Handle collisions with box
			if (particle.position.y < minPos.y) 
			{
				particle.position.y = minPos.y;
				particle.velocity.y = -particle.velocity.y * elasticity;
			}

			if (particle.position.y >= maxPos.y)
			{
				particle.position.y = maxPos.y - FLT_EPSILON;
				particle.velocity.y = -particle.velocity.y * elasticity;
			}

			if (particle.position.x < minPos.x) {
				particle.position.x = minPos.x;
				particle.velocity.x = -particle.velocity.x * elasticity;
			}

			if (particle.position.x >= maxPos.x) {
				particle.position.x = maxPos.x - FLT_EPSILON;
				particle.velocity.x = -particle.velocity.x * elasticity;
			}

			if (particle.position.z < minPos.z) {
				particle.position.z = minPos.z;
				particle.velocity.z = -particle.velocity.z * elasticity;
			}

			if (particle.position.z >= maxPos.z) {
				particle.position.z = maxPos.z - FLT_EPSILON;
				particle.velocity.z = -particle.velocity.z * elasticity;
			}
		}
	}
	void SystemCPU::UpdateParticles(float deltaTime)
	{     
		this->deltaTime = deltaTime;
		if (threadPool.ThreadCount() != 0)
		{
			std::unique_lock<std::mutex> lock{ mutex };
			RunThreadWork(lock, WorkType::All);
			WaitForAllThreadsIdle(lock);
		}
		else
		{
			CalculateDensityAndPressure(0, particleData.Count());
			CalculateForces(0, particleData.Count());
			CalculatePositions(0, particleData.Count());
		}

		CalculateParticleHashes();
		GenerateHashMap();
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
		
		threadPool.RunTask(0, particleData.Count(), [this](uintMem begin, uintMem end) -> uint {
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
			case SPH::SystemCPU::WorkType::CalculateDensityAndPressure:
				CalculateDensityAndPressure(begin, end);
				break;
			case SPH::SystemCPU::WorkType::CalculateForces:
				CalculateForces(begin, end);
				break;
			case SPH::SystemCPU::WorkType::CalculatePositions:
				CalculatePositions(begin, end);
				break;
			case SPH::SystemCPU::WorkType::All: {
				CalculateDensityAndPressure(begin, end);

				SyncThreads();
				
				CalculateForces(begin, end);

				SyncThreads();

				CalculatePositions(begin, end);

				break;
			}
			case SPH::SystemCPU::WorkType::Exit:
				exit = true;
				break;
			}
		}
	}
	void SystemCPU::SyncThreads()
	{
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