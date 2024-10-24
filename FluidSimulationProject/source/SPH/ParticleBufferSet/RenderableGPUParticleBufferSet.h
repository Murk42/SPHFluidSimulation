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
		void Advance() override;

		GPUParticleReadBufferHandle& GetReadBufferHandle() override;
		GPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;

		uintMem GetDynamicParticleCount() override;

		void ReorderParticles() override;
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
			
			cl::Event writeFinishedEvent;
			cl::Event readFinishedEvent;
			Graphics::OpenGLWrapper::Fence readFinishedFence;

			uintMem dynamicParticleCount;

		public:
			Buffer(OpenCLContext& clContext, cl::CommandQueue& queue);
			Buffer(OpenCLContext& clContext, cl::CommandQueue& queue, const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticlesCount);			

			void StartRead() override;
			void StartWrite() override;
			void FinishRead() override;
			void FinishWrite() override;
			void StartRender() override;
			void FinishRender() override;

			cl::Buffer& GetReadBuffer() override { return dynamicParticleBufferCL; }
			cl::Buffer& GetWriteBuffer() override { return dynamicParticleBufferCL; }
			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }			

			void Swap(Buffer&& other);
		};

		OpenCLContext& clContext;
		cl::CommandQueue& queue;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Buffer intermediateBuffer;			
		
		uintMem dynamicParticleCount;						
	};
	/*
	class RenderableGPUParticleBufferSet :
		public ParticleBufferSet,
		public GPUParticleBufferSet,
		public ParticleBufferSetRenderData
	{
	public:
		RenderableGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue);

		void Initialize(ArrayView<DynamicParticle> dynamicParticles) override;

		Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() override { return dynamicParticleBufferGL; }
		cl::Buffer& GetDynamicParticleBufferCL() override { return dynamicParticleBufferCL; }
		
		void StartSimulationRead() override;
		void StartSimulationWrite() override;
		void EndSimulationRead() override;
		void EndSimulationWrite() override;
	private:
		OpenCLContext& clContext;

		Graphics::OpenGLWrapper::GraphicsBuffer dynamicParticleBufferGL;
		cl::Buffer dynamicParticleBufferCL;
		void* dynamicParticleBufferMap;

		uintMem dynamicParticleCount;		

		static void RenderStart(void* userData);
	};
	*/
}