#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableGPUParticleBufferManager.h"
#include "GL/glew.h"
#include <CL/cl.h>
#include "OpenCLDebug.h"

namespace SPH
{	
	static void WaitForAndClearFence(Graphics::OpenGLWrapper::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);
		switch (fenceState)
		{
		case Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
		case Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
		case Graphics::OpenGLWrapper::FenceReturnState::FenceNotSet:
			break;
		case Graphics::OpenGLWrapper::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;
		case Graphics::OpenGLWrapper::FenceReturnState::Error:
			Debug::Logger::LogFatal("Client", "System simulation fence error");
			break;

		}		

		fence.Clear();
	}

	RenderableGPUParticleBufferManagerWithoutCLGLInterop::RenderableGPUParticleBufferManagerWithoutCLGLInterop(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue)
		: clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), 
		currentBuffer(0), 
		dynamicParticlesBuffers(3, clCommandQueue), dynamicParticlesMemoryCL(NULL), dynamicParticlesCount(0), dynamicParticlesBufferGL(0),
		staticParticlesBuffer(clCommandQueue), staticParticlesMemoryCL(NULL), staticParticlesCount(0), staticParticlesBufferGL(0)
	{
	}
	RenderableGPUParticleBufferManagerWithoutCLGLInterop::~RenderableGPUParticleBufferManagerWithoutCLGLInterop()
	{
		Clear();
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::Clear()
	{
		CleanDynamicParticlesBuffers();
		CleanStaticParticlesBuffer();
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::Advance()
	{
		currentBuffer = (currentBuffer + 1) % dynamicParticlesBuffers.Count();
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::AllocateDynamicParticles(uintMem count, DynamicParticle* particles)
	{
		CleanDynamicParticlesBuffers();

		if (count == 0)			
			return;

		currentBuffer = 0;
		dynamicParticlesCount = count;

		uint subBufferAlign = 0;
		CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(uint), &subBufferAlign, NULL));
		uintMem subBufferSize = (sizeof(DynamicParticle) * count + subBufferAlign - 1) / subBufferAlign * subBufferAlign; //Round up to a multiple of subBufferAlign

		//The OpenGL buffers aren't created so they need to be created
		dynamicParticlesBufferGL = decltype(dynamicParticlesBufferGL)();

		AllocateBuffers(sizeof(DynamicParticle) * count, subBufferSize * dynamicParticlesBuffers.Count(), particles, nullptr, dynamicParticlesBufferGL, dynamicParticlesMemoryCL);
		//dynamicParticlesBufferGL.Allocate(nullptr, sizeof(DynamicParticle) * count, Graphics::OpenGLWrapper::GraphicsBufferMapAccessFlags::Write, Graphics::OpenGLWrapper::GraphicsBufferMapType::None);		
		//
		//CL_CHECK_RET(dynamicParticlesMemoryCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE, subBufferSize * dynamicParticlesBuffers.Count(), nullptr, &ret));		
		CL_CALL(clEnqueueWriteBuffer(clCommandQueue, dynamicParticlesMemoryCL, CL_TRUE, 0, subBufferSize, particles, 0, nullptr, nullptr));

		for (uintMem i = 0; i < dynamicParticlesBuffers.Count(); ++i)
		{
			cl_buffer_region region{
				.origin = subBufferSize * i,
				.size = subBufferSize
			};
			cl_mem subBuffer = NULL;
			CL_CHECK_RET(subBuffer = clCreateSubBuffer(dynamicParticlesMemoryCL, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret));
			
			dynamicParticlesBuffers[i].SetBuffer(subBuffer, false);
		}
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		CleanStaticParticlesBuffer();

		if (count == 0)
			return;

		staticParticlesCount = count;

		staticParticlesBufferGL = decltype(staticParticlesBufferGL)();

		AllocateBuffers(sizeof(StaticParticle) * count, sizeof(StaticParticle) * count, particles, particles, staticParticlesBufferGL, staticParticlesMemoryCL);
		//staticParticlesBufferGL.Allocate(particles, sizeof(StaticParticle) * count, Graphics::OpenGLWrapper::GraphicsBufferMapAccessFlags::Write, Graphics::OpenGLWrapper::GraphicsBufferMapType::None);		
		//
		//CL_CHECK_RET(staticParticlesMemoryCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (particles == nullptr ? 0 : CL_MEM_COPY_HOST_PTR), sizeof(StaticParticle) * count, particles, &ret));
		
		cl_buffer_region region{
				.origin = 0,
				.size = sizeof(StaticParticle) * count
		};
		cl_mem subBuffer = NULL;
		CL_CHECK_RET(subBuffer = clCreateSubBuffer(staticParticlesMemoryCL, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret));

		staticParticlesBuffer.SetBuffer(subBuffer, false);
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetDynamicParticleBufferCount() const
	{
		return dynamicParticlesBuffers.Count();
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetDynamicParticleCount()
	{
		return dynamicParticlesCount;
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetStaticParticleCount()
	{
		return staticParticlesCount;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(DynamicParticle);
		bufferOffset = 0; //index* dynamicParticlesCount * sizeof(DynamicParticle);
		return &dynamicParticlesBufferGL;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(StaticParticle);
		bufferOffset = 0;
		return &staticParticlesBufferGL;
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockDynamicParticlesForRead(void* signalEvent)
	{						
		return dynamicParticlesBuffers[currentBuffer].LockRead((cl_event*)signalEvent, false);		
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockDynamicParticlesForWrite(void* signalEvent)
	{	
		return dynamicParticlesBuffers[currentBuffer].LockWrite((cl_event*)signalEvent, false);		
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockStaticParticlesForRead(void* signalEvent)
	{
		return staticParticlesBuffer.LockRead((cl_event*)signalEvent, false);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockStaticParticlesForWrite(void* signalEvent)
	{
		return staticParticlesBuffer.LockWrite((cl_event*)signalEvent, false);
	}	
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockDynamicParticlesForRendering(void* signalEvent)
	{
		return dynamicParticlesBuffers[currentBuffer].LockForRendering(dynamicParticlesBufferGL, sizeof(DynamicParticle) * dynamicParticlesCount);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockStaticParticlesForRendering(void* signalEvent)
	{
		return staticParticlesBuffer.LockForRendering(staticParticlesBufferGL, sizeof(StaticParticle) * staticParticlesCount);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::PrepareDynamicParticlesForRendering()
	{	
		dynamicParticlesBuffers[currentBuffer].PrepareForRendering(dynamicParticlesBufferGL, sizeof(DynamicParticle) * dynamicParticlesCount);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::PrepareStaticParticlesForRendering()
	{
		staticParticlesBuffer.PrepareForRendering(staticParticlesBufferGL, sizeof(StaticParticle) * staticParticlesCount);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::FlushAllOperations()
	{
		
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::AllocateBuffers(uintMem bufferSizeGL, uintMem bufferSizeCL, void* ptrGL, void* ptrCL, Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, cl_mem& bufferCL)
	{
		bufferGL.Allocate(ptrGL, bufferSizeGL, Graphics::OpenGLWrapper::GraphicsBufferMapAccessFlags::Write, Graphics::OpenGLWrapper::GraphicsBufferMapType::None);

		CL_CHECK_RET(bufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (ptrCL == nullptr ? 0 : CL_MEM_COPY_HOST_PTR), bufferSizeCL, ptrCL, &ret));					
	}

	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::CleanDynamicParticlesBuffers()
	{
		if (dynamicParticlesCount == 0)
			return;

		for (auto& buffer : dynamicParticlesBuffers)
			buffer.SetBuffer(NULL, false);		
				
		dynamicParticlesBufferGL.Release();

		CL_CALL(clReleaseMemObject(dynamicParticlesMemoryCL));
		dynamicParticlesMemoryCL = NULL;

		dynamicParticlesCount = 0;
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::CleanStaticParticlesBuffer()
	{
		if (staticParticlesCount == 0)
			return;
				
		staticParticlesBuffer.SetBuffer(NULL, false);
		
		staticParticlesBufferGL.Release();
		
		CL_CALL(clReleaseMemObject(staticParticlesMemoryCL));
		staticParticlesMemoryCL = NULL;

		staticParticlesCount = 0;
	}			

	RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::ParticlesBuffer(cl_command_queue clCommandQueue)
		: lock(clCommandQueue), copyForRenderingFinishedEvent(NULL), buffer(NULL), copyRequiredForRendering(false)
	{
	}
	RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::~ParticlesBuffer()
	{
		SetBuffer(NULL, false);

		if (copyForRenderingFinishedEvent != NULL)
			CL_CALL(clReleaseEvent(copyForRenderingFinishedEvent));
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::SetBuffer(cl_mem newBuffer, bool copyRequiredForRendering)
	{
		if (buffer != NULL)
		{
			lock.WaitForReadToFinish();
			lock.WaitForWriteToFinish();

			CL_CALL(clReleaseMemObject(buffer));
		}

		this->copyRequiredForRendering = copyRequiredForRendering;
		buffer = newBuffer;
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockRead(cl_event* signalEvent, bool acquireGLBuffers) 
	{	
		if (acquireGLBuffers)
		{
			EventWaitArray<1> lockAcquiredEvent;
			lock.LockRead(lockAcquiredEvent);

			CL_CALL(clEnqueueAcquireGLObjects(lock.GetCommandQueue(), 1, &buffer, lockAcquiredEvent.Count(), lockAcquiredEvent.Ptr(), signalEvent), ResourceLockGuard());
			lockAcquiredEvent.Release();

			return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
				EventWaitArray<1> objectsReleasedEvent;
				clEnqueueReleaseGLObjects(((ParticlesBuffer*)userData)->lock.GetCommandQueue(), 1, &((ParticlesBuffer*)userData)->buffer, waitEvents.Count(), (const cl_event*)waitEvents.Ptr(), objectsReleasedEvent);
				((ParticlesBuffer*)userData)->lock.UnlockRead(objectsReleasedEvent);
				}, buffer, this);
		}
		else
		{			
			lock.LockRead(signalEvent); 

			return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {				
				((ParticlesBuffer*)userData)->lock.UnlockRead({ (cl_event*)waitEvents.Ptr(), waitEvents.Count() });
				}, buffer, this);
		}
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockWrite(cl_event* signalEvent, bool acquireGLBuffers)
	{
		if (acquireGLBuffers)
		{
			EventWaitArray<1> lockAcquiredEvent;
			lock.LockWrite(lockAcquiredEvent);

			CL_CALL(clEnqueueAcquireGLObjects(lock.GetCommandQueue(), 1, &buffer, lockAcquiredEvent.Count(), lockAcquiredEvent.Ptr(), signalEvent), ResourceLockGuard());
			lockAcquiredEvent.Release();

			return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
				EventWaitArray<1> objectsReleasedEvent;
				clEnqueueReleaseGLObjects(((ParticlesBuffer*)userData)->lock.GetCommandQueue(), 1, &((ParticlesBuffer*)userData)->buffer, waitEvents.Count(), (const cl_event*)waitEvents.Ptr(), objectsReleasedEvent);
				((ParticlesBuffer*)userData)->lock.UnlockWrite(objectsReleasedEvent);
				}, buffer, this);
		}
		else
		{
			lock.LockWrite((cl_event*)signalEvent);			

			copyRequiredForRendering = true;

			return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
				((ParticlesBuffer*)userData)->lock.UnlockWrite(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
				}, buffer, this);
		}
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize)
	{		
		PrepareForRendering(bufferGL, bufferSize);

		if (copyForRenderingFinishedEvent != NULL)
		{
			CL_CALL(clWaitForEvents(1, &copyForRenderingFinishedEvent), ResourceLockGuard());
			CL_CALL(clReleaseEvent(copyForRenderingFinishedEvent), ResourceLockGuard());
			copyForRenderingFinishedEvent = NULL;
		}

		return ResourceLockGuard(nullptr, (void*)0, this);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::PrepareForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize)
	{
		if (!copyRequiredForRendering || bufferSize == 0)
			return;

		cl_event lockAcquiredEvent = NULL;
		void* map = bufferGL.MapBufferRange(0, bufferSize, Graphics::OpenGLWrapper::GraphicsBufferMapOptions::InvalidateBuffer);		

		lock.LockRead(&lockAcquiredEvent);		
		CL_CALL(clEnqueueReadBuffer(lock.GetCommandQueue(), buffer, CL_FALSE, 0, bufferSize, map, lockAcquiredEvent == NULL ? 0 : 1, lockAcquiredEvent == NULL ? nullptr : &lockAcquiredEvent, &copyForRenderingFinishedEvent));
		lock.UnlockRead({ copyForRenderingFinishedEvent });

		bufferGL.UnmapBuffer();
		copyRequiredForRendering = false;
	}
}