#include "pch.h"
#include "SystemRenderer.h"

namespace SPH
{
	SystemRenderer::SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "shaders/particle.frag" };
		Graphics::OpenGLWrapper::VertexShader vertexShader{ "shaders/particle.vert" };
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
		SPH::System* system = renderCache.GetSystem();

		system->StartRender();

		graphicsContext.SelectProgram(&shaderProgram);
		shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
		shaderProgram.SetUniform(1, projMatrix);

		if (auto va = system->GetDynamicParticlesVertexArray())
		{
			graphicsContext.SelectVertexArray(va);
			shaderProgram.SetUniform(2, 0.05f);
			shaderProgram.SetUniform(3, dynamicParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetDynamicParticleCount());
		}

		if (auto va = system->GetStaticParticlesVertexArray())
		{
			graphicsContext.SelectVertexArray(va);
			shaderProgram.SetUniform(2, 0.03f);
			shaderProgram.SetUniform(3, staticParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetStaticParticleCount());
		}

		system->EndRender();
	}
}