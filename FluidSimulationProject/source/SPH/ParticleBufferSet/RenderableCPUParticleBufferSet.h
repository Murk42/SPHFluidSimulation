#pragma once
#include "SPH/ParticleBufferSet/CPUParticleBufferSet.h"
#include "SPH/ParticleBufferSet/ParticleBufferSetRenderData.h"
#include "SPH/System/System.h"

namespace SPH
{			
	class RenderableCPUParticleBufferSet :		
		public CPUParticleBufferSet,
		public ParticleBufferSetRenderData
	{
	public:					
		RenderableCPUParticleBufferSet();

		void Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles) override;
		void Advance() override;
		
		CPUParticleReadBufferHandle& GetReadBufferHandle() override;
		CPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;

		uintMem GetDynamicParticleCount() override;

		void ReorderParticles() override;
	private:
		class Buffer : 
			public CPUParticleReadBufferHandle,
			public CPUParticleWriteBufferHandle,
			public ParticleRenderBufferHandle
		{
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesBuffer;
			DynamicParticle* dynamicParticleMap;						
			bool writeFinished;
			uint readCounter;
			std::mutex stateMutex;
			std::condition_variable stateCV;
			Graphics::OpenGLWrapper::Fence readFinishedFence;
		public:
			Buffer(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticlesCount);			

			void StartRead() override;
			void StartWrite() override;
			void FinishRead() override;
			void FinishWrite() override;
			void StartRender() override;
			void FinishRender() override;

			const DynamicParticle* GetReadBuffer() override { return dynamicParticleMap; }
			DynamicParticle* GetWriteBuffer() override { return dynamicParticleMap; }

			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }
		
			void Swap(Buffer&& buffer);
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Buffer intermediateBuffer;		

		uintMem dynamicParticleCount;
	};
}