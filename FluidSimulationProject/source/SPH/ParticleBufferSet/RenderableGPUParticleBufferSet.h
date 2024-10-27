#pragma once
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"
#include "SPH/ParticleBufferSet/GPUParticleBufferSet.h"
#include "SPH/ParticleBufferSet/ParticleBufferSetRenderData.h"
#include "SPH/System/System.h"

class OpenCLContext;

namespace SPH
{
	class RenderableGPUParticleBufferSet :		
		public GPUParticleBufferSet,
		public ParticleBufferSetRenderData
	{
	public:
		RenderableGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue);

		void Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles) override;
		void Clear() override;
		void Advance() override;

		GPUParticleReadBufferHandle& GetReadBufferHandle() override;
		GPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;

		uintMem GetDynamicParticleCount() override;
	private:
		class Buffer : 
			public GPUParticleReadBufferHandle,
			public GPUParticleWriteBufferHandle,
			public ParticleRenderBufferHandle
		{
			OpenCLContext& clContext;
			cl::CommandQueue& queue;
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;

			Graphics::OpenGLWrapper::GraphicsBuffer dynamicParticleBufferGL;
			cl::Buffer dynamicParticleBufferCL;
			void* dynamicParticleBufferMap;
			
			cl::Event readFinishedEvent;			
			cl::Event writeFinishedEvent;
			cl::Event copyFinishedEvent;
			
			//This fence is signaled when all particles are copied from the CL buffer to the GL buffer if there is no GL-CL interop. 
			//If there is it is signaled when the rendering is finished.
			Graphics::OpenGLWrapper::Fence renderingFence; 

			uintMem dynamicParticleCount;

		public:
			Buffer(OpenCLContext& clContext, cl::CommandQueue& queue);

			void Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount);

			void StartRead(cl_event* finishedEvent) override;
			void FinishRead(ArrayView<cl_event> waitEvents) override;
			void StartWrite(cl_event* finishedEvent) override;
			void FinishWrite(ArrayView<cl_event> waitEvents,bool prepareForRendering) override;
			void StartRender() override;
			void FinishRender() override;
			void WaitRender() override;

			cl::Buffer& GetReadBuffer() override { return dynamicParticleBufferCL; }
			cl::Buffer& GetWriteBuffer() override { return dynamicParticleBufferCL; }
			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }			
		};

		OpenCLContext& clContext;
		cl::CommandQueue& queue;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		uintMem dynamicParticleCount;						
	};
}