#include "pch.h"
#include "SystemRenderer.h"

namespace SPH
{
	SystemRenderCache::SystemRenderCache() :
		system(nullptr)
	{		
		staticParticleGeneratedEventHandler.SetFunction([&](auto event) {

			staticParticlesBuffer = decltype(staticParticlesBuffer)();
			staticParticlesBuffer.Allocate(event.staticParticles.Ptr(), event.staticParticles.Count() * sizeof(StaticParticle));
			staticParticleVertexArray.SetVertexAttributeBuffer(0, &staticParticlesBuffer, sizeof(StaticParticle), 0);
			});		

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);		
	}
	void SystemRenderCache::LinkSPHSystem(SPH::System* system, SPH::ParticleBufferSetRenderData& renderData)
	{		
		this->system = system;
		this->renderData = &renderData;
		system->staticParticlesGeneratedEventDispatcher.AddHandler(staticParticleGeneratedEventHandler);		
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
		
		{
			auto& renderBufferHandle = renderCache.renderData->GetRenderBufferHandle();

			renderBufferHandle.StartRender();
			graphicsContext.SelectVertexArray(&renderBufferHandle.GetVertexArray());
			shaderProgram.SetUniform(2, 0.10f);
			shaderProgram.SetUniform(3, dynamicParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.system->GetDynamicParticleCount());

			renderBufferHandle.FinishRender();			
		}
		
		//graphicsContext.SelectVertexArray(&renderCache.staticParticleVertexArray);
		//shaderProgram.SetUniform(2, 0.03f);
		//shaderProgram.SetUniform(3, staticParticleColor);
		//graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.system->GetStaticParticleCount());				
	}	
}