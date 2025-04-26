#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{
	static void WaitForAndClearFence(Graphics::OpenGLWrapper::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);
		switch (fenceState)
		{
		case Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
		case Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
		case Graphics::OpenGLWrapper::FenceReturnState::FenceNotSet:
			break;
		case Graphics::OpenGLWrapper::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;
		case Graphics::OpenGLWrapper::FenceReturnState::Error:
			Debug::Logger::LogFatal("Client", "System simulation fence error");
			break;
		}

		fence.Clear();
	}

	RenderableCPUParticleBufferManager::RenderableCPUParticleBufferManager() :
		currentBuffer(0), openGLThreadID(),
		dynamicParticlesCount(0), dynamicParticlesMemory(0), dynamicParticlesBuffers(3),
		staticParticlesCount(0), staticParticlesMemory(0), staticParticlesBuffer()
	{
	}
	RenderableCPUParticleBufferManager::~RenderableCPUParticleBufferManager()
	{
		Clear();
	}
	void RenderableCPUParticleBufferManager::Clear()
	{		
		ClearDynamicParticlesBuffers();		
		ClearStaticParticlesBuffer();
	}
	void RenderableCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % dynamicParticlesBuffers.Count();
	}
	void RenderableCPUParticleBufferManager::AllocateDynamicParticles(uintMem count, DynamicParticle* particles)
	{
		openGLThreadID = std::this_thread::get_id();

		ClearDynamicParticlesBuffers();

		if (count == 0)		
			return;

		//Set current buffer to 0 because the initial particles are going to be copied to that buffer
		currentBuffer = 0;
		dynamicParticlesCount = count;
		
		//The OpenGL buffers aren't created so they need to be created
		dynamicParticlesMemory = decltype(dynamicParticlesMemory)();

		dynamicParticlesMemory.Allocate(nullptr, sizeof(DynamicParticle) * count * dynamicParticlesBuffers.Count(),
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);

		DynamicParticle* ptr = (DynamicParticle*)dynamicParticlesMemory.MapBufferRange(0, sizeof(DynamicParticle) * count * dynamicParticlesBuffers.Count(), Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);
		
		//Copy the initial particles to the first buffer, intentionally not flushing because it will be done when 'preparing' for rendering
		if (particles != nullptr)
			memcpy(ptr, particles, sizeof(DynamicParticle) * count);

		for (uintMem i = 0; i < dynamicParticlesBuffers.Count(); ++i)
			dynamicParticlesBuffers[i].SetPointer(ptr + count * i, false); 
	}
	void RenderableCPUParticleBufferManager::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		openGLThreadID = std::this_thread::get_id();

		ClearStaticParticlesBuffer();

		if (count == 0)		
			return;				

		staticParticlesCount = count;
		
		//The OpenGL buffers aren't created so they need to be created
		staticParticlesMemory = decltype(staticParticlesMemory)();

		staticParticlesMemory.Allocate(particles, sizeof(StaticParticle) * count,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);

		void* ptr = (StaticParticle*)staticParticlesMemory.MapBufferRange(0, sizeof(StaticParticle) * count, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		//The buffer is prepared for rendering because there isn't a need to flush the buffer. It's been initialized with the particles
		staticParticlesBuffer.SetPointer(ptr, true);		
	}
	uintMem RenderableCPUParticleBufferManager::GetDynamicParticleBufferCount() const
	{
		return dynamicParticlesBuffers.Count();
	}
	uintMem RenderableCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticlesCount;
	}
	uintMem RenderableCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticlesCount;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableCPUParticleBufferManager::GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(DynamicParticle);
		bufferOffset = index * dynamicParticlesCount * sizeof(DynamicParticle);
		return &dynamicParticlesMemory;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableCPUParticleBufferManager::GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(StaticParticle);
		bufferOffset = 0;
		return &staticParticlesMemory;
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesForRead(void* signalEvent)
	{		
		return dynamicParticlesBuffers[currentBuffer].LockRead();
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesForWrite(void* signalEvent)
	{
		return dynamicParticlesBuffers[currentBuffer].LockWrite(openGLThreadID == std::this_thread::get_id());
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockStaticParticlesForRead(void* signalEvent)
	{	
		return staticParticlesBuffer.LockRead();	
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockStaticParticlesForWrite(void* signalEvent)
	{
		return staticParticlesBuffer.LockWrite(openGLThreadID == std::this_thread::get_id());
	}	
	ResourceLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesForRendering(void* signalEvent)
	{
		auto lockGuard = dynamicParticlesBuffers[currentBuffer].LockForRendering(dynamicParticlesMemory, currentBuffer, sizeof(DynamicParticle) * dynamicParticlesCount);

		CheckAllRenderingFences();

		return lockGuard;
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockStaticParticlesForRendering(void* signalEvent)
	{						
		auto lockGuard = staticParticlesBuffer.LockForRendering(staticParticlesMemory, 0, sizeof(StaticParticle) * staticParticlesCount);		

		CheckAllRenderingFences();

		return lockGuard;
	}
	void RenderableCPUParticleBufferManager::PrepareDynamicParticlesForRendering()
	{	
		auto lockGuard = dynamicParticlesBuffers[currentBuffer].LockRead();
		dynamicParticlesBuffers[currentBuffer].PrepareForRendering(dynamicParticlesMemory, currentBuffer, sizeof(DynamicParticle) * dynamicParticlesCount);
		lockGuard.Unlock({ });
	}
	void RenderableCPUParticleBufferManager::PrepareStaticParticlesForRendering() 
	{	
		auto lockGuard = staticParticlesBuffer.LockRead();
		staticParticlesBuffer.PrepareForRendering(staticParticlesMemory, 0, sizeof(StaticParticle) * staticParticlesCount);
		lockGuard.Unlock({ });
	}
	void RenderableCPUParticleBufferManager::FlushAllOperations()
	{
		for (auto& buffer : dynamicParticlesBuffers)
			buffer.WaitRenderingFence();
		staticParticlesBuffer.WaitRenderingFence();
	}
	void RenderableCPUParticleBufferManager::CheckAllRenderingFences()
	{
		for (auto& buffer : dynamicParticlesBuffers)
			buffer.CheckRenderingFence();

		staticParticlesBuffer.CheckRenderingFence();
	}
	void RenderableCPUParticleBufferManager::ClearDynamicParticlesBuffers()
	{				
		Array<ResourceLockGuard> lockGuards;
		lockGuards.ReserveExactly(dynamicParticlesBuffers.Count());

		for (auto& buffer : dynamicParticlesBuffers)
			lockGuards.AddBack(buffer.LockWrite(true));

		if (dynamicParticlesMemory.GetHandle() != 0)
		{
			dynamicParticlesMemory.UnmapBuffer();
			dynamicParticlesMemory.Release();
		}

		for (auto& buffer : dynamicParticlesBuffers)
			buffer.SetPointer(nullptr, false);		

		dynamicParticlesCount = 0;

		for (auto& lockGuard : lockGuards)
			lockGuard.Unlock({});
	}
	void RenderableCPUParticleBufferManager::ClearStaticParticlesBuffer()
	{		
		ResourceLockGuard lockGuard = staticParticlesBuffer.LockWrite(true);

		if (staticParticlesMemory.GetHandle() != 0)
		{
			staticParticlesMemory.UnmapBuffer();
			staticParticlesMemory.Release();
		}

		staticParticlesBuffer.SetPointer(nullptr, false);
		staticParticlesCount = 0;

		lockGuard.Unlock({ });
	}

	RenderableCPUParticleBufferManager::ParticlesBuffer::ParticlesBuffer()
		: ptr(nullptr), renderingFenceFlag(true), preparedForRendering(false)
	{
	}	
	void RenderableCPUParticleBufferManager::ParticlesBuffer::SetPointer(void* ptr, bool preparedForRendering)
	{
		this->ptr = ptr;
		this->preparedForRendering = preparedForRendering;
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::ParticlesBuffer::LockRead()
	{
		lock.LockRead();

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((CPULock*)userData)->UnlockRead();
			}, ptr, &lock);
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::ParticlesBuffer::LockWrite(bool isOpenGLThread)
	{
		if (isOpenGLThread)
		{
			lock.LockWrite();
			WaitForAndClearFence(renderingFence);			
			renderingFenceFlag = true;
			lock.NotifyAll();
		}
		else		
			lock.LockWrite([&]() { return renderingFenceFlag; });
		
		preparedForRendering = false;

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((CPULock*)userData)->UnlockWrite();
			}, ptr, &lock);		
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::ParticlesBuffer::LockForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize)
	{		
		lock.LockRead();

		if (!preparedForRendering)
			PrepareForRendering(buffer, index, bufferSize);

		renderingFenceFlag = false;

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((ParticlesBuffer*)userData)->renderingFence.SetFence();			
			((ParticlesBuffer*)userData)->lock.UnlockRead();
			}, (void*)index, this);
	}
	void RenderableCPUParticleBufferManager::ParticlesBuffer::CheckRenderingFence()
	{
		if (!renderingFence.IsSet() || renderingFence.IsSignaled())
		{
			renderingFenceFlag = true;
			lock.NotifyAll();
		}
	}
	void RenderableCPUParticleBufferManager::ParticlesBuffer::WaitRenderingFence()
	{
		//This doesn't need to be under a write lock because all other write locks will wait until the fence is waited on and notified
		WaitForAndClearFence(renderingFence);
		renderingFenceFlag = true;
		lock.NotifyAll();
	}
	void RenderableCPUParticleBufferManager::ParticlesBuffer::PrepareForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize)
	{
		if (preparedForRendering || bufferSize == 0)
			return;

		buffer.FlushBufferRange(bufferSize * index, bufferSize);
		preparedForRendering = true;
	}
}