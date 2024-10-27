#pragma once
#include "SPH/System/System.h"
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"

namespace SPH
{	
	class CPUSync
	{
	public:
		CPUSync() 	
		{
			active.clear();
		}

		void MarkStart()
		{			
			active.test_and_set();			
		}
		void MarkEnd()
		{			
			active.clear();			
			active.notify_all();			
		}

		void WaitInactive()
		{			
			active.wait(true);			
		}
	private:		
		std::atomic_flag active;
	};

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
		virtual CPUSync& GetReadSync() = 0;

		virtual const DynamicParticle* GetReadBuffer() = 0;
	};
	class CPUParticleWriteBufferHandle
	{
	public:		
		virtual CPUSync& GetWriteSync() = 0;		
		virtual DynamicParticle* GetWriteBuffer() = 0;
	};
}