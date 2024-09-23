#include "pch.h"
#include "SPH/Graphics/SystemCPURenderer.h"
#include "RenderingSystem.h"

namespace SPH
{	
	SystemCPURenderer::SystemCPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "shaders/particle.frag" };
		Graphics::OpenGLWrapper::VertexShader vertexShader{ "shaders/particle.vert" };
		shaderProgram.LinkShaders({ &fragmentShader, &vertexShader });
	}

	SystemCPURenderer::~SystemCPURenderer()
	{
	}

	void SystemCPURenderer::Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
	{
		SPH::SystemCPU* system = (SPH::SystemCPU*)renderCache.GetSystem();

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
		shaderProgram.SetUniform(3, Vec4f(0.5, 0, 0, 0.7));
		graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetStaticParticleCount());

		system->EndRender();
	}
}