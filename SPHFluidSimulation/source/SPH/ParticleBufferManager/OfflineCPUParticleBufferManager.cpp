#include "pch.h"
#include "SPH/ParticleBufferManager/OfflineCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{		
	OfflineCPUParticleBufferManager::OfflineCPUParticleBufferManager()
		: currentBuffer(0), dynamicParticlesBuffers(3)
	{
	}
	OfflineCPUParticleBufferManager::~OfflineCPUParticleBufferManager()
	{		
		Clear();
	}
	void OfflineCPUParticleBufferManager::Clear()
	{		
		currentBuffer = 0;

		ClearDynamicParticlesBuffers();
		ClearStaticParticlesBuffer();						
	}
	void OfflineCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % dynamicParticlesBuffers.Count();
	}
	void OfflineCPUParticleBufferManager::AllocateDynamicParticles(uintMem count, DynamicParticle* particles)
	{
		ClearDynamicParticlesBuffers();
		
		if (count == 0)
			return;

		currentBuffer = 0;

		dynamicParticlesMemory = Array<DynamicParticle>((DynamicParticle*)nullptr, count * dynamicParticlesBuffers.Count());
		memcpy(dynamicParticlesMemory.Ptr(), particles, count * sizeof(DynamicParticle));

		for (uintMem i = 0; i < dynamicParticlesBuffers.Count(); ++i)		
			dynamicParticlesBuffers[i].ptr = dynamicParticlesMemory.Ptr() + count * i;		
	}
	void OfflineCPUParticleBufferManager::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		ClearStaticParticlesBuffer();
		
		staticParticlesMemory = Array<StaticParticle>(particles, count);		
		staticParticlesBuffer.ptr = staticParticlesMemory.Ptr();
	}
	uintMem OfflineCPUParticleBufferManager::GetDynamicParticleBufferCount() const
	{
		return dynamicParticlesBuffers.Count();
	}
	uintMem OfflineCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticlesMemory.Count() / dynamicParticlesBuffers.Count();
	}
	uintMem OfflineCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticlesMemory.Count();
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockDynamicParticlesForRead(void* signalEvent)
	{				
		return dynamicParticlesBuffers[currentBuffer].LockRead();
	}	
	ResourceLockGuard OfflineCPUParticleBufferManager::LockDynamicParticlesForWrite(void* signalEvent)
	{		
		return dynamicParticlesBuffers[currentBuffer].LockWrite();
	}	
	ResourceLockGuard OfflineCPUParticleBufferManager::LockStaticParticlesForRead(void* signalEvent)
	{				
		return staticParticlesBuffer.LockRead();		
	}
	ResourceLockGuard OfflineCPUParticleBufferManager::LockStaticParticlesForWrite(void* signalEvent)
	{	
		return staticParticlesBuffer.LockWrite();
	}
	void OfflineCPUParticleBufferManager::FlushAllOperations()
	{

	}
	void OfflineCPUParticleBufferManager::ClearDynamicParticlesBuffers()
	{
		Array<ResourceLockGuard> lockGuards;
		lockGuards.ReserveExactly(dynamicParticlesBuffers.Count());

		for (auto& buffer : dynamicParticlesBuffers)
			lockGuards.AddBack(buffer.LockWrite());

		dynamicParticlesMemory.Clear();

		for (auto& buffer : dynamicParticlesBuffers)		
			buffer.ptr = nullptr;					

		for (auto& lockGuard : lockGuards)
			lockGuard.Unlock({});
	}
	void OfflineCPUParticleBufferManager::ClearStaticParticlesBuffer()
	{
		ResourceLockGuard lockGuard = staticParticlesBuffer.LockWrite();

		staticParticlesMemory.Clear();

		staticParticlesBuffer.ptr = nullptr;				

		lockGuard.Unlock({ });
	}
	OfflineCPUParticleBufferManager::ParticlesBuffer::ParticlesBuffer()
		: ptr(nullptr)
	{
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