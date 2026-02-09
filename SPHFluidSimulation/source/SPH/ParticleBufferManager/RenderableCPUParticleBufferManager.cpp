#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{
	static void WaitForAndClearFence(Graphics::OpenGL::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);
		switch (fenceState)
		{
		case Graphics::OpenGL::FenceReturnState::AlreadySignaled:
		case Graphics::OpenGL::FenceReturnState::ConditionSatisfied:
		case Graphics::OpenGL::FenceReturnState::FenceNotSet:
			break;
		case Graphics::OpenGL::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;
		case Graphics::OpenGL::FenceReturnState::Error:
			Debug::Logger::LogFatal("Client", "System simulation fence error");
			break;
		}

		fence.Clear();
	}

	RenderableCPUParticleBufferManager::RenderableCPUParticleBufferManager()
		: openGLThreadID(), currentBuffer(0), particleSize(0), particleCount(0), bufferGL(0)
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
		particleSize = 0;
		particleCount = 0;
		currentBuffer = 0;
	}
	void RenderableCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableCPUParticleBufferManager::Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount)
	{
		openGLThreadID = std::this_thread::get_id();

		Clear();

		if (newBufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		buffers = Array<ParticlesBuffer>(newBufferCount);
		particleSize = newParticleSize;
		particleCount = newParticleCount;

		uintMem bufferSize = particleSize * particleCount;

		using namespace Graphics::OpenGL;
		//The OpenGL buffers aren't created so they need to be created
		bufferGL = ImmutableMappedGraphicsBuffer();
		bufferGL.Allocate(nullptr, bufferSize * newBufferCount, GraphicsBufferMapAccessFlags::Read | GraphicsBufferMapAccessFlags::Write, GraphicsBufferMapType::PersistentUncoherent);

		void* map = bufferGL.MapBufferRange(0, bufferSize * newBufferCount, GraphicsBufferMapOptions::ExplicitFlush);

		//Copy the initial particles to the first buffer, intentionally not flushing because it will be done when 'preparing' for rendering
		if (particles != nullptr)
			memcpy(map, particles, bufferSize);

		bufferGL.FlushBufferRange(0, bufferSize);

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].SetPointer((char*)map + bufferSize * i, true);
	}
	uintMem RenderableCPUParticleBufferManager::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem RenderableCPUParticleBufferManager::GetParticleCount() const
	{
		return particleCount;
	}
	uintMem RenderableCPUParticleBufferManager::GetParticleSize() const
	{
		return particleSize;
	}
	Graphics::OpenGL::GraphicsBuffer* RenderableCPUParticleBufferManager::GetGraphicsBuffer(uintMem index, uintMem& bufferOffset)
	{
		bufferOffset = index * particleCount * particleSize;
		return &bufferGL;
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockRead(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockRead();
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockWrite(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockWrite(openGLThreadID == std::this_thread::get_id());
	}
	ResourceLockGuard RenderableCPUParticleBufferManager::LockForRendering(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		auto lockGuard = buffers[currentBuffer].LockForRendering(bufferGL, currentBuffer, particleCount * particleSize);

		CheckAllRenderingFences();

		return lockGuard;
	}
	void RenderableCPUParticleBufferManager::PrepareForRendering()
	{
		if (buffers.Empty())
			return;

		auto lockGuard = buffers[currentBuffer].LockRead();
		buffers[currentBuffer].PrepareForRendering(bufferGL, currentBuffer, particleCount * particleSize);
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
	ResourceLockGuard RenderableCPUParticleBufferManager::ParticlesBuffer::LockForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize)
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
	void RenderableCPUParticleBufferManager::ParticlesBuffer::PrepareForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize)
	{
		if (preparedForRendering || bufferSize == 0)
			return;

		buffer.FlushBufferRange(bufferSize * index, bufferSize);
		preparedForRendering = true;
	}
}