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
		virtual void StartRead() = 0;
		virtual void FinishRead() = 0;

		virtual cl::Buffer& GetReadBuffer() = 0;
	};
	class GPUParticleWriteBufferHandle
	{
	public:
		virtual void StartWrite() = 0;
		virtual void FinishWrite(bool prepareForRendering) = 0;
		virtual cl::Buffer& GetWriteBuffer() = 0;
	};	
}