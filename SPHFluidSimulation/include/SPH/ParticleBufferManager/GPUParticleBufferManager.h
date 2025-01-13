#pragma once
#include "SPH/ParticleBufferManager/ParticleBufferManager.h"

namespace SPH
{
	/*
	class GPUParticleReadBufferHandle;
	class GPUParticleWriteBufferHandle;

	class GPUParticleBufferManager :
		public ParticleBufferManager
	{
	public:
		virtual GPUParticleReadBufferHandle& GetReadBufferHandle() = 0;
		virtual GPUParticleWriteBufferHandle& GetWriteBufferHandle() = 0;		
		virtual GPUParticleWriteBufferHandle& GetReadWriteBufferHandle() = 0;
		virtual const cl_mem& GetStaticParticleBuffer() = 0;
	};

	class GPUParticleReadBufferHandle
	{
	public:
		virtual void StartRead(cl_event* finishedEvent) = 0;
		virtual void FinishRead(ArrayView<cl_event> waitEvents) = 0;

		virtual const cl_mem& GetReadBuffer() = 0;
	};
	class GPUParticleWriteBufferHandle
	{
	public:
		virtual void StartWrite(cl_event* finishedEvent) = 0;
		virtual void FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering) = 0;
		virtual const cl_mem& GetWriteBuffer() = 0;
	};	
	*/

	class GPUParticleBufferLockGuard
	{
	public:
		using UnlockFunction = std::function<void(ArrayView<cl_event> waitEvents)>;

		GPUParticleBufferLockGuard();
		GPUParticleBufferLockGuard(UnlockFunction&& unlockFunction, cl_mem buffer);
		GPUParticleBufferLockGuard(const GPUParticleBufferLockGuard&) = delete;
		GPUParticleBufferLockGuard(GPUParticleBufferLockGuard&& other) noexcept;
		~GPUParticleBufferLockGuard();

		void Unlock(ArrayView<cl_event> waitEvents);
		cl_mem GetBuffer();

		const GPUParticleBufferLockGuard& operator=(const GPUParticleBufferLockGuard& other) = delete;
		const GPUParticleBufferLockGuard& operator=(GPUParticleBufferLockGuard&& other) noexcept;
	private:
		UnlockFunction unlockFunction;
		cl_mem buffer;
	};

	class GPUParticleBufferManager : public ParticleBufferManager
	{
	public:
		virtual GPUParticleBufferLockGuard LockDynamicParticlesActiveRead(cl_event* signalEvent) = 0;
		virtual GPUParticleBufferLockGuard LockDynamicParticlesAvailableRead(cl_event* signalEvent, uintMem* index) = 0;
		virtual GPUParticleBufferLockGuard LockDynamicParticlesReadWrite(cl_event* signalEvent) = 0;

		virtual GPUParticleBufferLockGuard LockStaticParticlesRead(cl_event* signalEvent) = 0;
		virtual GPUParticleBufferLockGuard LockStaticParticlesReadWrite(cl_event* signalEvent) = 0;
	};
}