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

	RenderableCPUParticleBufferManager::RenderableCPUParticleBufferManager() 
		: openGLThreadID(), currentBuffer(0), bufferSize(0), bufferGL(0)
	{
	}
	RenderableCPUParticleBufferManager::~RenderableCPUParticleBufferManager()
	{
		Clear();
	}
	void RenderableCPUParticleBufferManager::Clear()
	{				
		if (bufferGL.GetHandle() != 0)
		{
			bufferGL.UnmapBuffer();
			bufferGL.Release();
		}

		buffers.Clear();		
		bufferSize = 0;
		currentBuffer = 0;
	}
	void RenderableCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableCPUParticleBufferManager::Allocate(uintMem newBufferSize, void* ptr, uintMem bufferCount)
	{
		openGLThreadID = std::this_thread::get_id();

		Clear();

		if (bufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		buffers = Array<ParticlesBuffer>(bufferCount);
		bufferSize = newBufferSize;

		using namespace Graphics::OpenGLWrapper;
		//The OpenGL buffers aren't created so they need to be created
		bufferGL = ImmutableMappedGraphicsBuffer();
		bufferGL.Allocate(nullptr, bufferSize * bufferCount, GraphicsBufferMapAccessFlags::Read | GraphicsBufferMapAccessFlags::Write, GraphicsBufferMapType::PersistentUncoherent);

		void* map = bufferGL.MapBufferRange(0, bufferSize * bufferCount, GraphicsBufferMapOptions::ExplicitFlush);

		//Copy the initial particles to the first buffer, intentionally not flushing because it will be done when 'preparing' for rendering
		if (ptr != nullptr)
			memcpy(map, ptr, bufferSize);

		bufferGL.FlushBufferRange(0, bufferSize);

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].SetPointer((char*)map + bufferSize * i, true);
	}
	uintMem RenderableCPUParticleBufferManager::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem RenderableCPUParticleBufferManager::GetBufferSize()
	{
		return bufferSize;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableCPUParticleBufferManager::GetGraphicsBuffer(uintMem index, uintMem& bufferOffset)
	{
		bufferOffset = index * bufferSize;
		return &bufferGL;
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockRead(void* signalEvent)
	{
		return buffers[currentBuffer].LockRead();
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockWrite(void* signalEvent)
	{
		return buffers[currentBuffer].LockWrite(openGLThreadID == std::this_thread::get_id());
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockForRendering(void* signalEvent)
	{
		auto lockGuard = buffers[currentBuffer].LockForRendering(bufferGL, currentBuffer, bufferSize);

		CheckAllRenderingFences();

		return lockGuard;		
	}
	void RenderableCPUParticleBufferManager::PrepareForRendering()
	{
		auto lockGuard = buffers[currentBuffer].LockRead();
		buffers[currentBuffer].PrepareForRendering(bufferGL, currentBuffer, bufferSize);
		lockGuard.Unlock({ });
	}
	void RenderableCPUParticleBufferManager::FlushAllOperations()
	{
		for (auto& buffer : buffers)
			buffer.WaitRenderingFence();
	}
	void RenderableCPUParticleBufferManager::CheckAllRenderingFences()
	{
		for (auto& buffer : buffers)
			buffer.CheckRenderingFence();
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