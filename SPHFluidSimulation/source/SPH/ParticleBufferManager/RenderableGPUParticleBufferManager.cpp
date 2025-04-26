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
		dynamicParticlesBufferGL.Allocate(nullptr, sizeof(DynamicParticle) * count, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::None);

		CL_CHECK_RET(dynamicParticlesMemoryCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE, subBufferSize * dynamicParticlesBuffers.Count(), nullptr, &ret));
		CL_CALL(clEnqueueWriteBuffer(clCommandQueue, dynamicParticlesMemoryCL, CL_TRUE, 0, subBufferSize, particles, 0, nullptr, nullptr));

		for (uintMem i = 0; i < dynamicParticlesBuffers.Count(); ++i)
		{
			cl_buffer_region region{
				.origin = subBufferSize * i,
				.size = subBufferSize
			};
			cl_mem subBuffer = NULL;
			CL_CHECK_RET(subBuffer = clCreateSubBuffer(dynamicParticlesMemoryCL, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret));
			
			dynamicParticlesBuffers[i].SetBuffer(subBuffer, true);
		}
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		CleanStaticParticlesBuffer();

		if (count == 0)
			return;

		staticParticlesCount = count;

		staticParticlesBufferGL = decltype(staticParticlesBufferGL)();
		staticParticlesBufferGL.Allocate(particles, sizeof(StaticParticle) * count, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::None);		

		CL_CHECK_RET(staticParticlesMemoryCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (particles == nullptr ? 0 : CL_MEM_COPY_HOST_PTR), sizeof(StaticParticle) * count, particles, &ret));
		
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
		return dynamicParticlesBuffers[currentBuffer].LockRead((cl_event*)signalEvent);		
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockDynamicParticlesForWrite(void* signalEvent)
	{	
		return dynamicParticlesBuffers[currentBuffer].LockWrite((cl_event*)signalEvent);		
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockStaticParticlesForRead(void* signalEvent)
	{
		return staticParticlesBuffer.LockRead((cl_event*)signalEvent);		
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockStaticParticlesForWrite(void* signalEvent)
	{
		return staticParticlesBuffer.LockWrite((cl_event*)signalEvent);		
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
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockRead(cl_event* signalEvent)
	{		
		lock.LockRead(signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockRead(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer, &lock);		
	}

	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockWrite(cl_event* signalEvent)
	{
		lock.LockWrite((cl_event*)signalEvent);

		copyRequiredForRendering = true;

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockWrite(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer, &lock);
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
		void* map = bufferGL.MapBufferRange(0, bufferSize, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::InvalidateBuffer);		

		lock.LockRead(&lockAcquiredEvent);		
		CL_CALL(clEnqueueReadBuffer(lock.GetCommandQueue(), buffer, CL_FALSE, 0, bufferSize, map, lockAcquiredEvent == NULL ? 0 : 1, lockAcquiredEvent == NULL ? nullptr : &lockAcquiredEvent, &copyForRenderingFinishedEvent));
		lock.UnlockRead({ copyForRenderingFinishedEvent });

		bufferGL.UnmapBuffer();
		copyRequiredForRendering = false;
	}

	/*
	RenderableGPUParticleBufferManagerWithCLGLInterop::RenderableGPUParticleBufferManagerWithCLGLInterop(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue)
		: clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), staticParticlesLock(clCommandQueue), currentBuffer(0), dynamicParticlesBufferCL(NULL), staticParticlesBufferCL(NULL), dynamicParticlesCount(0), staticParticlesCount(0)
	{
		buffers = Array<DynamicParticlesSubBuffer>(3, [clCommandQueue = clCommandQueue](DynamicParticlesSubBuffer* buffer, uintMem index) {
			std::construct_at(buffer, DynamicParticlesSubBuffer{ NULL, clCommandQueue });
			});

		bool CLGLInteropSupported = CheckForExtensions(clDevice, { "cl_khr_gl_sharing" });
		if (!CLGLInteropSupported)
			Debug::Logger::LogFatal("SPH Library", "Creating a buffer manager with OpenCL-OpenGL interop without the required OpenCL extension cl_khr_gl_sharing");		
	}
	RenderableGPUParticleBufferManagerWithCLGLInterop::~RenderableGPUParticleBufferManagerWithCLGLInterop()
	{
		Clear();
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::Clear()
	{
		CleanDynamicParticlesBuffers();
		CleanStaticParticlesBuffer();
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::AllocateDynamicParticles(uintMem count, DynamicParticle* particles)
	{
		if (count == 0)
			//TODO Clean buffers
			return;


		//TODO Remove clean
		CleanDynamicParticlesBuffers();

		currentBuffer = 0;

		dynamicParticlesCount = count;

		uint subBufferAlign = 0;
		CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(uint), &subBufferAlign, NULL));
		uintMem subBufferSize = (sizeof(DynamicParticle) * count + subBufferAlign - 1) / subBufferAlign * subBufferAlign; //Round up to a multiple of subBufferAlign
		
		dynamicParticlesBufferGL = decltype(dynamicParticlesBufferGL)();
		dynamicParticlesBufferGL.Allocate(nullptr, subBufferSize * buffers.Count());
		dynamicParticlesBufferGL.WriteData(particles, subBufferSize, 0);

		//TODO seperate one big buffers into small ones so each one can be acquired individually
		CL_CHECK_RET(dynamicParticlesBufferCL = clCreateFromGLBuffer(clContext, CL_MEM_READ_WRITE, dynamicParticlesBufferGL.GetHandle(), &ret));

		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			cl_buffer_region region{
				.origin = subBufferSize * i,
				.size = subBufferSize
			};
			CL_CHECK_RET(buffers[i].bufferView = clCreateSubBuffer(dynamicParticlesBufferCL, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret));
		}
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		if (count == 0)
			return;

		CleanStaticParticlesBuffer();

		staticParticlesCount = count;
		
		staticParticlesBufferGL = decltype(staticParticlesBufferGL)();
		staticParticlesBufferGL.Allocate(particles, sizeof(StaticParticle) * count);

		CL_CHECK_RET(staticParticlesBufferCL = clCreateFromGLBuffer(clContext, CL_MEM_READ_WRITE, staticParticlesBufferGL.GetHandle(), &ret));
	}
	uintMem RenderableGPUParticleBufferManagerWithCLGLInterop::GetDynamicParticleBufferCount() const
	{
		return buffers.Count();
	}
	uintMem RenderableGPUParticleBufferManagerWithCLGLInterop::GetDynamicParticleCount()
	{
		return dynamicParticlesCount;
	}
	uintMem RenderableGPUParticleBufferManagerWithCLGLInterop::GetStaticParticleCount()
	{
		return staticParticlesCount;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableGPUParticleBufferManagerWithCLGLInterop::GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(DynamicParticle);
		bufferOffset = index * dynamicParticlesCount * sizeof(DynamicParticle);
		return &dynamicParticlesBufferGL;
	}
	Graphics::OpenGLWrapper::GraphicsBuffer* RenderableGPUParticleBufferManagerWithCLGLInterop::GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset)
	{
		stride = sizeof(StaticParticle);
		bufferOffset = 0;
		return &staticParticlesBufferGL;
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockDynamicParticlesForRead(void* signalEvent)
	{
		//TODO acquire buffer
		auto& buffer = buffers[(currentBuffer + buffers.Count() - 1) % buffers.Count()];
		return CreateResourceLockGuard(buffer.lock, &OpenCLLock::LockRead, (cl_event*)signalEvent, buffer.bufferView);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockDynamicParticlesForWrite(void* signalEvent)
	{
		auto& buffer = buffers[currentBuffer];

		WaitForAndClearFence(buffer.renderingFence);

		return CreateResourceLockGuard(buffer.lock, &OpenCLLock::LockWrite, (cl_event*)signalEvent, buffer.bufferView);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockStaticParticlesForRead(void* signalEvent)
	{
		return CreateResourceLockGuard(staticParticlesLock, &OpenCLLock::LockRead, (cl_event*)signalEvent, staticParticlesBufferCL);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockStaticParticlesForWrite(void* signalEvent)
	{
		WaitForAndClearFence(staticParticlesRenderingFence);

		return CreateResourceLockGuard(staticParticlesLock, &OpenCLLock::LockWrite, (cl_event*)signalEvent, staticParticlesBufferCL);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockDynamicParticlesForRendering(void* signalEvent)
	{
		uintMem index = (currentBuffer + buffers.Count() - 1) % buffers.Count();
		auto& buffer = buffers[index];

		buffer.lock.WaitForWriteToFinish();

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((Graphics::OpenGLWrapper::Fence*)userData)->SetFence();
			}, (void*)index, &buffer.renderingFence);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::LockStaticParticlesForRendering(void* signalEvent)
	{
		staticParticlesLock.WaitForWriteToFinish();

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((Graphics::OpenGLWrapper::Fence*)userData)->SetFence();
			}, nullptr, &staticParticlesRenderingFence);
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::PrepareDynamicParticlesForRendering()
	{
		//TODO acquire buffers
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::PrepareStaticParticlesForRendering()
	{
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithCLGLInterop::CreateResourceLockGuard(OpenCLLock& lock, void(OpenCLLock::* lockFunction)(cl_event*), cl_event* signalEvent, void* resource)
	{
		(lock.*lockFunction)((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->Unlock(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, resource, &lock);
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::CleanDynamicParticlesBuffers()
	{
		if (dynamicParticlesCount == 0)
			return;

		for (auto& buffer : buffers)
		{
			CL_CALL(clReleaseMemObject(buffer.bufferView));

			WaitForAndClearFence(buffer.renderingFence);
			buffer.lock.WaitForReadToFinish();
			buffer.lock.WaitForWriteToFinish();
		}

		dynamicParticlesBufferGL.Release();

		CL_CALL(clReleaseMemObject(dynamicParticlesBufferCL));
		dynamicParticlesBufferCL = NULL;

		dynamicParticlesCount = 0;
	}
	void RenderableGPUParticleBufferManagerWithCLGLInterop::CleanStaticParticlesBuffer()
	{
		if (staticParticlesCount == 0)
			return;

		WaitForAndClearFence(staticParticlesRenderingFence);
		staticParticlesLock.WaitForReadToFinish();
		staticParticlesLock.WaitForWriteToFinish();

		staticParticlesBufferGL.Release();

		CL_CALL(clReleaseMemObject(staticParticlesBufferCL));
		staticParticlesBufferCL = NULL;

		staticParticlesCount = 0;
	}
	*/
}