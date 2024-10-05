#include "pch.h"
#include "SystemCPU.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{
	void TaskThreadContext::SyncThreads()
	{
		if (context.threadPool.ThreadCount() == 0)
			return;

		std::unique_lock<std::mutex> lock{ context.mutex };
		
		++context.threadSyncCount1;
		context.syncCV.notify_all();				
		context.syncCV.wait(lock, [&]() { return context.threadSyncCount1 % context.threadPool.ThreadCount() == 0; });				

		++context.threadSyncCount2;
		context.syncCV.notify_all();
		context.syncCV.wait(lock, [&]() { return context.threadSyncCount2 % context.threadPool.ThreadCount() == 0; });
	}
	TaskThreadContext::TaskThreadContext(ThreadContext& context) : context(context)
	{
	}
	ThreadContext::ThreadContext(ThreadPool& threadPool) :
		threadPool(threadPool), threadIdleCount(0), threadSyncCount1(0), exit(false), taskThreadContext(*this), begin(0), end(0), threadSyncCount2(0)
	{		
	}
	ThreadContext::~ThreadContext()
	{
		if (threadPool.ThreadCount() == 0)
			return;

		if (!threadPool.IsAnyRunning())
			return;

		{
			std::lock_guard lock{ mutex };			
			exit = true;			
			idleCV.notify_all();
		}

		if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
			Debug::Logger::LogWarning("Client", "Threads didn't exit on time");		
	}
	void ThreadContext::FinishTasks()
	{
		if (!threadPool.IsAnyRunning())
			return;

		std::unique_lock<std::mutex> lock{ mutex };
		idleCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
	}
	void ThreadContext::StartThreads(uintMem begin, uintMem end)
	{
		if (threadPool.IsAnyRunning())
		{
			{
				std::lock_guard lock{ mutex };
				exit = true;
				idleCV.notify_all();
			}

			if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
				Debug::Logger::LogWarning("Client", "Threads didn't exit on time");
		}

		this->begin = begin;
		this->end = end;
		threadIdleCount = 0;
		threadSyncCount1 = 0;
		threadSyncCount2 = 0;
		exit = false;

		if (threadPool.ThreadCount() == 0)
			return;

		threadPool.RunTask(begin, end, [this](uintMem begin, uintMem end) -> uint {
			SimulationThreadFunc(taskThreadContext, begin, end);
			return 0;
			});
	}
	void ThreadContext::StopThreads()
	{
		if (threadPool.IsAnyRunning())
		{
			{
				std::lock_guard lock{ mutex };
				exit = true;
				idleCV.notify_all();
			}

			if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
				Debug::Logger::LogWarning("Client", "Threads didn't exit on time");			
		}
	}
	void ThreadContext::EnqueueTask(TaskFunction taskFunction)
	{
		if (threadPool.ThreadCount() == 0)		
			taskFunction(taskThreadContext, begin, end);		
		else
		{
			std::lock_guard lk{ mutex };
			tasks.push(taskFunction);			
			idleCV.notify_all();
		}
	}	
	void ThreadContext::SimulationThreadFunc(TaskThreadContext& taskThreadContext, uintMem begin, uintMem end)
	{		
		while (true)
		{			
			std::unique_lock<std::mutex> lock{ mutex };
			
			++threadIdleCount;
			idleCV.notify_all();
			idleCV.wait(lock, [&]() { return exit || !tasks.empty(); });			
			--threadIdleCount;
			
			if (exit)
				break;

			TaskFunction task = tasks.front();
			
			lock.unlock();

			taskThreadContext.SyncThreads();			

			if (begin == 0)
			{
				std::lock_guard lg{ lock };				

				tasks.pop();				
			}

			task(taskThreadContext, begin, end);			
		}
	}

	SystemCPU::SystemCPU(ThreadPool& threadPool) :
		threadContext(threadPool)
	{
	}
	SystemCPU::~SystemCPU()
	{
		Clear();
	}
	void SystemCPU::Initialize(const SystemInitParameters& initParams)
	{				
		Clear();

		Array<DynamicParticle> dynamicParticles;
		initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);
		initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		dynamicParticleCount = dynamicParticles.Count();
		staticParticleCount = staticParticles.Count();

		bufferSets.Clear();
		bufferSets = Array<ParticleBufferSet>(std::max(initParams.bufferCount, 2Ui64));		

		renderBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 1;
		simulationReadBufferSetIndex = 0;

		for (auto& bufferSet : bufferSets)
		{
			bufferSet.dynamicParticlesBuffer.Allocate(
				dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count(),
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentCoherent
			);			
			bufferSet.dynamicParticleMap = (DynamicParticle*)bufferSet.dynamicParticlesBuffer.MapBufferRange(
				0, sizeof(DynamicParticle) * dynamicParticles.Count(),
				Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::None
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush |
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::Unsynchronized |
				//Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::InvalidateBuffer 
			);

			bufferSet.dynamicParticleVA.EnableVertexAttribute(0);
			bufferSet.dynamicParticleVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
			bufferSet.dynamicParticleVA.SetVertexAttributeBuffer(0, &bufferSet.dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
			bufferSet.dynamicParticleVA.SetVertexAttributeDivisor(0, 1);

			bufferSet.dynamicParticleVA.EnableVertexAttribute(1);
			bufferSet.dynamicParticleVA.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
			bufferSet.dynamicParticleVA.SetVertexAttributeBuffer(1, &bufferSet.dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
			bufferSet.dynamicParticleVA.SetVertexAttributeDivisor(1, 1);

			bufferSet.dynamicParticleVA.EnableVertexAttribute(2);
			bufferSet.dynamicParticleVA.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
			bufferSet.dynamicParticleVA.SetVertexAttributeBuffer(2, &bufferSet.dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
			bufferSet.dynamicParticleVA.SetVertexAttributeDivisor(2, 1);			

			bufferSet.writeFinished.test_and_set();
		}

		staticParticlesBuffer = decltype(staticParticlesBuffer)();
		staticParticlesBuffer.Allocate(
			staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count()
		);

		staticParticleVA.EnableVertexAttribute(0);
		staticParticleVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVA.SetVertexAttributeBuffer(0, &staticParticlesBuffer, sizeof(StaticParticle), 0);
		staticParticleVA.SetVertexAttributeDivisor(0, 1);

		dynamicParticleHashMapSize = initParams.hashesPerDynamicParticle * dynamicParticleCount ;
		staticParticleHashMapSize = initParams.hashesPerStaticParticle * staticParticleCount;

		dynamicParticleReadHashMapBuffer.Resize(dynamicParticleHashMapSize + 1);
		dynamicParticleWriteHashMap.Resize(dynamicParticleHashMapSize + 1);
		staticParticleHashMap.Resize(staticParticleHashMapSize + 1);
		memset(dynamicParticleReadHashMapBuffer.Ptr(), 0, sizeof(uint32) * dynamicParticleReadHashMapBuffer.Count());
		dynamicParticleReadHashMapBuffer.Last() = dynamicParticleCount;
		memset(dynamicParticleWriteHashMap.Ptr(), 0, sizeof(uint32) * dynamicParticleWriteHashMap.Count());
		dynamicParticleWriteHashMap.Last() = dynamicParticleCount;
		memset(staticParticleHashMap.Ptr(), 0, sizeof(uint32) * staticParticleHashMap.Count());		

		particleMap.Resize(dynamicParticleCount);
				
		simulationParameters.behaviour = initParams.particleBehaviourParameters;
		simulationParameters.bounds = initParams.particleBoundParameters;
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleReadHashMapBuffer.Count() - 1;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMap.Count() - 1;
		
		threadContext.StartThreads(0, dynamicParticleCount);		

		auto GetStaticParticleHash = [gridSize = initParams.particleBehaviourParameters.maxInteractionDistance, mod = staticParticleHashMapSize](const StaticParticle& particle) {
			return GetHash(Vec3i(particle.position / gridSize)) % mod;
			};
		staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticleHashMap, GetStaticParticleHash);		
		
		CalculateHashAndParticleMapTask task{
			.particles = bufferSets[0].dynamicParticleMap,
			.simulationParameters = &simulationParameters,
			.hashMap = dynamicParticleReadHashMapBuffer.Ptr(),
			.particleMap = particleMap.Ptr(),
		};
		threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem begin, uintMem end) {
			CalculateHashAndParticleMap(context, begin, end, task);
			});		
	}
	void SystemCPU::Clear()
	{
		threadContext.StopThreads();

		dynamicParticleCount = 0;
		dynamicParticleHashMapSize = 0;
		staticParticleCount = 0;
		staticParticleHashMapSize = 0;

		bufferSets.Clear();
		dynamicParticleReadHashMapBuffer.Clear();
		dynamicParticleWriteHashMap.Clear();
		particleMap.Clear();
		staticParticles.Clear();
		staticParticleHashMap.Clear();

		staticParticlesBuffer = decltype(staticParticlesBuffer)();
		staticParticleVA = decltype(staticParticleVA)();

		simulationWriteBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		renderBufferSetIndex = 0;

		simulationParameters = { };
	}
	void SystemCPU::Update(float deltaTime)
	{
		SimulateParticlesTimeStepTask task{
			.readParticles = bufferSets[simulationReadBufferSetIndex].dynamicParticleMap,
			.writeParticles = bufferSets[simulationWriteBufferSetIndex].dynamicParticleMap,
			.dynamicParticleReadHashMapBuffer = dynamicParticleReadHashMapBuffer.Ptr(),
			.dynamicParticleWriteHashMap = dynamicParticleWriteHashMap.Ptr(),
			.particleMap = particleMap.Ptr(),
			.staticParticles = staticParticles.Ptr(),
			.staticParticleHashMap = staticParticleHashMap.Ptr(),
			.simulationParameters = &simulationParameters,			
			.readStarted = &bufferSets[simulationWriteBufferSetIndex].readStarted,
			.readFinished = &bufferSets[simulationWriteBufferSetIndex].readFinished,
			.writeFinished = &bufferSets[simulationWriteBufferSetIndex].writeFinished,
			.dt = deltaTime,
		};
		threadContext.EnqueueTask([task](TaskThreadContext& context, uintMem begin, uintMem end) {
			SimulateParticlesTimeStep(context, begin, end, task);
			});

		std::swap(dynamicParticleReadHashMapBuffer, dynamicParticleWriteHashMap);
		
		simulationReadBufferSetIndex = (simulationReadBufferSetIndex + 1) % bufferSets.Count();
		simulationWriteBufferSetIndex = (simulationWriteBufferSetIndex + 1) % bufferSets.Count();		
	}
	void SystemCPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{
		bufferSets[renderBufferSetIndex].readStarted.test_and_set();
		bufferSets[renderBufferSetIndex].writeFinished.wait(false);				
		renderBufferSetIndex = simulationReadBufferSetIndex;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemCPU::GetDynamicParticlesVertexArray()
	{
		return bufferSets[renderBufferSetIndex].dynamicParticleVA;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemCPU::GetStaticParticlesVertexArray()
	{
		return staticParticleVA;
	}
	void SystemCPU::EndRender()
	{		
		bufferSets[renderBufferSetIndex].readFinished.SetFence();		
	}	
	void SystemCPU::CalculateHashAndParticleMap(TaskThreadContext& context, uintMem begin, uintMem end, CalculateHashAndParticleMapTask task)
	{
		for (uintMem i = begin; i < end; ++i)
		{
			Vec3f particlePosition = task.particles[i].position;

			Vec3i cell = GetCell(particlePosition, task.simulationParameters->behaviour.maxInteractionDistance);
			uint particleHash = GetHash(cell) % task.simulationParameters->dynamicParticleHashMapSize;

			task.particles[i].hash = particleHash;		
			++task.hashMap[particleHash];
		}

		context.SyncThreads();

		if (begin == 0)
		{
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.simulationParameters->dynamicParticleHashMapSize; ++i)
			{
				valueSum += task.hashMap[i];
				task.hashMap[i] = valueSum;
			}
		}

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{
			uint index = --task.hashMap[task.particles[i].hash];
			task.particleMap[index] = i;
		}		
	}
	void SystemCPU::SimulateParticlesTimeStep(TaskThreadContext& context, uintMem begin, uintMem end, SimulateParticlesTimeStepTask task)
	{		
		if (begin == 0)
		{
			if (task.readStarted->test())
			{
				task.readFinished->BlockClient(10);				
				task.readFinished->Clear();
				task.readStarted->clear();
			}
			task.writeFinished->clear();
		}

		context.SyncThreads();
		
		for (uintMem i = begin; i < end; ++i)
			UpdateParticlePressure(
				i,
				task.readParticles,
				task.writeParticles,
				task.dynamicParticleReadHashMapBuffer,
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				task.simulationParameters
			);

		if (begin == 0)		
			memset(task.dynamicParticleWriteHashMap, 0, task.simulationParameters->dynamicParticleHashMapSize * sizeof(uint32));

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
			UpdateParticleDynamics(
				i,
				task.readParticles,
				task.writeParticles,
				task.dynamicParticleReadHashMapBuffer,
				task.dynamicParticleWriteHashMap,
				task.particleMap,
				task.staticParticles,
				task.staticParticleHashMap,
				task.dt,
				task.simulationParameters
			);		

		context.SyncThreads();
		

		if (begin == 0)
		{			
			task.writeFinished->test_and_set();
			task.writeFinished->notify_all();
					
			uint32 valueSum = 0;
			for (uintMem i = 0; i < task.simulationParameters->dynamicParticleHashMapSize; ++i)
			{
				valueSum += task.dynamicParticleWriteHashMap[i];
				task.dynamicParticleWriteHashMap[i] = valueSum;
			}
		}

		context.SyncThreads();

		for (uintMem i = begin; i < end; ++i)
		{						
			uint index = --task.dynamicParticleWriteHashMap[task.writeParticles[i].hash];
			task.particleMap[index] = i;
		}		
	}			
}