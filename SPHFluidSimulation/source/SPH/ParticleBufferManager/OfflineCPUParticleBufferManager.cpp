#include "pch.h"
#include "SPH/ParticleBufferManager/OfflineCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{/*
	OfflineCPUParticleBufferManager::OfflineCPUParticleBufferManager()
		: dynamicParticleCount(0)
	{
		buffers = Array<Buffer>(3, *this);
		currentBuffer = buffers.Count() - 1;
	}	
	void OfflineCPUParticleBufferManager::Clear()
	{
		for (auto& buffer : buffers)
			buffer.Clear();		
		
		currentBuffer = 0;
		dynamicParticleCount = 0;		
		
		staticParticles.Clear();
	}
	void OfflineCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineCPUParticleBufferManager::ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers[currentBuffer].ManagerDynamicParticles(dynamicParticles.Ptr());
		for (uintMem i = 0; i < buffers.Count(); ++i)
			if (i != currentBuffer)
				buffers[i].ManagerDynamicParticles(nullptr);
	}
	void OfflineCPUParticleBufferManager::ManagerStaticParticles(ArrayView<StaticParticle> staticParticles)
	{
		this->staticParticles = staticParticles;
	}
	CPUParticleReadBufferHandle& OfflineCPUParticleBufferManager::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	CPUParticleWriteBufferHandle& OfflineCPUParticleBufferManager::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}
	CPUParticleWriteBufferHandle& OfflineCPUParticleBufferManager::GetReadWriteBufferHandle()
	{
		return buffers[currentBuffer];
	}
	StaticParticle* OfflineCPUParticleBufferManager::GetStaticParticles()
	{
		return staticParticles.Ptr();
	}

	uintMem OfflineCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	uintMem OfflineCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticles.Count();
	}
	OfflineCPUParticleBufferManager::Buffer::Buffer(const OfflineCPUParticleBufferManager& bufferManager) 
		: bufferManager(bufferManager)
	{
	}
	void OfflineCPUParticleBufferManager::Buffer::Clear()
	{
		dynamicParticles.Clear();
	}
	void OfflineCPUParticleBufferManager::Buffer::ManagerDynamicParticles(const DynamicParticle* particles)
	{
		{
			std::unique_lock lk{ stateMutex };

			writeSync.WaitInactive();
			readSync.WaitInactive();			
		}
		
		if (particles != nullptr)
			dynamicParticles = Array<DynamicParticle>(particles, bufferManager.dynamicParticleCount);
		else
			dynamicParticles = Array<DynamicParticle>(bufferManager.dynamicParticleCount);
	}
	CPUSync& OfflineCPUParticleBufferManager::Buffer::GetReadSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		readSync.MarkStart();
		return readSync;
	}
	CPUSync& OfflineCPUParticleBufferManager::Buffer::GetWriteSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();		
		writeSync.MarkStart();
		return writeSync;
	}	*/
	
	OfflineCPUParticleBufferManager::OfflineCPUParticleBufferManager()
		: currentBuffer(0)
	{		
		buffers = Array<Buffer>(3);
	}
	OfflineCPUParticleBufferManager::~OfflineCPUParticleBufferManager()
	{		
		Clear();
	}
	void OfflineCPUParticleBufferManager::Clear()
	{		
		for (auto& buffer : buffers)
			buffer.dynamicParticles = nullptr;

		dynamicParticlesBuffer.Clear();
		staticParticles.Clear();

		currentBuffer = 0;
	}
	void OfflineCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineCPUParticleBufferManager::AllocateDynamicParticles(uintMem count)
	{
		dynamicParticlesBuffer.Resize(count * buffers.Count());

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].dynamicParticles = dynamicParticlesBuffer.Ptr() + count * i;
	}
	void OfflineCPUParticleBufferManager::AllocateStaticParticles(uintMem count)
	{
		staticParticles.Resize(count);
	}
	uintMem OfflineCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticlesBuffer.Count() / buffers.Count();
	}
	uintMem OfflineCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticles.Count();
	}
	CPUParticleBufferLockGuard OfflineCPUParticleBufferManager::LockDynamicParticlesActiveRead(const TimeInterval& timeInterval)
	{		
		auto& buffer = buffers[currentBuffer];

		uint64 lockTicket = buffer.dynamicParticlesLock.TryLockRead(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffer.dynamicParticles);		
	}
	CPUParticleBufferLockGuard OfflineCPUParticleBufferManager::LockDynamicParticlesAvailableRead(const TimeInterval& timeInterval, uintMem* index)
	{
		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			auto& buffer = buffers[(currentBuffer + buffers.Count() - i) % buffers.Count()];			

			uint64 lockTicket = buffer.dynamicParticlesLock.TryLockRead(i == buffers.Count() - 1 ? timeInterval : TimeInterval::Zero());

			if (lockTicket != 0)
			{
				if (index != nullptr)
					*index = i;
				return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffers[currentBuffer].dynamicParticles);
			}
		}

		return CPUParticleBufferLockGuard();
	}
	CPUParticleBufferLockGuard OfflineCPUParticleBufferManager::LockDynamicParticlesReadWrite(const TimeInterval& timeInterval)
	{
		auto& buffer = buffers[currentBuffer];

		uint64 lockTicket = buffer.dynamicParticlesLock.TryLockReadWrite(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffer.dynamicParticles);		
	}	
	CPUParticleBufferLockGuard OfflineCPUParticleBufferManager::LockStaticParticlesRead(const TimeInterval& timeInterval)
	{				
		uint64 lockTicket = staticParticlesLock.TryLockRead(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		return CPUParticleBufferLockGuard([this, lockTicket=lockTicket]() { staticParticlesLock.Unlock(lockTicket); }, staticParticles.Ptr());		
	}
	CPUParticleBufferLockGuard OfflineCPUParticleBufferManager::LockStaticParticlesReadWrite(const TimeInterval& timeInterval)
	{	
		auto lockTicket = staticParticlesLock.TryLockReadWrite(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		return CPUParticleBufferLockGuard([this, lockTicket = lockTicket]() { staticParticlesLock.Unlock(lockTicket); }, staticParticles.Ptr());
	}		
}