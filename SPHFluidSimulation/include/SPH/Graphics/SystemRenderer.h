#pragma once
#include "BlazeEngineCore/DataStructures/Color.h"
#include "BlazeEngineCore/Math/Matrix.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLVertexArray.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLProgram.h"
#include "BlazeEngineGraphics/Core/OpenGL/GraphicsContext_OpenGL.h"
using namespace Blaze;

#include "SPH/System/System.h"
#include "SPH/ParticleBufferManager/ParticleBufferManagerRenderData.h"

namespace SPH
{
	class SystemRenderCache
	{
	public:
		SystemRenderCache();

		void SetParticleBufferManagerRenderData(SPH::ParticleBufferManagerRenderData& renderData);
		void SetModelMatrix(Mat4f modelMatrix) { this->modelMatrix = modelMatrix; }

		const Mat4f& GetModelMatrix() const { return modelMatrix; }
	private:		
		Array<Graphics::OpenGLWrapper::VertexArray> VAs;
		Graphics::OpenGLWrapper::VertexArray staticParticlesVA;
		SPH::ParticleBufferManagerRenderData* renderData;		

		Mat4f modelMatrix;

		bool VAsInitialized;

		void InitializeVAs();

		friend class SystemRenderer;
	};

	class SystemRenderer
	{
	public:
		SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		~SystemRenderer() { }

		void SetDynamicParticleColor(ColorRGBAf color);
		void SetStaticParticleColor(ColorRGBAf color);

		template<typename T>
		void SetUniform(uintMem index, const T& value)
		{
			shaderProgram.SetUniform(index, value);
		}

		void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix);
	protected:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGLWrapper::ShaderProgram shaderProgram;

		Vec4f dynamicParticleColor = Vec4f(1.0f);
		Vec4f staticParticleColor = Vec4f(1.0f, 0.0f, 0.0f, 0.5f);
	};
}