#include "pch.h"
#include "SystemGPURenderer.h"
#include "RenderingSystem.h"

namespace SPH
{
	SystemGPURenderer::SystemGPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "shaders/particle.frag" };
		Graphics::OpenGLWrapper::VertexShader vertexShader{ "shaders/particle.vert" };
		shaderProgram.LinkShaders({ &fragmentShader, &vertexShader });
	}

	SystemGPURenderer::~SystemGPURenderer()
	{
	}

	void SystemGPURenderer::Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
	{
		SPH::SystemGPU* system = (SPH::SystemGPU*)renderCache.GetSystem();

		system->StartRender(graphicsContext);

		graphicsContext.SelectProgram(&shaderProgram);
		shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
		shaderProgram.SetUniform(1, projMatrix);

		graphicsContext.SelectVertexArray(&system->GetDynamicParticlesVertexArray());
		shaderProgram.SetUniform(2, 0.05f);
		shaderProgram.SetUniform(3, Vec4f(1, 1, 1, 1));
		graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetDynamicParticleCount());

		graphicsContext.SelectVertexArray(&system->GetStaticParticlesVertexArray());
		shaderProgram.SetUniform(2, 0.01f);
		shaderProgram.SetUniform(3, Vec4f(0.5f, 0.0f, 0.0f, 0.7f));
		graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetStaticParticleCount());

		system->EndRender();
	}
}