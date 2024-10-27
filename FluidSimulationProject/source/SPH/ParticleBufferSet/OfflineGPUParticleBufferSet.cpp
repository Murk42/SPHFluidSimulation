#include "pch.h"
#include "OfflineGPUParticleBufferSet.h"
#include "OpenCLContext.h"

namespace SPH
{
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence);

	OfflineGPUParticleBufferSet::OfflineGPUParticleBufferSet(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), currentBuffer(0), dynamicParticleCount(0)
	{
	}
	void OfflineGPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
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
	void OfflineGPUParticleBufferSet::Clear()
	{
		buffers.Clear();
		currentBuffer = 0;
		dynamicParticleCount = 0;
	}
	void OfflineGPUParticleBufferSet::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	GPUParticleReadBufferHandle& OfflineGPUParticleBufferSet::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	GPUParticleWriteBufferHandle& OfflineGPUParticleBufferSet::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}	
	uintMem OfflineGPUParticleBufferSet::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}

	OfflineGPUParticleBufferSet::Buffer::Buffer(OpenCLContext& clContext, cl::CommandQueue& queue) :
		clContext(clContext), queue(queue), dynamicParticleCount(0)
	{
	}
	void OfflineGPUParticleBufferSet::Buffer::Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount)
	{
		cl_int ret;		

		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (copyFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());

		if (readFinishedEvent() != nullptr)
			CL_CALL(readFinishedEvent.wait());

		this->dynamicParticleCount = dynamicParticleCount;		
		
		dynamicParticleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | (dynamicParticlesPtr != nullptr ? CL_MEM_COPY_HOST_PTR : 0), sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticlesPtr, &ret);
		CL_CHECK();		
	}
	void OfflineGPUParticleBufferSet::Buffer::StartRead(cl_event* finishedEvent)
	{
		cl_int ret;
				
		if (writeFinishedEvent() != nullptr)
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), 1, &writeFinishedEvent(), finishedEvent));		
	}
	void OfflineGPUParticleBufferSet::Buffer::FinishRead(ArrayView<cl_event> waitEvents)
	{
		cl_int ret;
		
		readFinishedEvent = cl::Event();
		if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent()));
	}
	void OfflineGPUParticleBufferSet::Buffer::StartWrite(cl_event* finishedEvent)
	{
		cl_int ret;			

		uintMem count = 0;
		cl_event waitEvents[3]{ };
		if (copyFinishedEvent()) waitEvents[count++] = copyFinishedEvent();
		if (writeFinishedEvent()) waitEvents[count++] = writeFinishedEvent();
		if (readFinishedEvent()) waitEvents[count++] = readFinishedEvent();

		if (count != 0)
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), count, count == 0 ? nullptr : waitEvents, finishedEvent));
	}
	void OfflineGPUParticleBufferSet::Buffer::FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering)
	{
		cl_int ret;

		writeFinishedEvent = cl::Event();
		if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(queue(), waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent()));
	}	
}