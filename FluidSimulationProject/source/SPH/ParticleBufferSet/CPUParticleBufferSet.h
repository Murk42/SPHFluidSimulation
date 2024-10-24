#pragma once
#include "SPH/System/System.h"
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"

namespace SPH
{	
	class CPUParticleReadBufferHandle;
	class CPUParticleWriteBufferHandle;

	class CPUParticleBufferSet :
		public ParticleBufferSet
	{
	public:		
		virtual CPUParticleReadBufferHandle& GetReadBufferHandle() = 0;
		virtual CPUParticleWriteBufferHandle& GetWriteBufferHandle() = 0;
	};	

	class CPUParticleReadBufferHandle
	{
	public:
		virtual void StartRead() = 0;
		virtual void FinishRead() = 0;

		virtual const DynamicParticle* GetReadBuffer() = 0;
	};
	class CPUParticleWriteBufferHandle
	{
	public:		
		virtual void StartWrite() = 0;
		virtual void FinishWrite() = 0;
		virtual DynamicParticle* GetWriteBuffer() = 0;
	};
}