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
		{
			CL_CALL(writeFinishedEvent.wait());
			writeFinishedEvent = cl::Event();
		}

		if (copyFinishedEvent() != nullptr)
		{
			CL_CALL(writeFinishedEvent.wait());
			copyFinishedEvent = cl::Event();
		}

		if (readFinishedEvent() != nullptr)
		{
			CL_CALL(readFinishedEvent.wait());
			readFinishedEvent = cl::Event();
		}
	

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
	void RenderableGPUParticleBufferSet::Buffer::StartRead(cl_event* finishedEvent)
	{
		cl_int ret;
		
		if (clContext.supportedCLGLInterop)
		{
			CL_CALL(clEnqueueAcquireGLObjects(queue(), 1, &dynamicParticleBufferCL(), writeFinishedEvent() != nullptr ? 1 : 0, writeFinishedEvent() != nullptr ? &writeFinishedEvent() : nullptr, finishedEvent));
		}
		else if (writeFinishedEvent() != nullptr)
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), 1, &writeFinishedEvent(), finishedEvent));
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishRead(ArrayView<cl_event> waitEvents)
	{
		cl_int ret;

		readFinishedEvent = cl::Event();		
		if (clContext.supportedCLGLInterop)
		{			
			CL_CALL(clEnqueueReleaseGLObjects(queue(), 1, &dynamicParticleBufferCL(), waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent()));
		}
		else if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent()));					
	}
	void RenderableGPUParticleBufferSet::Buffer::StartWrite(cl_event* finishedEvent)
	{
		cl_int ret;

		WaitFence(renderingFence);		

		uintMem count = 0;
		cl_event waitEvents[3]{ };
		if (copyFinishedEvent()) waitEvents[count++] = copyFinishedEvent();
		if (writeFinishedEvent()) waitEvents[count++] = writeFinishedEvent();
		if (readFinishedEvent()) waitEvents[count++] = readFinishedEvent();

		if (clContext.supportedCLGLInterop)
		{
			CL_CALL(clEnqueueAcquireGLObjects(queue(), 1, &dynamicParticleBufferCL(), count, count == 0 ? nullptr : waitEvents, finishedEvent));
		}
		else if (count != 0)
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), count, count == 0 ? nullptr : waitEvents, finishedEvent));
	}
	void RenderableGPUParticleBufferSet::Buffer::FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering)
	{
		cl_int ret;

		writeFinishedEvent = cl::Event();				
		if (clContext.supportedCLGLInterop)
		{
			CL_CALL(clEnqueueReleaseGLObjects(queue(), 1, &dynamicParticleBufferCL(), waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent()));
		}
		else if (prepareForRendering)
		{
			CL_CALL(clEnqueueReadBuffer(queue(), dynamicParticleBufferCL(), CL_FALSE, 0, sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticleBufferMap, waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent()))
		}
		else if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent()));		
	}
	void RenderableGPUParticleBufferSet::Buffer::StartRender()
	{
		cl_int ret;
		
		if (clContext.supportedCLGLInterop)
		{
			if (writeFinishedEvent() != nullptr)			
				CL_CALL(writeFinishedEvent.wait())							
		}
		else
		{			
			if (copyFinishedEvent() != nullptr)			
				CL_CALL(copyFinishedEvent.wait())				

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