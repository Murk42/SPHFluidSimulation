#pragma once
#include "SPH/ParticleBufferManager/ParticleBufferManager.h"
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
	class OfflineGPUParticleBufferManager : public ParticleBufferManager
	{
	public:
		OfflineGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue);
		~OfflineGPUParticleBufferManager();

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
		struct DynamicParticlesSubBuffer
		{
			OpenCLLock dynamicParticlesLock;
			cl_mem dynamicParticlesView;
		};

		cl_context clContext;
		cl_device_id clDevice;
		cl_command_queue clCommandQueue;

		Array<DynamicParticlesSubBuffer> buffers;
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