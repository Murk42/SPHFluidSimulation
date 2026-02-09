#pragma once
#include "SPH/Core/Particles.h"
#include "SPH/Core/ResourceLockGuard.h"

namespace SPH
{
	class ParticleBufferManager
	{
	public:
		virtual ~ParticleBufferManager() {}

		virtual void Clear() = 0;
		virtual void Advance() = 0;

		virtual void Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount) = 0;

		virtual uintMem GetBufferCount() const = 0;
		virtual uintMem GetParticleCount() const = 0;
		virtual uintMem GetParticleSize() const = 0;

		virtual ResourceLockGuard LockRead(void* signalEvent) = 0;
		virtual ResourceLockGuard LockWrite(void* signalEvent) = 0;

		virtual void FlushAllOperations() = 0;
	};
}