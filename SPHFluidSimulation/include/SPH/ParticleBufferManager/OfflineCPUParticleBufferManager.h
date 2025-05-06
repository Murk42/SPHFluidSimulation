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

		void Allocate(uintMem newBufferSize, void* ptr, uintMem bufferCount) override;

		uintMem GetBufferCount() const override;
		uintMem GetBufferSize() override;		

		ResourceLockGuard LockRead(void* signalEvent) override;		
		ResourceLockGuard LockWrite(void* signalEvent) override;		

		void FlushAllOperations() override;
	private:
		struct ParticlesBuffer
		{
		public:
			ParticlesBuffer();

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
	};
}