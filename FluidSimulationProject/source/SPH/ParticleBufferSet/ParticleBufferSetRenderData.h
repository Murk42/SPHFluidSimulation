#pragma once
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"

namespace SPH
{	
	class ParticleRenderBufferHandle;
	class ParticleBufferSetRenderData
	{
	public:
		virtual ParticleRenderBufferHandle& GetRenderBufferHandle() = 0;
	};

	class ParticleRenderBufferHandle
	{
	public:
		virtual void StartRender() = 0;
		virtual void FinishRender() = 0;		
		virtual Graphics::OpenGLWrapper::VertexArray& GetVertexArray() = 0;
	};
}