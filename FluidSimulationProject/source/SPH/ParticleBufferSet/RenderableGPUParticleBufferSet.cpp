#include "pch.h"
#include "RenderableGPUParticleBufferSet.h"
#include "OpenCLContext.h"
#include "GL/glew.h"

namespace SPH
{	
	RenderableGPUParticleBufferSet::RenderableGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), intermediateBuffer(clContext, queue)
	{
	}
	void RenderableGPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers = Array<Buffer>(dynamicParticleBufferCount, [&](Buffer* ptr, uintMem i) {			
			std::construct_at(ptr, clContext, queue, dynamicParticles.Ptr(), dynamicParticles.Count());
			});
		intermediateBuffer.Swap(Buffer(clContext, queue, nullptr, dynamicParticles.Count()));
	
		currentBuffer = dynamicParticleBufferCount - 1;
	}
	void RenderableGPUParticleBufferSet::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	GPUParticleReadBufferHandle& RenderableGPUParticleBufferSet::GetReadBufferHandle()
	{				
		return buffers[currentBuffer];
	}	
	GPUParticleWriteBufferHandle& RenderableGPUParticleBufferSet::GetWriteBufferHandle()
	{			
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}	
	ParticleRenderBufferHandle& RenderableGPUParticleBufferSet::GetRenderBufferHandle()
	{				
		return buffers[currentBuffer];
	}	
	uintMem RenderableGPUParticleBufferSet::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}

	void RenderableGPUParticleBufferSet::ReorderParticles()
	{
	}		
	RenderableGPUParticleBufferSet::Buffer::Buffer(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), dynamicParticleBufferMap(nullptr), dynamicParticleCount(0)
	{
	}
	RenderableGPUParticleBufferSet::Buffer::Buffer(OpenCLContext& clContext, cl::CommandQueue& queue, const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticlesCount) :
		clContext(clContext), queue(queue), dynamicParticleBufferMap(nullptr), dynamicParticleCount(dynamicParticlesCount)
	{
		cl_int ret;
		if (clContext.supportedCLGLInterop)
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * dynamicParticlesCount, dynamicParticlesPtr, GL_DYNAMIC_STORAGE_BIT);
			dynamicParticleBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
			CL_CHECK();
		}
		else
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * dynamicParticlesCount, dynamicParticlesPtr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
			dynamicParticleBufferMap = glMapNamedBufferRange(dynamicParticleBufferGL.GetHandle(), 0, sizeof(DynamicParticle) * dynamicParticlesCount, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

			dynamicParticleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(DynamicParticle) * dynamicParticlesCount, (void*)dynamicParticlesPtr, &ret);
			CL_CHECK();
		}

		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);		
	}
	void RenderableGPUParticleBufferSet::Buffer::StartRead()
	{
		cl_int ret;

		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				dynamicParticleBufferCL()
			};

			CL_CALL(clEnqueueAcquireGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, nullptr));
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishRead()
	{
		cl_int ret;

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				dynamicParticleBufferCL()
			};

			CL_CALL(clEnqueueReleaseGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, &readFinishedEvent()));
		}
		else
		{
			CL_CALL(queue.enqueueBarrierWithWaitList(nullptr, &readFinishedEvent));
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::StartWrite()
	{
		cl_int ret;

		if (readFinishedEvent() != nullptr)
			CL_CALL(readFinishedEvent.wait());

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
		
		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				dynamicParticleBufferCL()
			};

			CL_CALL(clEnqueueAcquireGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, nullptr));
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishWrite()
	{
		cl_int ret;

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				dynamicParticleBufferCL(),
			};

			CL_CALL(clEnqueueReleaseGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, &writeFinishedEvent()));
		}
		else
		{
			CL_CALL(clEnqueueReadBuffer(queue(), dynamicParticleBufferCL(), CL_FALSE, 0, sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticleBufferMap, 0, nullptr, &writeFinishedEvent()))
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::StartRender()
	{
		StartRead();
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishRender()
	{
		FinishRead();
	}
	void RenderableGPUParticleBufferSet::Buffer::Swap(Buffer&& other)
	{
		std::swap(dynamicParticleVertexArray, other.dynamicParticleVertexArray);		
		std::swap(dynamicParticleBufferGL, other.dynamicParticleBufferGL);
		std::swap(dynamicParticleBufferCL, other.dynamicParticleBufferCL);
		std::swap(dynamicParticleBufferMap, other.dynamicParticleBufferMap);
	}
}