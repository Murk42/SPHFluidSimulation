#include "pch.h"
#include "RenderableGPUParticleBufferSet.h"
#include "OpenCLContext.h"
#include "GL/glew.h"

namespace SPH
{	
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence);

	RenderableGPUParticleBufferSet::RenderableGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), currentBuffer(0), dynamicParticleCount(0)
	{
	}
	void RenderableGPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		if (dynamicParticleBufferCount < 3)
		{
			Debug::Logger::LogWarning("Client", "Buffer count must be at least 3. It is set to 3");
			dynamicParticleBufferCount = 3;
		}

		buffers = Array<Buffer>(dynamicParticleBufferCount, clContext, queue);

		buffers[0].Initialize(dynamicParticles.Ptr(), dynamicParticles.Count());
		for (uintMem i = 1; i < buffers.Count(); ++i)
			buffers[i].Initialize(nullptr, dynamicParticles.Count());		
	
		currentBuffer = dynamicParticleBufferCount - 1;
	}
	void RenderableGPUParticleBufferSet::Clear()
	{
		buffers.Clear();
		currentBuffer = 0;
		dynamicParticleCount = 0;
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
	
	RenderableGPUParticleBufferSet::Buffer::Buffer(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), dynamicParticleBufferMap(nullptr), dynamicParticleCount(0), dynamicParticleVertexArray(0), dynamicParticleBufferGL(0)
	{		
	}
	void RenderableGPUParticleBufferSet::Buffer::Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount)
	{		
		cl_int ret;

		WaitFence(renderingFence);

		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (copyFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (readFinishedEvent() != nullptr)
			CL_CALL(readFinishedEvent.wait());		

		this->dynamicParticleCount = dynamicParticleCount;
		dynamicParticleBufferGL = decltype(dynamicParticleBufferGL)();
		dynamicParticleVertexArray = decltype(dynamicParticleVertexArray)();

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
		
		if (clContext.supportedCLGLInterop)
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticlesPtr, GL_DYNAMIC_STORAGE_BIT);
			dynamicParticleBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
			CL_CHECK();
		}
		else
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticlesPtr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
			dynamicParticleBufferMap = glMapNamedBufferRange(dynamicParticleBufferGL.GetHandle(), 0, sizeof(DynamicParticle) * dynamicParticleCount, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
			
			dynamicParticleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | (dynamicParticlesPtr != nullptr ? CL_MEM_COPY_HOST_PTR : 0), sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticlesPtr, &ret);
			CL_CHECK();
		}
	}	
	void RenderableGPUParticleBufferSet::Buffer::StartRead()
	{
		cl_int ret;

		if (writeFinishedEvent() != nullptr)
		{
			CL_CALL(writeFinishedEvent.wait());
			writeFinishedEvent = cl::Event();
		}

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
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &readFinishedEvent));
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::StartWrite()
	{
		cl_int ret;

		WaitFence(renderingFence);

		if (copyFinishedEvent() != nullptr)
		{
			CL_CALL(copyFinishedEvent.wait())
			copyFinishedEvent = cl::Event();
		}

		if (writeFinishedEvent() != nullptr)
		{
			CL_CALL(writeFinishedEvent.wait());
			writeFinishedEvent = cl::Event();
		}

		if (readFinishedEvent() != nullptr)
		{
			CL_CALL(readFinishedEvent.wait());
			readFinishedEvent = cl::Event();
		}			

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				dynamicParticleBufferCL()
			};

			CL_CALL(clEnqueueAcquireGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, nullptr));
		}		
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishWrite(bool prepareForRendering)
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
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &writeFinishedEvent));

			if (prepareForRendering)
				CL_CALL(clEnqueueReadBuffer(queue(), dynamicParticleBufferCL(), CL_FALSE, 0, sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticleBufferMap, 1, &writeFinishedEvent(), &copyFinishedEvent()))
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::StartRender()
	{
		cl_int ret;
		
		if (clContext.supportedCLGLInterop)
		{
			if (writeFinishedEvent() != nullptr)
			{
				CL_CALL(writeFinishedEvent.wait())
				writeFinishedEvent = cl::Event();
			}
		}
		else
		{			
			if (copyFinishedEvent() != nullptr)
			{
				CL_CALL(copyFinishedEvent.wait())
				copyFinishedEvent = cl::Event();
			}

			glFlushMappedNamedBufferRange(dynamicParticleBufferGL.GetHandle(), 0, sizeof(DynamicParticle) * dynamicParticleCount);			
			renderingFence.SetFence();
		}
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishRender()
	{	
		if (clContext.supportedCLGLInterop)
			renderingFence.SetFence();

		dynamicParticleBufferGL.Invalidate();
	}
	void RenderableGPUParticleBufferSet::Buffer::WaitRender()
	{		
		WaitFence(renderingFence);
	}	
}