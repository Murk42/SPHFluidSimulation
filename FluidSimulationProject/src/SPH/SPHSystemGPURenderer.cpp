#include "pch.h"
#include "SPHSystemGPURenderer.h"
#include "RenderingSystem.h"

SPHSystemGPURenderer::SPHSystemGPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	: graphicsContext(graphicsContext)
{
	Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "shaders/particle.frag" };
	Graphics::OpenGLWrapper::VertexShader vertexShader{ "shaders/particle.vert" };
	shaderProgram.LinkShaders({ &fragmentShader, &vertexShader });
}

SPHSystemGPURenderer::~SPHSystemGPURenderer()
{
}

void SPHSystemGPURenderer::Render(SPHSystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
{
	SPH::SystemGPU* system = (SPH::SystemGPU*)renderCache.GetSystem();
	system->StartRender(graphicsContext);
	graphicsContext.SelectProgram(&shaderProgram);	

	shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
	shaderProgram.SetUniform(1, projMatrix);

	graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, system->GetParticleCount());
	system->EndRender();
}