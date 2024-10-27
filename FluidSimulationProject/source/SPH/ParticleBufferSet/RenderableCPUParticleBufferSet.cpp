#include "pch.h"
#include "RenderableCPUParticleBufferSet.h"

#include "GL/glew.h"

namespace SPH
{
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);

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
		fence = Graphics::OpenGLWrapper::Fence();
	}
	RenderableCPUParticleBufferSet::RenderableCPUParticleBufferSet()
	{
	}
	void RenderableCPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		if (dynamicParticleBufferCount < 3)
		{
			Debug::Logger::LogWarning("Client", "Buffer count must be at least 3. It is set to 3");
			dynamicParticleBufferCount = 3;
		}
		
		buffers = Array<Buffer>(dynamicParticleBufferCount);

		buffers[0].Initialize(dynamicParticles.Ptr(), dynamicParticles.Count());
		for (uintMem i = 1; i < buffers.Count(); ++i)
			buffers[i].Initialize(nullptr, dynamicParticles.Count());
						
		currentBuffer = dynamicParticleBufferCount - 1;
	}
	void RenderableCPUParticleBufferSet::Clear()
	{
		buffers.Clear();
		currentBuffer = 0;
		dynamicParticleCount = 0;
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
	RenderableCPUParticleBufferSet::Buffer::Buffer() :
		dynamicParticleMap(nullptr), dynamicParticleCount(0)
	{		
	}	
	void RenderableCPUParticleBufferSet::Buffer::Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount)
	{
		{
			std::unique_lock lk{ stateMutex };

			writeSync.WaitInactive();
			readSync.WaitInactive();			
			WaitFence(renderingFinishedFence);
		}

		if (dynamicParticleCount == 0)
			return;

		dynamicParticleBuffer.Allocate(
			dynamicParticlesPtr, sizeof(DynamicParticle) * dynamicParticleCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);
		dynamicParticleMap = (DynamicParticle*)dynamicParticleBuffer.MapBufferRange(
			0, sizeof(DynamicParticle) * dynamicParticleCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush			
		);

		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);

		this->dynamicParticleCount = dynamicParticleCount;
	}
	CPUSync& RenderableCPUParticleBufferSet::Buffer::GetReadSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		readSync.MarkStart();
		return readSync;
	}
	CPUSync& RenderableCPUParticleBufferSet::Buffer::GetWriteSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		WaitRender();
		writeSync.MarkStart();
		return writeSync;
	}	
	void RenderableCPUParticleBufferSet::Buffer::StartRender()
	{		
		std::unique_lock lk{ stateMutex };		
		writeSync.WaitInactive();
		dynamicParticleBuffer.FlushBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount);				
	}
	void RenderableCPUParticleBufferSet::Buffer::FinishRender()
	{				
		renderingFinishedFence.SetFence();			
		dynamicParticleBuffer.Invalidate();
	}
	void RenderableCPUParticleBufferSet::Buffer::WaitRender()
	{				
		WaitFence(renderingFinishedFence);
	}

}