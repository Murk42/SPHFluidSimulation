#pragma once
#include "SPH/ParticleBufferManager/GPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"

namespace SPH
{
	/*
	class OfflineGPUParticleBufferManager :
		public GPUParticleBufferManager
	{
	public:
		OfflineGPUParticleBufferManager(cl_context clContext, cl_command_queue queue);
		
		void Clear() override;
		void Advance() override;

		void ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles) override;
		void ManagerStaticParticles(ArrayView<StaticParticle> staticParticles) override;

		GPUParticleReadBufferHandle& GetReadBufferHandle() override;
		GPUParticleWriteBufferHandle& GetWriteBufferHandle() override;		
		GPUParticleWriteBufferHandle& GetReadWriteBufferHandle() override;
		const cl_mem& GetStaticParticleBuffer() override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;
	private:
		class Buffer :
			public GPUParticleReadBufferHandle,
			public GPUParticleWriteBufferHandle
		{
			const OfflineGPUParticleBufferManager& bufferManager;
			
			cl_mem dynamicParticleBufferCL;			

			cl_event readFinishedEvent;
			cl_event writeFinishedEvent;
			cl_event copyFinishedEvent;						
		public:
			Buffer(const OfflineGPUParticleBufferManager& bufferManager);
			~Buffer();

			void Clear();

			void ManagerDynamicParticles(const DynamicParticle* dynamicParticlesPtr);

			void StartRead(cl_event* finishedEvent) override;
			void FinishRead(ArrayView<cl_event> waitEvents) override;
			void StartWrite(cl_event* finishedEvent) override;
			void FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering) override;

			const cl_mem& GetReadBuffer() override { return dynamicParticleBufferCL; }
			const cl_mem& GetWriteBuffer() override { return dynamicParticleBufferCL; }			
		};

		cl_context clContext;
		cl_command_queue queue;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		cl_mem staticParticleBuffer;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
	};
	*/
	class OfflineGPUParticleBufferManager : public GPUParticleBufferManager
	{
	public:
		OfflineGPUParticleBufferManager(cl_context clContext, cl_command_queue commandQueue);
		~OfflineGPUParticleBufferManager();

		void Clear() override;
		void Advance() override;

		void AllocateDynamicParticles(uintMem count) override;
		void AllocateStaticParticles(uintMem count) override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;

		GPUParticleBufferLockGuard LockDynamicParticlesActiveRead(cl_event* signalEvent) override;
		GPUParticleBufferLockGuard LockDynamicParticlesAvailableRead(cl_event* signalEvent, uintMem* index) override;
		GPUParticleBufferLockGuard LockDynamicParticlesReadWrite(cl_event* signalEvent) override;

		GPUParticleBufferLockGuard LockStaticParticlesRead(cl_event* signalEvent) override;
		GPUParticleBufferLockGuard LockStaticParticlesReadWrite(cl_event* signalEvent) override;
	private:
		struct Buffer
		{
			OpenCLLock dynamicParticlesLock;
			cl_mem dynamicParticlesView;			

			Buffer(cl_command_queue commandQueue);
		};

		cl_context clContext;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		OpenCLLock staticParticlesLock;
		cl_mem staticParticlesBuffer;
		uintMem staticParticlesCount;

		cl_mem dynamicParticlesBuffer;		
		uintMem dynamicParticlesCount;

		void CleanDynamicParticlesBuffers();
		void CleanStaticParticlesBuffer();
	};
}