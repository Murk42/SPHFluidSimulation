#pragma once
#include "BlazeEngineCore/BlazeEngineCore.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{	
	class ParticleBufferManager
	{
	public:
		virtual ~ParticleBufferManager() { }
				
		virtual void Clear() = 0;
		virtual void Advance() = 0;	

		virtual void AllocateDynamicParticles(uintMem count) = 0;
		virtual void AllocateStaticParticles(uintMem count) = 0;

		virtual uintMem GetDynamicParticleCount() = 0;
		virtual uintMem GetStaticParticleCount() = 0;		
	};		
}