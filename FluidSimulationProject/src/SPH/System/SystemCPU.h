#pragma once
#include "SPH/System/System.h"
#include "ThreadPool.h"
#include <condition_variable>

namespace SPH
{		
	class SystemCPU : public System
	{
	public:				
		SystemCPU(ThreadPool& threadPool);
		~SystemCPU();

		void Initialize(const SystemInitParameters& initParams) override;
		
		void Update(float dt) override;	
		
		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "CPU"; };

		void StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray();
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray();
		void EndRender();
	private:						
		enum class WorkType
		{
			CalculateHashAndParticleMap,
			SimulateParticlesTimeStep,			
			Exit
		};
		struct BufferSet
		{
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesBuffer;
			Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticlesBuffer;
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVA;
			Graphics::OpenGLWrapper::VertexArray staticParticleVA;
			Graphics::OpenGLWrapper::Fence readFence;
			Graphics::OpenGLWrapper::Fence writeFence;
			DynamicParticle* dynamicParticleMap;			
		};

		ThreadPool& threadPool;

		ParticleBehaviourParameters behaviourParameters;
		ParticleBoundParameters boundParameters;
		
		uintMem writeBufferSetIndex;
		uintMem readBufferSetIndex;
		uintMem dynamicParticleCount;
		uintMem staticParticleCount;		
		Array<BufferSet> bufferSets;
		Array<uint32> dynamicHashMap;	
		Array<uint32> staticHashMap;
		Array<uint32> particleMap;
		Array<StaticParticle> staticParticles;

		uintMem hashesPerDynamicParticle;

		float smoothingKernelConstant;
		float selfDensity;
		
		WorkType threadWorkType;
		std::condition_variable idleCV;
		std::condition_variable syncCV;
		std::mutex mutex;
		std::mutex syncMutex;
		uintMem threadIdleCount;
		uintMem threadSyncCount;

		float deltaTime;		
				
		void SimulateParticlesTimeStep(uintMem begin, uintMem end);
		void CalculateHashAndParticleMap(uintMem begin, uintMem end);		
		
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