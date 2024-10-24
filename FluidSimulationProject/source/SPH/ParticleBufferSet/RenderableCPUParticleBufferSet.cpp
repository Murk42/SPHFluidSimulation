#include "pch.h"
#include "RenderableCPUParticleBufferSet.h"

namespace SPH
{
	RenderableCPUParticleBufferSet::RenderableCPUParticleBufferSet() :
		intermediateBuffer(nullptr, 0)
	{
	}
	void RenderableCPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers = Array<Buffer>(dynamicParticleBufferCount, [&](auto* ptr, uintMem index) {
			std::construct_at(ptr, dynamicParticles.Ptr(), dynamicParticles.Count());
			});
		intermediateBuffer.Swap(Buffer(nullptr, dynamicParticles.Count()));		
						
		currentBuffer = dynamicParticleBufferCount - 1;
	}
	void RenderableCPUParticleBufferSet::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	CPUParticleReadBufferHandle& RenderableCPUParticleBufferSet::GetReadBufferHandle()
	{				
		return buffers[currentBuffer];	
	}	
	CPUParticleWriteBufferHandle& RenderableCPUParticleBufferSet::GetWriteBufferHandle()
	{		
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}	
	ParticleRenderBufferHandle& RenderableCPUParticleBufferSet::GetRenderBufferHandle()
	{				
		return buffers[currentBuffer];
	}	
	uintMem RenderableCPUParticleBufferSet::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	void RenderableCPUParticleBufferSet::ReorderParticles()
	{		
	}
	RenderableCPUParticleBufferSet::Buffer::Buffer(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticlesCount) :
		writeFinished(true), readCounter(0), dynamicParticleMap(nullptr)
	{
		if (dynamicParticlesCount == 0)
			return;

		dynamicParticlesBuffer.Allocate(
			dynamicParticlesPtr, sizeof(DynamicParticle) * dynamicParticlesCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentCoherent
		);
		dynamicParticleMap = (DynamicParticle*)dynamicParticlesBuffer.MapBufferRange(
			0, sizeof(DynamicParticle) * dynamicParticlesCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::None
		);

		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &dynamicParticlesBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);
		
		readCounter = 0;		
	}	
	
	static std::atomic_bool writeStarted = false;
	void RenderableCPUParticleBufferSet::Buffer::StartRead()
	{		
		std::unique_lock lk{ stateMutex };		

		stateCV.wait(lk, [&]() { return writeFinished; });
		readCounter++;				
	}
	void RenderableCPUParticleBufferSet::Buffer::FinishRead()
	{
		std::unique_lock lk{ stateMutex };

		readCounter--;
		stateCV.notify_all();
	}
	void RenderableCPUParticleBufferSet::Buffer::StartWrite()
	{	
		std::unique_lock lk{ stateMutex };

		assert(!writeStarted);

		stateCV.wait(lk, [&]() { return writeFinished; });

		writeFinished = false;

		stateCV.wait(lk, [&]() { return readCounter == 0; });
			
		auto fenceState = readFinishedFence.BlockClient(2);

		switch (fenceState)
		{
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::FenceNotSet:
			break;
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::Error:
			Debug::Logger::LogWarning("Client", "System simulation fence error");
			break;
		default:
			Debug::Logger::LogWarning("Client", "Invalid FenceReturnState enum value");
			break;
		}
		readFinishedFence = Graphics::OpenGLWrapper::Fence();

		writeStarted = true;
	}
	void RenderableCPUParticleBufferSet::Buffer::FinishWrite()
	{
		std::unique_lock lk{ stateMutex };

		assert(writeStarted);

		writeFinished = true;		
		stateCV.notify_all();

		writeStarted = false;
	}
	void RenderableCPUParticleBufferSet::Buffer::StartRender()
	{
		StartRead();
	}
	void RenderableCPUParticleBufferSet::Buffer::FinishRender()
	{
		FinishRead();
	}
	void RenderableCPUParticleBufferSet::Buffer::Swap(Buffer&& other)
	{		
		std::swap(dynamicParticleVertexArray, other.dynamicParticleVertexArray);
		std::swap(dynamicParticlesBuffer, other.dynamicParticlesBuffer);
		std::swap(dynamicParticleMap, other.dynamicParticleMap);
	}
}