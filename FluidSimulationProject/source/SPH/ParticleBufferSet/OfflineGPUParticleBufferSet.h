#pragma once
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"
#include "SPH/ParticleBufferSet/GPUParticleBufferSet.h"
#include "SPH/System/System.h"

class OpenCLContext;

namespace SPH
{
	class OfflineGPUParticleBufferSet :
		public GPUParticleBufferSet
	{
	public:
		OfflineGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue);

		void Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles) override;
		void Clear() override;
		void Advance() override;

		GPUParticleReadBufferHandle& GetReadBufferHandle() override;
		GPUParticleWriteBufferHandle& GetWriteBufferHandle() override;		

		uintMem GetDynamicParticleCount() override;
	private:
		class Buffer :
			public GPUParticleReadBufferHandle,
			public GPUParticleWriteBufferHandle
		{
			OpenCLContext& clContext;
			cl::CommandQueue& queue;			
			
			cl::Buffer dynamicParticleBufferCL;			

			cl::Event readFinishedEvent;
			cl::Event writeFinishedEvent;
			cl::Event copyFinishedEvent;			

			uintMem dynamicParticleCount;
		public:
			Buffer(OpenCLContext& clContext, cl::CommandQueue& queue);

			void Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount);

			void StartRead(cl_event* finishedEvent) override;
			void FinishRead(ArrayView<cl_event> waitEvents) override;
			void StartWrite(cl_event* finishedEvent) override;
			void FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering) override;

			cl::Buffer& GetReadBuffer() override { return dynamicParticleBufferCL; }
			cl::Buffer& GetWriteBuffer() override { return dynamicParticleBufferCL; }			
		};

		OpenCLContext& clContext;
		cl::CommandQueue& queue;

		Array<Buffer> buffers;
		uintMem currentBuffer;

		uintMem dynamicParticleCount;
	};
}