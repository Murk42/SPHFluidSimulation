#pragma once
#include "SPHSystem.h"
#include "ThreadPool.h"

namespace SPH
{		

	class SystemCPU : public System
	{
	public:				
		SystemCPU(ThreadPool& threadPool);
		~SystemCPU();

		void Initialize(const SystemInitParameters& initParams) override;
		
		void Update(float dt) override;	
		
		void ApplyForceToArea(Vec3f position, Vec3f force);		

		StringView SystemImplementationName() override { return "CPU"; };
	private:						
		float particleMass;
		float gasConstant;
		float viscosity;		
		float elasticity;

		float restDensity;
		Vec3f boundingBoxSize;		
				
		float smoothingKernelConstant;
		float selfDensity;
		
		enum class WorkType
		{
			CalculateDensityAndPressure,
			CalculateForces,
			CalculatePositions,
			All,
			Exit
		};
		ThreadPool& threadPool;
		WorkType threadWorkType;
		std::condition_variable idleCV;
		std::condition_variable syncCV;
		std::mutex mutex;
		std::mutex syncMutex;
		uintMem threadIdleCount;
		uintMem threadSyncCount;

		float deltaTime;		
				
		void CalculateDensityAndPressure(uintMem begin, uintMem end);
		void CalculateForces(uintMem begin, uintMem end);
		void CalculatePositions(uintMem begin, uintMem end);
		void UpdateParticles(float deltaTime);
		
		//Functions for manipulation simulation threads
		void RunThreadWork(std::unique_lock<std::mutex>& lock, WorkType workType);
		void WaitForAllThreadsIdle(std::unique_lock<std::mutex>& lock);
		void StopThreads();
		void StartThreads();
		void SimulationThreadFunc(uintMem begin, uintMem end);

		//Functions for simulation threads
		void SyncThreads();
	};		
}