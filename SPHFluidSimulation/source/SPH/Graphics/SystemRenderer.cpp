#include "pch.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLShader.h"
#include "SPH/Graphics/SystemRenderer.h"
#include "SPH/Graphics/Shaders/Shaders.h"

namespace SPH
{
	SystemRenderCache::SystemRenderCache() : renderData(nullptr), VAsInitialized(false)
	{				
	}
	void SystemRenderCache::SetParticleBufferManagerRenderData(SPH::ParticleBufferManagerRenderData& renderData)
	{
		this->renderData = &renderData;		
		VAsInitialized = false;
		VAs.Clear();
	}
	void SystemRenderCache::InitializeVAs()
	{
		if (VAsInitialized)
			return;

		VAsInitialized = true;

		VAs.Resize(renderData->GetDynamicParticleBufferCount());

		uintMem stride, bufferOffset;
		Graphics::OpenGLWrapper::GraphicsBuffer* buffer;

		for (uintMem i = 0; i < renderData->GetDynamicParticleBufferCount(); ++i)
		{
			auto& va = VAs[i];

			buffer = renderData->GetDynamicParticlesGraphicsBuffer(i, stride, bufferOffset);

			va.EnableVertexAttribute(0);
			va.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
			va.SetVertexAttributeBuffer(0, buffer, stride, bufferOffset);
			va.SetVertexAttributeDivisor(0, 1);
			va.EnableVertexAttribute(1);
			va.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));			
			va.SetVertexAttributeBuffer(1, buffer, stride, bufferOffset);
			va.SetVertexAttributeDivisor(1, 1);
			va.EnableVertexAttribute(2);
			va.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));			
			va.SetVertexAttributeBuffer(2, buffer, stride, bufferOffset);
			va.SetVertexAttributeDivisor(2, 1);
			va.EnableVertexAttribute(3);
			va.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, offsetof(DynamicParticle, hash));
			va.SetVertexAttributeBuffer(3, buffer, stride, bufferOffset);
			va.SetVertexAttributeDivisor(3, 1);
		}

		buffer = renderData->GetStaticParticlesGraphicsBuffer(stride, bufferOffset);
		staticParticlesVA.EnableVertexAttribute(0);
		staticParticlesVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticlesVA.SetVertexAttributeBuffer(0, buffer, stride, bufferOffset);
		staticParticlesVA.SetVertexAttributeDivisor(0, 1);
	}


	SystemRenderer::SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGLWrapper::FragmentShader fragmentShader;
		fragmentShader.ShaderSource(StringView(particleFragBytes, particleFragSize));
		fragmentShader.CompileShader();
		Graphics::OpenGLWrapper::VertexShader vertexShader;
		vertexShader.ShaderSource(StringView(particleVertBytes, particleVertSize));
		vertexShader.CompileShader();

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
		if (renderCache.renderData == nullptr)
			return;

		renderCache.InitializeVAs();
		
		auto& renderData = *renderCache.renderData;

		graphicsContext.SelectProgram(&shaderProgram);
		shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
		shaderProgram.SetUniform(1, projMatrix);
		
		if (renderData.GetDynamicParticleCount() != 0)
		{
			auto guardLock = renderData.LockDynamicParticlesForRendering(nullptr);
			uintMem bufferIndex = (uintMem)guardLock.GetResource();			
			
			graphicsContext.SelectVertexArray(&renderCache.VAs[bufferIndex]);
			shaderProgram.SetUniform(2, 0.10f);
			shaderProgram.SetUniform(3, dynamicParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderData.GetDynamicParticleCount());

			guardLock.Unlock({});
		}
		
		if (renderData.GetStaticParticleCount() > 0)
		{
			auto guardLock = renderData.LockStaticParticlesForRendering(nullptr);			

			graphicsContext.SelectVertexArray(&renderCache.staticParticlesVA);
			shaderProgram.SetUniform(2, 0.03f);
			shaderProgram.SetUniform(3, staticParticleColor);
			graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderData.GetStaticParticleCount());

			guardLock.Unlock({});
		}
	}
}