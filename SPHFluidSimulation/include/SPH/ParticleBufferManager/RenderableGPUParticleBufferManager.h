#pragma once
#include "SPH/ParticleBufferManager/GPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"
#include "SPH/ParticleBufferManager/ParticleBufferManagerRenderData.h"

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

	class RenderableGPUParticleBufferManager :
		public GPUParticleBufferManager,
		public ParticleBufferManagerRenderData
	{
	public:
		RenderableGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue commandQueue);
		~RenderableGPUParticleBufferManager();

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

		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray(uintMem index) override;
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray() override;

	private:
		struct Buffer
		{
			Graphics::OpenGLWrapper::VertexArray dynamicParticlesVA;
			std::mutex dynamicParticlesFencesMutex;
			Array<Graphics::OpenGLWrapper::Fence> dynamicParticleFences;
			OpenCLLock dynamicParticlesLock;

			cl_mem dynamicParticlesView;		

			Buffer(cl_command_queue commandQueue);
		};

		cl_context clContext;
		bool CLGLInteropSupported;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Graphics::OpenGLWrapper::VertexArray staticParticlesVA;
		std::mutex staticParticlesFencesMutex;
		Array<Graphics::OpenGLWrapper::Fence> staticParticleFences;
		OpenCLLock staticParticlesLock;

		cl_mem staticParticlesBufferCL;
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer staticParticlesBufferGL;
		uintMem staticParticlesCount;

		cl_mem dynamicParticlesBufferCL;
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticlesBufferGL;
		uintMem dynamicParticlesCount;

		void CleanDynamicParticlesBuffers();
		void CleanStaticParticlesBuffer();
	};
}