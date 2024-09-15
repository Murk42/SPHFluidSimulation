#pragma once
#include "SPHSystemRenderer.h"

class RenderingSystem;

class SPHSystemCPURenderCache : public SPHSystemRenderCache
{
public:
	SPHSystemCPURenderCache(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~SPHSystemCPURenderCache();

	void LinkSPHSystem(SPH::System* system) override;
	void SetModelMatrix(Mat4f modelMatrix) override;

	void UpdateParticles(Array<SPH::Particle>& particles);

	void FlushParticles();

	uintMem GetClosestParticleToScreenPos(Vec2f screenPos, RenderingSystem& renderingSystem);

	const Mat4f& GetModelMatrix() const override { return modelMatrix; }
	SPH::System* GetSystem() const override { return system; }
	uintMem GetInstanceCount() const { return instanceCount; }
	Graphics::OpenGLWrapper::VertexArray& GetVertexArray() { return vertexArray; }
private:
	struct Instance
	{
		Vec3f pos;
		Vec4f color;
	};

	Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

	Graphics::OpenGLWrapper::VertexArray vertexArray;
	Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer instanceBuffer;
	Instance* instanceMap;
	uintMem instanceCount;

	SPH::System* system;

	Mat4f modelMatrix;

	void RecreateInstanceBuffer(uintMem instanceCount);
	bool GetScreenSpacePos(const Mat4f& viewProjMatrix, Vec3f pos, Vec3f& screenPos, Vec2f screenSize);
};

class SPHSystemCPURenderer
{
public:
	SPHSystemCPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~SPHSystemCPURenderer();

	void Render(SPHSystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix);
private:
	Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

	Graphics::OpenGLWrapper::ShaderProgram shaderProgram;
};