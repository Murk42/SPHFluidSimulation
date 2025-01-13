#include "pch.h"
#include "SPH/Graphics/SystemRenderer.h"

namespace SPH
{
	SystemRenderCache::SystemRenderCache() :
		system(nullptr)
	{				
	}
	void SystemRenderCache::LinkSPHSystem(SPH::System* system, SPH::ParticleBufferSetRenderData& renderData)
	{		
		this->system = system;
		this->renderData = &renderData;		
	}

	SystemRenderer::SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "assets/shaders/particle.frag" };
		Graphics::OpenGLWrapper::VertexShader vertexShader{ "assets/shaders/particle.vert" };
		shaderProgram.LinkShaders({ &fragmentShader, &vertexShader });		
	}

	void SystemRenderer::SetDynamicParticleColor(ColorRGBAf color)
	{
		dynamicParticleColor = color;
	}

	void SystemRenderer::SetStaticParticleColor(ColorRGBAf color)
	{
		staticParticleColor = color;
	}

	void SystemRenderer::Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
	{
		if (renderCache.system == nullptr)
			return;
		

		graphicsContext.SelectProgram(&shaderProgram);
		shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
		shaderProgram.SetUniform(1, projMatrix);
		
		if (renderCache.system->GetDynamicParticleCount() > 0)
		{
			auto& renderBufferHandle = renderCache.renderData->GetRenderBufferHandle();

			renderBufferHandle.StartRender();
			graphicsContext.SelectVertexArray(&renderBufferHandle.GetVertexArray());
			shaderProgram.SetUniform(2, 0.10f);
			shaderProgram.SetUniform(3, dynamicParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.system->GetDynamicParticleCount());

			renderBufferHandle.FinishRender();			
		}
		
		if (renderCache.system->GetStaticParticleCount() > 0)
		{
			graphicsContext.SelectVertexArray(&renderCache.renderData->GetStaticParticleVertexArray());
			shaderProgram.SetUniform(2, 0.03f);
			shaderProgram.SetUniform(3, staticParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.system->GetStaticParticleCount());
		}
	}
}