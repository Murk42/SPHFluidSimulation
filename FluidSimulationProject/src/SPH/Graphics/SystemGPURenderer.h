#pragma once
#include "SPH/System/SystemGPU.h"
#include "SPH/Graphics/SystemRenderer.h"

namespace SPH
{
	class SystemGPURenderCache : public SystemRenderCache
	{
	public:
		void LinkSPHSystem(SPH::System* system) override { this->system = dynamic_cast<SPH::SystemGPU*>(system); }
		void SetModelMatrix(Mat4f modelMatrix) override { this->modelMatrix = modelMatrix; }

		const Mat4f& GetModelMatrix() const override { return modelMatrix; }
		SPH::System* GetSystem() const override { return system; }
	private:
		SPH::SystemGPU* system;
		Mat4f modelMatrix;
	};

	class SystemGPURenderer : public SystemRenderer
	{
	public:
		SystemGPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		~SystemGPURenderer();

		void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix) override;
	private:
		Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

		Graphics::OpenGLWrapper::ShaderProgram shaderProgram;
	};
}