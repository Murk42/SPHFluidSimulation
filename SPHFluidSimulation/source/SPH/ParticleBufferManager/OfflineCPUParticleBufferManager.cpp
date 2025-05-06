#include "pch.h"
#include "SPH/ParticleBufferManager/OfflineCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{		
	OfflineCPUParticleBufferManager::OfflineCPUParticleBufferManager()
		: currentBuffer(0)
	{
	}
	OfflineCPUParticleBufferManager::~OfflineCPUParticleBufferManager()
	{		
		Clear();
	}
	void OfflineCPUParticleBufferManager::Clear()
	{		
		currentBuffer = 0;
	
		buffer.Free();
		buffers.Clear();
	}
	void OfflineCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineCPUParticleBufferManager::Allocate(uintMem newBufferSize, void* ptr, uintMem bufferCount)
	{
		Clear();

		if (bufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		buffers = Array<ParticlesBuffer>(bufferCount);

		buffer.Allocate(newBufferSize);		
		
		if (ptr != nullptr)
			memcpy(buffer.Ptr(), ptr, newBufferSize);

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].SetPointer((char*)buffer.Ptr() + newBufferSize * i);
	}
	uintMem OfflineCPUParticleBufferManager::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem OfflineCPUParticleBufferManager::GetBufferSize()
	{
		return buffer.Size();
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockRead(void* signalEvent)
	{
		return buffers[currentBuffer].LockRead();
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockWrite(void* signalEvent)
	{
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