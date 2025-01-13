#pragma once
#include "SPH/ParticleBufferManager/CPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/ResourceLock.h"

namespace SPH
{
	/*
	class OfflineCPUParticleBufferManager :
		public CPUParticleBufferManager
	{
	public:
		OfflineCPUParticleBufferManager();
		
		void Clear() override;
		void Advance() override;		

		void ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles) override;
		void ManagerStaticParticles(ArrayView<StaticParticle> staticParticles) override;

		CPUParticleReadBufferHandle& GetReadBufferHandle() override;
		CPUParticleWriteBufferHandle& GetWriteBufferHandle() override;		
		CPUParticleWriteBufferHandle& GetReadWriteBufferHandle() override;
		StaticParticle* GetStaticParticles() override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;
	private:
		class Buffer :
			public CPUParticleReadBufferHandle,
			public CPUParticleWriteBufferHandle
		{			
			const OfflineCPUParticleBufferManager& bufferManager;
			Array<DynamicParticle> dynamicParticles;

			CPUSync readSync;
			CPUSync writeSync;

			std::mutex stateMutex;
			std::condition_variable stateCV;			
		public:
			Buffer(const OfflineCPUParticleBufferManager& bufferManager);

			void Clear();

			void ManagerDynamicParticles(const DynamicParticle* dynamicParticlePtr);

			CPUSync& GetReadSync() override;
			CPUSync& GetWriteSync() override;			

			const DynamicParticle* GetReadBuffer() override { return dynamicParticles.Ptr(); }
			DynamicParticle* GetWriteBuffer() override { return dynamicParticles.Ptr(); }
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Array<StaticParticle> staticParticles;

		uintMem dynamicParticleCount;		
	};
	*/	

	class OfflineCPUParticleBufferManager : public CPUParticleBufferManager
	{
	public:		
		OfflineCPUParticleBufferManager();
		~OfflineCPUParticleBufferManager();

		void Clear() override;
		void Advance() override;

		void AllocateDynamicParticles(uintMem count) override;
		void AllocateStaticParticles(uintMem count) override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;

		CPUParticleBufferLockGuard LockDynamicParticlesActiveRead(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;
		CPUParticleBufferLockGuard LockDynamicParticlesAvailableRead(const TimeInterval& timeInterval = TimeInterval::Infinity(), uintMem* index) override;
		CPUParticleBufferLockGuard LockDynamicParticlesReadWrite(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;		

		CPUParticleBufferLockGuard LockStaticParticlesRead(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;
		CPUParticleBufferLockGuard LockStaticParticlesReadWrite(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;
	private:				
		struct Buffer
		{
			Lock dynamicParticlesLock;
			DynamicParticle* dynamicParticles;
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Lock staticParticlesLock;
		Array<StaticParticle> staticParticles;

		Array<DynamicParticle> dynamicParticlesBuffer;		
	};
}