#include "pch.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLShader.h"
#include "SPH/Graphics/SystemRenderer.h"
#include "SPH/Graphics/Shaders/Shaders.h"

namespace SPH
{
	SystemRenderCache::SystemRenderCache() : renderData(nullptr), particleSize(0)
	{				
	}
	void SystemRenderCache::SetParticleBufferManagerRenderData(SPH::ParticleBufferManagerGL& renderData, uintMem particleByteSize)
	{
		this->renderData = &renderData;		
		this->particleByteSize = particleByteSize;		
		
		VAs.Resize(renderData.GetBufferCount());
		
		uintMem bufferOffset;
		Graphics::OpenGLWrapper::GraphicsBuffer* buffer;

		for (uintMem i = 0; i < renderData.GetBufferCount(); ++i)
		{
			auto& va = VAs[i];

			buffer = renderData.GetGraphicsBuffer(i, bufferOffset);

			va.EnableVertexAttribute(0);
			va.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
			va.SetVertexAttributeBuffer(0, buffer, particleByteSize, bufferOffset);
			va.SetVertexAttributeDivisor(0, 1);
			va.EnableVertexAttribute(1);
			va.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
			va.SetVertexAttributeBuffer(1, buffer, particleByteSize, bufferOffset);
			va.SetVertexAttributeDivisor(1, 1);
			va.EnableVertexAttribute(2);
			va.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
			va.SetVertexAttributeBuffer(2, buffer, particleByteSize, bufferOffset);
			va.SetVertexAttributeDivisor(2, 1);
			va.EnableVertexAttribute(3);
			va.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, offsetof(DynamicParticle, hash));
			va.SetVertexAttributeBuffer(3, buffer, particleByteSize, bufferOffset);
			va.SetVertexAttributeDivisor(3, 1);
		}
	}	
	
	ResourceLockGuard SystemRenderCache::GetVertexArray(Graphics::OpenGLWrapper::VertexArray*& VA)
	{
		auto lockGuard = renderData->LockForRendering(nullptr);
		uintMem bufferIndex = (uintMem)lockGuard.GetResource();				
		VA = &VAs[bufferIndex];
		return lockGuard;
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

	void SystemRenderer::Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
	{
		if (renderCache.Empty())
			return;						

		graphicsContext.SelectProgram(&shaderProgram);
		shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
		shaderProgram.SetUniform(1, projMatrix);
		shaderProgram.SetUniform(2, renderCache.GetParticleSize());
		shaderProgram.SetUniform(3, (Vec4f)renderCache.GetParticleColor());
		
		Graphics::OpenGLWrapper::VertexArray* VA;
		auto lockGuard = renderCache.GetVertexArray(VA);

		graphicsContext.SelectVertexArray(VA);
		graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.GetParticleCount());

		lockGuard.Unlock({});
	}
}