#pragma once
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"
#include "SPH/Core/ParticleBufferManagerRenderData.h"

namespace SPH
{
	/*
	class RenderableGPUParticleBufferManager :		
		public GPUParticleBufferManager,
		public ParticleBufferManagerRenderData
	{
	public:
		RenderableGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue);
		
		void Clear() override;
		void Advance() override;		

		void ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles) override;
		void ManagerStaticParticles(ArrayView<StaticParticle> staticParticles) override;

		GPUParticleReadBufferHandle& GetReadBufferHandle() override;
		GPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		GPUParticleWriteBufferHandle& GetReadWriteBufferHandle() override;
		const cl_mem& GetStaticParticleBuffer() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticleVertexArray() override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;
	private:
		class Buffer : 
			public GPUParticleReadBufferHandle,
			public GPUParticleWriteBufferHandle,
			public ParticleRenderBufferHandle
		{
			const RenderableGPUParticleBufferManager& bufferManager;
			
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;

			Graphics::OpenGLWrapper::GraphicsBuffer dynamicParticleBufferGL;
			cl_mem dynamicParticleBufferCL;
			void* dynamicParticleBufferMap;
			
			cl_event readFinishedEvent;		
			//This is true while commands for reading are still being queued, or between StartRead and FinishRead function calls.
			bool readQueueing;
			cl_event writeFinishedEvent;
			//This is true while commands for reading are still being queued, or between StartRead and FinishRead function calls.
			bool writeQueueing;
			cl_event copyFinishedEvent;
			//This is true while commands for copying are still being queued, or between StartRead and FinishRead function calls.
			bool copyQueueing;
			
			//This fence is signaled when all particles are copied from the CL buffer to the GL buffer if there is no GL-CL interop. 
			//If there is it is signaled when the rendering is finished.
			Graphics::OpenGLWrapper::Fence renderingFence; 			
		public:
			Buffer(const RenderableGPUParticleBufferManager& bufferManager);
			~Buffer();

			void Clear();

			void ManagerDynamicParticles(const DynamicParticle* particles);

			void StartRead(cl_event* finishedEvent) override;
			void FinishRead(ArrayView<cl_event> waitEvents) override;
			void StartWrite(cl_event* finishedEvent) override;
			void FinishWrite(ArrayView<cl_event> waitEvents,bool prepareForRendering) override;
			void StartRender() override;
			void FinishRender() override;
			void WaitRender() override;

			const cl_mem& GetReadBuffer() override { return dynamicParticleBufferCL; }
			const cl_mem& GetWriteBuffer() override { return dynamicParticleBufferCL; }
			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }			
		};

		cl_context clContext;
		bool CLGLInterop;
		cl_command_queue clCommandQueue;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		cl_mem staticParticleBufferCL;
		Graphics::OpenGLWrapper::VertexArray staticParticleVertexArray;
		Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticleBufferGL;

		uintMem dynamicParticleCount;		
		uintMem staticParticleCount;
	};
	*/
	
	class RenderableGPUParticleBufferManagerWithoutCLGLInterop :		
		public ParticleBufferManagerRenderData
	{
	public:
		RenderableGPUParticleBufferManagerWithoutCLGLInterop(cl_context clContext, cl_device_id clDevice, cl_command_queue commandQueue);
		~RenderableGPUParticleBufferManagerWithoutCLGLInterop();

		void Clear() override;
		void Advance() override;

		void AllocateDynamicParticles(uintMem count, DynamicParticle* particles) override;
		void AllocateStaticParticles(uintMem count, StaticParticle* particles) override;

		uintMem GetDynamicParticleBufferCount() const override;
		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;				
		Graphics::OpenGLWrapper::GraphicsBuffer* GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset) override;
		Graphics::OpenGLWrapper::GraphicsBuffer* GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset) override;\

		ResourceLockGuard LockDynamicParticlesForRead(void* signalEvent) override;
		ResourceLockGuard LockDynamicParticlesForWrite(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForRead(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForWrite(void* signalEvent) override;				
		ResourceLockGuard LockDynamicParticlesForRendering(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForRendering(void* signalEvent) override;

		void PrepareDynamicParticlesForRendering() override;
		void PrepareStaticParticlesForRendering() override;

		void FlushAllOperations() override;
	protected:
		virtual void AllocateBuffers(uintMem bufferSizeGL, uintMem bufferSizeCL, void* ptrGL, void* ptrCL, Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, cl_mem& bufferCL);
	private:		
		struct ParticlesBuffer
		{			
		public:
			ParticlesBuffer(cl_command_queue clCommandQueue);
			~ParticlesBuffer();

			void SetBuffer(cl_mem buffer, bool copyRequiredForRendering);

			ResourceLockGuard LockRead(cl_event* signalEvent, bool acquireGLBuffers);
			ResourceLockGuard LockWrite(cl_event* signalEvent, bool acquireGLBuffers);
			ResourceLockGuard LockForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize);

			void PrepareForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize);
		private:
			bool copyRequiredForRendering;
			cl_event copyForRenderingFinishedEvent;
			cl_mem buffer;
			OpenCLLock lock;			
		};		

		cl_context clContext;
		cl_device_id clDevice;
		cl_command_queue clCommandQueue;		

		uintMem currentBuffer;
		
		Array<ParticlesBuffer> dynamicParticlesBuffers;
		cl_mem dynamicParticlesMemoryCL;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesBufferGL;
		uintMem dynamicParticlesCount;

		ParticlesBuffer staticParticlesBuffer;
		cl_mem staticParticlesMemoryCL;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer staticParticlesBufferGL;
		uintMem staticParticlesCount;
		
		void CleanDynamicParticlesBuffers();
		void CleanStaticParticlesBuffer();		
	};	

	class RenderableGPUParticleBufferManagerWithCLGLInterop :
		public RenderableGPUParticleBufferManagerWithoutCLGLInterop
	{	
	public:
		ResourceLockGuard LockDynamicParticlesForRead(void* signalEvent) override;
		ResourceLockGuard LockDynamicParticlesForWrite(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForRead(void* signalEvent) override;
		ResourceLockGuard LockStaticParticlesForWrite(void* signalEvent) override;
	private:
		void AllocateBuffers(uintMem bufferSizeGL, uintMem bufferSizeCL, void* ptrGL, void* ptrCL, Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, cl_mem& bufferCL) override;
	};
}