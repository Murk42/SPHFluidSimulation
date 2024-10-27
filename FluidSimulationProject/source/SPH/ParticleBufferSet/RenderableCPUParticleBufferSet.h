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
		void Clear();
		void Advance() override;
		
		CPUParticleReadBufferHandle& GetReadBufferHandle() override;
		CPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;

		uintMem GetDynamicParticleCount() override;		
	private:
		class Buffer : 
			public CPUParticleReadBufferHandle,
			public CPUParticleWriteBufferHandle,
			public ParticleRenderBufferHandle
		{
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleBuffer;
			DynamicParticle* dynamicParticleMap;						

			CPUSync readSync;
			CPUSync writeSync;	

			std::mutex stateMutex;
			std::condition_variable stateCV;
						
			Graphics::OpenGLWrapper::Fence renderingFinishedFence;			

			uintMem dynamicParticleCount;
		public:
			Buffer();			

			void Initialize(const DynamicParticle* dynamicParticlePtr, uintMem dynamicParticleCount);

			CPUSync& GetReadSync() override;
			CPUSync& GetWriteSync() override;
			void StartRender() override;
			void FinishRender() override;
			void WaitRender() override;

			const DynamicParticle* GetReadBuffer() override { return dynamicParticleMap; }
			DynamicParticle* GetWriteBuffer() override { return dynamicParticleMap; }

			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }			
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		uintMem dynamicParticleCount;
	};
}