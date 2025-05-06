#pragma once
#include "BlazeEngineCore/DataStructures/Color.h"
#include "BlazeEngineCore/Math/Matrix.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLVertexArray.h"
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLProgram.h"
#include "BlazeEngineGraphics/Core/OpenGL/GraphicsContext_OpenGL.h"
using namespace Blaze;

#include "SPH/Core/System.h"
#include "SPH/Core/ParticleBufferManagerGL.h"

namespace SPH
{
	class SystemRenderCache
	{
	public:		
		SystemRenderCache();

		void SetParticleBufferManagerRenderData(SPH::ParticleBufferManagerGL& renderData, uintMem particleByteSize);

		void SetModelMatrix(Mat4f modelMatrix) { this->modelMatrix = modelMatrix; }
		void SetParticleColor(ColorRGBAf particleColor) { this->particleColor = particleColor; }
		void SetParticleSize(float particleSize) { this->particleSize = particleSize; }
		
		const Mat4f& GetModelMatrix() const { return modelMatrix; }
		float GetParticleSize() const { return particleSize; }
		ColorRGBAf GetParticleColor() const { return particleColor; }
		ResourceLockGuard GetVertexArray(Graphics::OpenGLWrapper::VertexArray*& VA);
		uintMem GetParticleCount() const { return renderData->GetBufferSize() / particleByteSize; }

		bool Empty() { return renderData == nullptr || renderData->GetBufferSize() == 0; }
	private:		
		Array<Graphics::OpenGLWrapper::VertexArray> VAs;
		SPH::ParticleBufferManagerGL* renderData;		
		uintMem particleByteSize;

		Mat4f modelMatrix;
		ColorRGBAf particleColor;
		float particleSize;		
	};

	class SystemRenderer
	{
	public:
		SystemRenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		~SystemRenderer() { }		

		template<typename T>
		void SetUniform(uintMem index, const T& value)
		{
			shaderProgram.SetUniform(index, value);
		}

		void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix);
	protected:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGLWrapper::ShaderProgram shaderProgram;
	};
}