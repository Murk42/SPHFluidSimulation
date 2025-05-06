#pragma once
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"
#include "SPH/Core/ParticleBufferManagerGL.h"

namespace SPH
{
	/*
	class RenderableGPUParticleBufferManager :		
		public GPUParticleBufferManager,
		public ParticleBufferManagerGL
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
		public ParticleBufferManagerGL
	{
	public:
		RenderableGPUParticleBufferManagerWithoutCLGLInterop(cl_context clContext, cl_device_id clDevice, cl_command_queue commandQueue);
		~RenderableGPUParticleBufferManagerWithoutCLGLInterop();

		void Clear() override;
		void Advance() override;

		void Allocate(uintMem newBufferSize, void* ptr, uintMem bufferCount) override;

		uintMem GetBufferCount() const override;
		uintMem GetBufferSize() override;

		Graphics::OpenGLWrapper::GraphicsBuffer* GetGraphicsBuffer(uintMem index, uintMem& bufferOffset) override;		

		ResourceLockGuard LockRead(void* signalEvent) override;
		ResourceLockGuard LockWrite(void* signalEvent) override;		
		ResourceLockGuard LockForRendering(void* signalEvent) override;		
		
		void PrepareForRendering() override;

		void FlushAllOperations() override;
	protected:
		virtual void CreateBuffers(void* ptr);
	private:		
		struct ParticlesBuffer
		{			
		public:
			ParticlesBuffer(cl_command_queue clCommandQueue);
			~ParticlesBuffer();

			void CreateBuffer(cl_mem parentBuffer, uintMem offset, uintMem size, bool copyRequiredForRendering);

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
		
		uintMem bufferSize;
		Array<ParticlesBuffer> buffers;
		cl_mem bufferCL;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer bufferGL;
	};	

	class RenderableGPUParticleBufferManagerWithCLGLInterop :
		public RenderableGPUParticleBufferManagerWithoutCLGLInterop
	{	
	public:
		ResourceLockGuard LockRead(void* signalEvent) override;
		ResourceLockGuard LockWrite(void* signalEvent) override;		
	private:
		void CreateBuffers(void* ptr) override;
	};
}