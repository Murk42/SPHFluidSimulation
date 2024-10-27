#pragma once
#include "CL/opencl.hpp"
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"

namespace SPH
{
	class GPUParticleReadBufferHandle;
	class GPUParticleWriteBufferHandle;

	class GPUParticleBufferSet :
		public ParticleBufferSet
	{
	public:
		virtual GPUParticleReadBufferHandle& GetReadBufferHandle() = 0;
		virtual GPUParticleWriteBufferHandle& GetWriteBufferHandle() = 0;		
	};

	class GPUParticleReadBufferHandle
	{
	public:
		virtual void StartRead(cl_event* finishedEvent) = 0;
		virtual void FinishRead(ArrayView<cl_event> waitEvents) = 0;

		virtual cl::Buffer& GetReadBuffer() = 0;
	};
	class GPUParticleWriteBufferHandle
	{
	public:
		virtual void StartWrite(cl_event* finishedEvent) = 0;
		virtual void FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering) = 0;
		virtual cl::Buffer& GetWriteBuffer() = 0;
	};	
}