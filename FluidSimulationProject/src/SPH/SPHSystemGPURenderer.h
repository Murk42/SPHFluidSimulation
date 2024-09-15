#pragma once
#include "SPHSystemGPU.h"
#include "SPHSystemRenderer.h"

class SPHSystemGPURenderCache : public SPHSystemRenderCache
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

class SPHSystemGPURenderer : public SPHSystemRenderer
{
public:
	SPHSystemGPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~SPHSystemGPURenderer();

	void Render(SPHSystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix) override;
private:
	Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

	Graphics::OpenGLWrapper::ShaderProgram shaderProgram;
};