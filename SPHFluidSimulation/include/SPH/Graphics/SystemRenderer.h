#pragma once
#include "SPH/System/System.h"
#include "SPH/ParticleBufferSet/ParticleBufferSetRenderData.h"
#include <memory>

namespace SPH
{
	class SystemRenderCache
	{
	public:
		SystemRenderCache();

		void LinkSPHSystem(SPH::System* system, SPH::ParticleBufferSetRenderData& renderData);
		void SetModelMatrix(Mat4f modelMatrix) { this->modelMatrix = modelMatrix; }

		const Mat4f& GetModelMatrix() const { return modelMatrix; }
		SPH::System* GetSystem() const { return system; }
	private:		
		SPH::ParticleBufferSetRenderData* renderData;
		SPH::System* system;

		Mat4f modelMatrix;		

		friend class SystemRenderer;
	};

	class SystemRenderer
	{
	public:
		SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		virtual ~SystemRenderer() { }

		virtual void SetDynamicParticleColor(ColorRGBAf color);		
		virtual void SetStaticParticleColor(ColorRGBAf color);

		virtual void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix);
	protected:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGLWrapper::ShaderProgram shaderProgram;

		Vec4f dynamicParticleColor = Vec4f(1.0f);
		Vec4f staticParticleColor = Vec4f(1.0f, 0.0f, 0.0f, 0.5f);
	};
}