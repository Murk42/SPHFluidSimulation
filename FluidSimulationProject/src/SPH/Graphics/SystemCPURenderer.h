#pragma once
#include "SPH/System/SystemCPU.h"
#include "SPH/Graphics/SystemRenderer.h"

namespace SPH
{
	class SystemCPURenderCache : public SystemRenderCache
	{
	public:
		void LinkSPHSystem(SPH::System* system) override { this->system = dynamic_cast<SPH::SystemCPU*>(system); }
		void SetModelMatrix(Mat4f modelMatrix) override { this->modelMatrix = modelMatrix; }

		const Mat4f& GetModelMatrix() const override { return modelMatrix; }
		SPH::System* GetSystem() const override { return system; }
	private:
		SPH::SystemCPU* system;
		Mat4f modelMatrix;
	};

	class SystemCPURenderer : public SystemRenderer
	{
	public:
		SystemCPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		~SystemCPURenderer();

		void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix);
	private:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGLWrapper::ShaderProgram shaderProgram;
	};
}