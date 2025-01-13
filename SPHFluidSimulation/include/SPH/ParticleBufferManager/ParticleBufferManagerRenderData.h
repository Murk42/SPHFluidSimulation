#pragma once
#include "SPH/ParticleBufferManager/ParticleBufferManager.h"

namespace SPH
{	
	class ParticleBufferManagerRenderData
	{
	public:
		virtual Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray(uintMem index) = 0;
		virtual Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray() = 0;
	};
}