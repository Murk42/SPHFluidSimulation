#include "pch.h"
#include "SPH/ParticleBufferManagers/OfflineCPUParticleBufferManager.h"


namespace SPH
{
	OfflineCPUParticleBufferManager::OfflineCPUParticleBufferManager()
		: currentBuffer(0), particleSize(0), particleCount(0)
	{
	}
	OfflineCPUParticleBufferManager::~OfflineCPUParticleBufferManager()
	{
		Clear();
	}
	void OfflineCPUParticleBufferManager::Clear()
	{
		currentBuffer = 0;
		buffers.Clear();
		buffer.Clear();
		particleSize = 0;
		particleCount = 0;
	}
	void OfflineCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineCPUParticleBufferManager::Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount)
	{
		Clear();

		if (newBufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		particleSize = newParticleSize;
		particleCount = newParticleCount;

		buffers = Array<ParticlesBuffer>(newBufferCount);
		buffer.Allocate(particleSize * particleCount * newBufferCount);

		if (particles != nullptr)
			memcpy(buffer.Ptr(), particles, particleSize * particleCount);

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].SetPointer((char*)buffer.Ptr() + particleSize * particleCount * i);
	}
	uintMem OfflineCPUParticleBufferManager::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem OfflineCPUParticleBufferManager::GetParticleCount() const
	{
		return particleCount;
	}
	uintMem OfflineCPUParticleBufferManager::GetParticleSize() const
	{
		return particleSize;
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockRead(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockRead();
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockWrite(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockWrite();
	}
	void OfflineCPUParticleBufferManager::FlushAllOperations()
	{

	}
	OfflineCPUParticleBufferManager::ParticlesBuffer::ParticlesBuffer()
		: ptr(nullptr)
	{
	}
	void OfflineCPUParticleBufferManager::ParticlesBuffer::SetPointer(void* ptr)
	{
		this->ptr = ptr;
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::ParticlesBuffer::LockRead()
	{
		lock.LockRead();

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((CPULock*)userData)->UnlockRead();
			}, ptr, &lock);
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::ParticlesBuffer::LockWrite()
	{
		lock.LockWrite();

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((CPULock*)userData)->UnlockWrite();
			}, ptr, &lock);
	}
}