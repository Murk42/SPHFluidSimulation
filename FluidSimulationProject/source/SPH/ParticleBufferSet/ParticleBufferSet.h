#pragma once
#include "BlazeEngineCore/BlazeEngineCore.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{
	class ParticleBufferSet
	{
	public:
		virtual ~ParticleBufferSet() { }

		virtual void Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles) = 0;
		virtual void Advance() = 0;		

		virtual void ReorderParticles() = 0;

		virtual uintMem GetDynamicParticleCount() = 0;
	};
}