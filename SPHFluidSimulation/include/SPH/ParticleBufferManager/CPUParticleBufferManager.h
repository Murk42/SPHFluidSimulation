#pragma once
#include "SPH/ParticleBufferManager/ParticleBufferManager.h"

namespace SPH
{
	/*
	class CPUSync
	{
	public:
		CPUSync()
		{
			active.clear();
		}

		void MarkStart()
		{
			active.test_and_Manager();
		}
		void MarkEnd()
		{
			active.clear();
			active.notify_all();
		}

		void WaitInactive()
		{
			active.wait(true);
		}
	private:
		std::atomic_flag active;
	};

	class CPUParticleReadBufferHandle;
	class CPUParticleWriteBufferHandle;

	class CPUParticleBufferManager :
		public ParticleBufferManager
	{
	public:
		virtual CPUParticleReadBufferHandle& GetReadBufferHandle() = 0;
		virtual CPUParticleWriteBufferHandle& GetWriteBufferHandle() = 0;
		virtual CPUParticleWriteBufferHandle& GetReadWriteBufferHandle() = 0;

		virtual const StaticParticle* GetStaticParticles() = 0;
	};

	class CPUParticleReadBufferHandle
	{
	public:
		virtual CPUSync& GetReadSync() = 0;

		virtual const DynamicParticle* GetReadBuffer() = 0;
	};
	class CPUParticleWriteBufferHandle
	{
	public:
		virtual CPUSync& GetWriteSync() = 0;
		virtual DynamicParticle* GetWriteBuffer() = 0;
	};
	*/
	class CPUParticleBufferLockGuard
	{
	public:
		using UnlockFunction = std::function<void()>;

		CPUParticleBufferLockGuard();
		CPUParticleBufferLockGuard(UnlockFunction&& unlockFunction, void* buffer);
		CPUParticleBufferLockGuard(const CPUParticleBufferLockGuard&) = delete;
		CPUParticleBufferLockGuard(CPUParticleBufferLockGuard&& other) noexcept;
		~CPUParticleBufferLockGuard();

		void Unlock();
		void* GetBuffer();

		const CPUParticleBufferLockGuard& operator=(const CPUParticleBufferLockGuard& other) = delete;
		const CPUParticleBufferLockGuard& operator=(CPUParticleBufferLockGuard&& other) noexcept;
	private:
		UnlockFunction unlockFunction;
		void* buffer;
	};

	class CPUParticleBufferManager : public ParticleBufferManager
	{
	public:
		virtual CPUParticleBufferLockGuard LockDynamicParticlesActiveRead(const TimeInterval& timeInterval) = 0;
		virtual CPUParticleBufferLockGuard LockDynamicParticlesAvailableRead(const TimeInterval& timeInterval, uintMem* index) = 0;
		virtual CPUParticleBufferLockGuard LockDynamicParticlesReadWrite(const TimeInterval& timeInterval) = 0;

		virtual CPUParticleBufferLockGuard LockStaticParticlesRead(const TimeInterval& timeInterval) = 0;
		virtual CPUParticleBufferLockGuard LockStaticParticlesReadWrite(const TimeInterval& timeInterval) = 0;
	};
}