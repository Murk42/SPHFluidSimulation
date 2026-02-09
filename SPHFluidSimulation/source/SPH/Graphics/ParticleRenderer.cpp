#include "pch.h"
#include "BlazeEngine/Graphics/Core/OpenGL/OpenGLWrapper/OpenGLShader.h"
#include "SPH/Graphics/ParticleRenderer.h"
#include "SPH/Graphics/Shaders/Shaders.h"

namespace SPH
{
	ParticleBufferManagerRenderCache::ParticleBufferManagerRenderCache()
		: renderData(nullptr)
	{
	}
	void ParticleBufferManagerRenderCache::SetParticleBufferManagerRenderData(SPH::ParticleBufferManagerGL& renderData)
	{
		this->renderData = &renderData;

		VAs.Resize(renderData.GetBufferCount());

		uintMem bufferOffset;
		Graphics::OpenGL::GraphicsBuffer* buffer;

		for (uintMem i = 0; i < renderData.GetBufferCount(); ++i)
		{
			auto& va = VAs[i];

			buffer = renderData.GetGraphicsBuffer(i, bufferOffset);

			va.EnableVertexAttribute(0);
			va.SetFloatVertexAttributeFormat(0, Graphics::OpenGL::FloatVertexAttributeType::Float, 3, offsetof(DynamicParticle, position));
			va.SetVertexAttributeBuffer(0, buffer, renderData.GetParticleSize(), bufferOffset);
			va.SetVertexAttributeDivisor(0, 1);
			va.EnableVertexAttribute(1);
			va.SetFloatVertexAttributeFormat(1, Graphics::OpenGL::FloatVertexAttributeType::Float, 3, offsetof(DynamicParticle, velocity));
			va.SetVertexAttributeBuffer(1, buffer, renderData.GetParticleSize(), bufferOffset);
			va.SetVertexAttributeDivisor(1, 1);
			va.EnableVertexAttribute(2);
			va.SetFloatVertexAttributeFormat(2, Graphics::OpenGL::FloatVertexAttributeType::Float, 1, offsetof(DynamicParticle, pressure));
			va.SetVertexAttributeBuffer(2, buffer, renderData.GetParticleSize(), bufferOffset);
			va.SetVertexAttributeDivisor(2, 1);
			va.EnableVertexAttribute(3);
			va.SetFloatVertexAttributeFormat(3, Graphics::OpenGL::FloatVertexAttributeType::Uint32, 1, offsetof(DynamicParticle, hash));
			va.SetVertexAttributeBuffer(3, buffer, renderData.GetParticleSize(), bufferOffset);
			va.SetVertexAttributeDivisor(3, 1);
		}
	}
	ResourceLockGuard ParticleBufferManagerRenderCache::GetVertexArray(Graphics::OpenGL::VertexArray*& VA)
	{
		auto lockGuard = renderData->LockForRendering(nullptr);
		uintMem bufferIndex = (uintMem)lockGuard.GetResource();
		VA = &VAs[bufferIndex];
		return lockGuard;
	}

	ParticleRenderer::ParticleRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
		: graphicsContext(graphicsContext)
	{
		Graphics::OpenGL::FragmentShader frag{ ShaderSource::particle_frag };
		Graphics::OpenGL::VertexShader vert{ ShaderSource::particle_vert };
		shaderProgram.LinkShaders({ frag, vert });
	}
	void ParticleRenderer::SetProjectionMatrix(const Mat4f& projectionMatrix)
	{
		shaderProgram.SetUniform(0, projectionMatrix);
	}
	void ParticleRenderer::SetViewMatrix(const Mat4f& viewMatrix)
	{
		shaderProgram.SetUniform(1, viewMatrix);
	}
	void ParticleRenderer::StartRender(const Graphics::RenderContext& renderContext)
	{
		graphicsContext.SelectProgram(&shaderProgram);
	}
	void ParticleRenderer::EndRender(const Graphics::RenderContext& renderContext)
	{
	}
	void ParticleRenderer::Render(ParticleBufferManagerRenderCache& renderCache, const Mat4f& modelMatrix, ColorRGBAf particleColor, float particleSize)
	{
		if (renderCache.Empty())
			return;

		shaderProgram.SetUniform(2, modelMatrix);
		shaderProgram.SetUniform(3, particleSize);
		shaderProgram.SetUniform(4, (Vec4f)particleColor);

		Graphics::OpenGL::VertexArray* VA;
		auto lockGuard = renderCache.GetVertexArray(VA);

		graphicsContext.SelectVertexArray(VA);
		graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGL::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.GetParticleCount());

		lockGuard.Unlock({});
	}
}