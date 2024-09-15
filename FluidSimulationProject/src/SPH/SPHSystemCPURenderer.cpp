#include "pch.h"
#include "SPHSystemCPURenderer.h"
#include "RenderingSystem.h"

SPHSystemCPURenderCache::SPHSystemCPURenderCache(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext) :
	graphicsContext(graphicsContext), instanceMap(nullptr), instanceCount(0), system(nullptr)
{
	vertexArray.EnableVertexAttribute(0);
	vertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, 0);
	vertexArray.SetVertexAttributeBuffer(0, &instanceBuffer, sizeof(Instance), 0);
	vertexArray.SetVertexAttributeDivisor(0, 1);
	vertexArray.EnableVertexAttribute(1);
	vertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 4, false, sizeof(Vec3f));
	vertexArray.SetVertexAttributeBuffer(1, &instanceBuffer, sizeof(Instance), 0);
	vertexArray.SetVertexAttributeDivisor(1, 1);
}

SPHSystemCPURenderCache::~SPHSystemCPURenderCache()
{
	if (instanceMap != nullptr)
		instanceBuffer.UnmapBuffer();
}

void SPHSystemCPURenderCache::LinkSPHSystem(SPH::System* system)
{
	this->system = system;
	RecreateInstanceBuffer(system->GetParticles().Count());
}

void SPHSystemCPURenderCache::SetModelMatrix(Mat4f modelMatrix)
{
	this->modelMatrix = modelMatrix;
}

void SPHSystemCPURenderCache::UpdateParticles(Array<SPH::Particle>& particles)
{
	if (system == nullptr)
	{
		RecreateInstanceBuffer(0);
		return;
	}

	if (instanceCount != particles.Count())
		RecreateInstanceBuffer(system->GetParticles().Count());

	Instance* it = instanceMap;
	for (auto& particle : particles)
	{
		it->pos = particle.position;
		it->color = particle.color;
		++it;
	}
}

void SPHSystemCPURenderCache::FlushParticles()
{
	instanceBuffer.FlushBufferRange(0, sizeof(Instance) * instanceCount);
}

bool SPHSystemCPURenderCache::GetScreenSpacePos(const Mat4f& viewProjMatrix, Vec3f pos, Vec3f& screenPos, Vec2f screenSize)
{
	Vec4f projectedPos = viewProjMatrix * modelMatrix * Vec4f(pos, 1);
	screenPos.x = projectedPos.x / projectedPos.w;
	screenPos.y = projectedPos.y / projectedPos.w;
	screenPos.z = projectedPos.z;

	screenPos.x = (screenPos.x + 1) / 2;
	screenPos.y = (screenPos.y + 1) / 2;
	screenPos.x *= screenSize.x;
	screenPos.y *= screenSize.y;

	return screenPos.z > 0;
}

uintMem SPHSystemCPURenderCache::GetClosestParticleToScreenPos(Vec2f screenPos, RenderingSystem& renderingSystem)
{
	Mat4f viewProjMatrix = renderingSystem.GetProjectionMatrix() * renderingSystem.GetViewMatrix();
	Vec2f screenSize = (Vec2f)renderingSystem.GetWindow().GetSize();

	float minLength = FLT_MAX;
	uintMem closestParticle = SIZE_MAX;
	for (uintMem i = 0; i < instanceCount; ++i)
	{
		Vec3f particleScreenPos;

		if (!GetScreenSpacePos(viewProjMatrix, instanceMap[i].pos, particleScreenPos, screenSize))
			continue;

		float sqrDistance = (screenPos - particleScreenPos.xy()).SqrLenght();

		if (sqrDistance < minLength)
		{
			minLength = sqrDistance;
			closestParticle = i;
		}
	}

	return closestParticle;
}

void SPHSystemCPURenderCache::RecreateInstanceBuffer(uintMem instanceCount)
{
	if (instanceMap != nullptr)
		instanceBuffer.UnmapBuffer();

	this->instanceCount = instanceCount;

	if (instanceCount == 0)
		return;

	instanceBuffer.Allocate(nullptr, instanceCount * sizeof(Instance),
		Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
		Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
	);

	instanceMap = (Instance*)instanceBuffer.MapBufferRange(0, instanceCount * sizeof(Instance),
		Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush |
		Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::InvalidateBuffer |
		Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::Unsynchronized
	);
}

SPHSystemCPURenderer::SPHSystemCPURenderer(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	: graphicsContext(graphicsContext)
{
	Graphics::OpenGLWrapper::FragmentShader fragmentShader{ "shaders/particle.frag" };
	Graphics::OpenGLWrapper::VertexShader vertexShader{ "shaders/particle.vert" };
	shaderProgram.LinkShaders({ &fragmentShader, &vertexShader });
}

SPHSystemCPURenderer::~SPHSystemCPURenderer()
{
}

void SPHSystemCPURenderer::Render(SPHSystemRenderCache& _renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix)
{
	SPHSystemCPURenderCache& renderCache = (SPHSystemCPURenderCache&)_renderCache;
	graphicsContext.SelectProgram(&shaderProgram);
	graphicsContext.SelectVertexArray(&renderCache.GetVertexArray());

	shaderProgram.SetUniform(0, viewMatrix * renderCache.GetModelMatrix());
	shaderProgram.SetUniform(1, projMatrix);

	graphicsContext.RenderInstancedPrimitiveArray(Graphics::OpenGLWrapper::PrimitiveType::TriangleStrip, 0, 4, 0, renderCache.GetInstanceCount());
}