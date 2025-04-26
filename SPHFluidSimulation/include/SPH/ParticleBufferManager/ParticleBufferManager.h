#pragma once
#include "SPH/System/Particles.h"
#include "SPH/ParticleBufferManager/ResourceLockGuard.h"

namespace SPH
{		
	class ParticleBufferManager
	{
	public:
		virtual ~ParticleBufferManager() { }
				
		virtual void Clear() = 0;
		virtual void Advance() = 0;	

		virtual void AllocateDynamicParticles(uintMem count, DynamicParticle* particles) = 0;
		virtual void AllocateStaticParticles(uintMem count, StaticParticle* particles) = 0;

		virtual uintMem GetDynamicParticleBufferCount() const = 0;

		virtual uintMem GetDynamicParticleCount() = 0;
		virtual uintMem GetStaticParticleCount() = 0;

		virtual ResourceLockGuard LockDynamicParticlesForRead(void* signalEvent) = 0;		
		virtual ResourceLockGuard LockDynamicParticlesForWrite(void* signalEvent) = 0;

		virtual ResourceLockGuard LockStaticParticlesForRead(void* signalEvent) = 0;
		virtual ResourceLockGuard LockStaticParticlesForWrite(void* signalEvent) = 0;

		virtual void FlushAllOperations() = 0;
	};		
}