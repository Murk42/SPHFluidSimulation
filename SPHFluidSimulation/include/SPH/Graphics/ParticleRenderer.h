#pragma once
#include "BlazeEngine/Core/Common/Color.h"
#include "BlazeEngine/Core/Math/Matrix.h"
#include "BlazeEngine/Graphics/Core/OpenGL/OpenGLWrapper/OpenGLVertexArray.h"
#include "BlazeEngine/Graphics/Core/OpenGL/OpenGLWrapper/OpenGLProgram.h"
#include "BlazeEngine/Graphics/Core/OpenGL/GraphicsContext_OpenGL.h"
using namespace Blaze;

#include "SPH/Core/SimulationEngine.h"
#include "SPH/ParticleBufferManagers/ParticleBufferManagerGL.h"

namespace SPH
{
	class ParticleBufferManagerRenderCache
	{
	public:
		ParticleBufferManagerRenderCache();

		void SetParticleBufferManagerRenderData(ParticleBufferManagerGL& renderData);

		ResourceLockGuard GetVertexArray(Graphics::OpenGL::VertexArray*& VA);
		uintMem GetParticleCount() const { return renderData->GetParticleCount(); }

		inline bool Empty() { return VAs.Empty(); }
	private:
		Array<Graphics::OpenGL::VertexArray> VAs;
		ParticleBufferManagerGL* renderData;
	};

	class ParticleRenderer : public Graphics::RendererBase
	{
	public:
		ParticleRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		~ParticleRenderer() { }

		void SetProjectionMatrix(const Mat4f& projectionMatrix);
		void SetViewMatrix(const Mat4f& viewMatrix);

		void StartRender(const Graphics::RenderContext& renderContext) override;
		void EndRender(const Graphics::RenderContext& renderContext) override;

		void Render(ParticleBufferManagerRenderCache& renderCache, const Mat4f& modelMatrix, ColorRGBAf particleColor, float particleSize);

		uint64 GetTypeID() const override { return RendererBase::GetTypeID<ParticleRenderer>(); }

		Graphics::OpenGL::GraphicsContext_OpenGL& GetGraphicsContext() const override { return graphicsContext; }
	protected:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGL::ShaderProgram shaderProgram;
	};
}