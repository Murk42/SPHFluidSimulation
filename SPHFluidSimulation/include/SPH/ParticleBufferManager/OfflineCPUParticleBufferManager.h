#pragma once
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/ParticleBufferManager/CPUResourceLock.h"

namespace SPH
{
	class OfflineCPUParticleBufferManager : public ParticleBufferManager
	{
	public:		
		OfflineCPUParticleBufferManager();
		~OfflineCPUParticleBufferManager();

		void Clear() override;
		void Advance() override;

		void AllocateDynamicParticles(uintMem count, DynamicParticle* particles) override;
		void AllocateStaticParticles(uintMem count, StaticParticle* particles) override;

		uintMem GetDynamicParticleBufferCount() const override;
		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;

		ResourceLockGuard LockDynamicParticlesForRead(void* signalEvent) override;		
		ResourceLockGuard LockDynamicParticlesForWrite(void* signalEvent) override;

		ResourceLockGuard LockStaticParticlesForRead(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForWrite(void* signalEvent) override;

		void FlushAllOperations() override;
	private:
		struct ParticlesBuffer
		{
			void* ptr;
			CPULock lock;

			ParticlesBuffer();

			ResourceLockGuard LockRead();
			ResourceLockGuard LockWrite();
		}; 		

		uintMem currentBuffer;		

		Array<ParticlesBuffer> dynamicParticlesBuffers;
		Array<DynamicParticle> dynamicParticlesMemory;		

		ParticlesBuffer staticParticlesBuffer;		
		Array<StaticParticle> staticParticlesMemory;

		void ClearDynamicParticlesBuffers();
		void ClearStaticParticlesBuffer();
	};
}