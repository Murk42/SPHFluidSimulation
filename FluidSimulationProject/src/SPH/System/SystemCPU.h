#pragma once
#include "SPH/System/System.h"
#include "ThreadPool.h"
#include <condition_variable>
#include <queue>
#include <functional>

namespace SPH
{		
	class ThreadContext;

	class TaskThreadContext
	{
	public:
		void SyncThreads();
	private:
		TaskThreadContext(ThreadContext& context);
		ThreadContext& context;

		friend class ThreadContext;
	};

	class ThreadContext
	{
	public:
		using TaskFunction = std::function<void(TaskThreadContext&, uintMem, uintMem)>;
		ThreadContext(ThreadPool& threadPool);
		~ThreadContext();

		void FinishTasks();
		void StartThreads(uintMem begin, uintMem end);		
		void StopThreads();
		void EnqueueTask(TaskFunction function);
	private:
		TaskThreadContext taskThreadContext;
		ThreadPool& threadPool;
		std::condition_variable idleCV;
		std::condition_variable syncCV;
		std::mutex mutex;
		std::mutex syncMutex;
		uintMem threadIdleCount;
		uintMem threadSyncCount1;
		uintMem threadSyncCount2;		

		uintMem begin, end;
		bool exit;		
		std::queue<TaskFunction> tasks;
		
		void SimulationThreadFunc(TaskThreadContext&, uintMem begin, uintMem end);

		friend class TaskThreadContext;
	};

	class SystemCPU : public System
	{
	public:				
		SystemCPU(ThreadPool& threadPool);
		~SystemCPU();

		void Initialize(const SystemInitParameters& initParams) override;
		void Clear();
		
		void Update(float dt) override;	
		
		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "CPU"; };

		void StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray();
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray();
		void EndRender();
	private:								
		struct ParticleBufferSet
		{
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesBuffer;			
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVA;
			std::atomic_flag readStarted;
			Graphics::OpenGLWrapper::Fence readFinished;
			std::atomic_flag writeFinished;
			DynamicParticle* dynamicParticleMap;
		};

		ThreadContext threadContext;				
		
		uintMem dynamicParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleCount;		
		uintMem staticParticleHashMapSize;

		Array<ParticleBufferSet> bufferSets;
		Array<uint32> dynamicParticleReadHashMapBuffer;	
		Array<uint32> dynamicParticleWriteHashMap;
		Array<uint32> particleMap;
		Array<StaticParticle> staticParticles;
		Array<uint32> staticParticleHashMap;

		Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticlesBuffer;
		Graphics::OpenGLWrapper::VertexArray staticParticleVA;

		uintMem simulationWriteBufferSetIndex;
		uintMem simulationReadBufferSetIndex;
		uintMem renderBufferSetIndex;				
		
		ParticleSimulationParameters simulationParameters;

		struct CalculateHashAndParticleMapTask
		{
			DynamicParticle* particles;
			ParticleSimulationParameters* simulationParameters;
			uint32* hashMap;
			uint32* particleMap;
		};
		struct SimulateParticlesTimeStepTask
		{
			DynamicParticle* readParticles;
			DynamicParticle* writeParticles;
			uint32* dynamicParticleReadHashMapBuffer;
			uint32* dynamicParticleWriteHashMap;
			uint32* particleMap;
			StaticParticle* staticParticles;
			uint32* staticParticleHashMap;
			ParticleSimulationParameters* simulationParameters;
			
			std::atomic_flag* readStarted;
			Graphics::OpenGLWrapper::Fence* readFinished;
			std::atomic_flag* writeFinished;

			float dt;
		};
				
		static void CalculateHashAndParticleMap(TaskThreadContext& context, uintMem begin, uintMem end, CalculateHashAndParticleMapTask task);
		static void SimulateParticlesTimeStep(TaskThreadContext& context, uintMem begin, uintMem end, SimulateParticlesTimeStepTask task);
	};		
}