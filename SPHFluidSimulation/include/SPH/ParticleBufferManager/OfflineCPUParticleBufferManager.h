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

		void Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount) override;

		uintMem GetBufferCount() const override;
		uintMem GetParticleCount() const override;
		uintMem GetParticleSize() const override;

		ResourceLockGuard LockRead(void* signalEvent) override;
		ResourceLockGuard LockWrite(void* signalEvent) override;

		void FlushAllOperations() override;
	private:
		struct ParticlesBuffer
		{
		public:
			ParticlesBuffer();
			ParticlesBuffer(ParticlesBuffer&&) noexcept = default;

			void SetPointer(void* ptr);

			ResourceLockGuard LockRead();
			ResourceLockGuard LockWrite();
		private:
			void* ptr;
			CPULock lock;
		};

		uintMem currentBuffer;

		Array<ParticlesBuffer> buffers;
		Buffer buffer;
		uintMem particleSize;
		uintMem particleCount;
	};
}