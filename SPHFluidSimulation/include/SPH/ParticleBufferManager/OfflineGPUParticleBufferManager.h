#pragma once
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"

namespace SPH
{
	class OfflineGPUParticleBufferManager : public ParticleBufferManager
	{
	public:
		OfflineGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue);
		~OfflineGPUParticleBufferManager();

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
			ParticlesBuffer(cl_command_queue clCommandQueue);

			void CreateBuffer(cl_mem parentBuffer, uintMem offset, uintMem size);

			ResourceLockGuard LockRead(cl_event* signalEvent);
			ResourceLockGuard LockWrite(cl_event* signalEvent);
		private:
			OpenCLLock lock;
			cl_mem buffer;
		};

		cl_context clContext;
		cl_device_id clDevice;
		cl_command_queue clCommandQueue;

		uintMem currentBuffer;

		Array<ParticlesBuffer> buffers;
		cl_mem bufferCL;
		uintMem bufferSize;		
	};
}