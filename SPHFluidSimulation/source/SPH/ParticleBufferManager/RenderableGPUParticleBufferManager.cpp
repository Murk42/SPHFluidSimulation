#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableGPUParticleBufferManager.h"
#include "GL/glew.h"
#include <CL/cl.h>
#include "OpenCLDebug.h"

namespace SPH
{
	static void WaitForAndClearFence(Graphics::OpenGL::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);
		switch (fenceState)
		{
		case Graphics::OpenGL::FenceReturnState::AlreadySignaled:
		case Graphics::OpenGL::FenceReturnState::ConditionSatisfied:
		case Graphics::OpenGL::FenceReturnState::FenceNotSet:
			break;
		case Graphics::OpenGL::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;
		case Graphics::OpenGL::FenceReturnState::Error:
			Debug::Logger::LogFatal("Client", "System simulation fence error");
			break;

		}

		fence.Clear();
	}

	RenderableGPUParticleBufferManagerWithoutCLGLInterop::RenderableGPUParticleBufferManagerWithoutCLGLInterop(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue)
		: clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue),
		currentBuffer(0), bufferCL(NULL), particleSize(0), particleCount(0), bufferGL(0)
	{
	}
	RenderableGPUParticleBufferManagerWithoutCLGLInterop::~RenderableGPUParticleBufferManagerWithoutCLGLInterop()
	{
		Clear();
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::Clear()
	{
		buffers.Clear();
		bufferGL.Release();

		if (bufferCL != NULL)
		{
			CL_CALL(clReleaseMemObject(bufferCL));
			bufferCL = NULL;
		}

		particleCount = 0;
		particleSize = 0;
		currentBuffer = 0;
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount)
	{
		Clear();

		if (newParticleSize * newParticleCount == 0)
			return;

		if (newBufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		particleCount = newParticleCount;
		particleSize = newParticleSize;

		buffers = Array<ParticlesBuffer>(newBufferCount, clCommandQueue);

		CreateBuffers(particles);
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetParticleCount() const
	{
		return particleCount;
	}
	uintMem RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetParticleSize() const
	{
		return particleSize;
	}
	Graphics::OpenGL::GraphicsBuffer* RenderableGPUParticleBufferManagerWithoutCLGLInterop::GetGraphicsBuffer(uintMem index, uintMem& bufferOffset)
	{
		bufferOffset = 0;
		return &bufferGL;
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockRead(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockRead((cl_event*)signalEvent, false);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockWrite(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockWrite((cl_event*)signalEvent, false);
	}
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::LockForRendering(void* signalEvent)
	{
		if (buffers.Empty())
			return ResourceLockGuard();

		return buffers[currentBuffer].LockForRendering(bufferGL, particleSize * particleCount);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::PrepareForRendering()
	{
		if (buffers.Empty())
			return;

		buffers[currentBuffer].PrepareForRendering(bufferGL, particleSize * particleCount);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::FlushAllOperations()
	{

	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::CreateBuffers(void* particles)
	{
		const uintMem bufferSize = particleSize * particleCount;

		//The OpenGL buffers aren't created so they need to be created
		bufferGL = Graphics::OpenGL::ImmutableMappedGraphicsBuffer();
		bufferGL.Allocate(particles, bufferSize, Graphics::OpenGL::GraphicsBufferMapAccessFlags::Write, Graphics::OpenGL::GraphicsBufferMapType::None);

		uintMem alignedBufferSize = bufferSize;
		if (buffers.Count() == 1)
		{
			CL_CHECK_RET(bufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (particles == nullptr ? 0 : CL_MEM_COPY_HOST_PTR), bufferSize, particles, &ret));
		}
		else
		{
			uint baseAddressAlign = 0;
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(uint), &baseAddressAlign, NULL));
			//Round up to a multiple of subBufferAlign
			alignedBufferSize = (bufferSize + baseAddressAlign - 1) / baseAddressAlign * baseAddressAlign;

			CL_CHECK_RET(bufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE, (buffers.Count() - 1) * alignedBufferSize + bufferSize, nullptr, &ret));

			//TODO lock here and make the write async by using events
			if (particles != nullptr)
				CL_CALL(clEnqueueWriteBuffer(clCommandQueue, bufferCL, CL_TRUE, 0, bufferSize, particles, 0, nullptr, nullptr));
		}

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].CreateBuffer(bufferCL, alignedBufferSize * i, bufferSize, false);
	}

	RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::ParticlesBuffer(cl_command_queue clCommandQueue)
		: lock(clCommandQueue), copyForRenderingFinishedEvent(NULL), buffer(NULL), copyRequiredForRendering(false)
	{
	}
	RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::~ParticlesBuffer()
	{
		if (buffer != NULL)
			CL_CALL(clReleaseMemObject(buffer));

		if (copyForRenderingFinishedEvent != NULL)
			CL_CALL(clReleaseEvent(copyForRenderingFinishedEvent));
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::CreateBuffer(cl_mem parentBuffer, uintMem offset, uintMem size, bool copyRequiredForRendering)
	{
		if (buffer != NULL)
		{
			lock.WaitForReadToFinish();
			lock.WaitForWriteToFinish();

			CL_CALL(clReleaseMemObject(buffer));
		}

		if (parentBuffer != NULL)
		{
			cl_buffer_region region{
					.origin = offset,
					.size = size
			};
			CL_CHECK_RET(buffer = clCreateSubBuffer(parentBuffer, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret));

			this->copyRequiredForRendering = copyRequiredForRendering;
		}
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
	ResourceLockGuard RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::LockForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize)
	{
		PrepareForRendering(bufferGL, bufferSize);

		if (copyForRenderingFinishedEvent != NULL)
		{
			CL_CALL(clWaitForEvents(1, &copyForRenderingFinishedEvent), ResourceLockGuard());
			CL_CALL(clReleaseEvent(copyForRenderingFinishedEvent), ResourceLockGuard());
			copyForRenderingFinishedEvent = NULL;
		}

		//No locks are made because the rendering buffer is sperate from the simulation buffers. The lock is locked while copying data in PrepareForRendering
		return ResourceLockGuard(nullptr, (void*)0, this);
	}
	void RenderableGPUParticleBufferManagerWithoutCLGLInterop::ParticlesBuffer::PrepareForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& bufferGL, uintMem bufferSize)
	{
		if (!copyRequiredForRendering || bufferSize == 0)
			return;

		cl_event lockAcquiredEvent = NULL;
		void* map = bufferGL.MapBufferRange(0, bufferSize, Graphics::OpenGL::GraphicsBufferMapOptions::InvalidateBuffer);

		lock.LockRead(&lockAcquiredEvent);
		CL_CALL(clEnqueueReadBuffer(lock.GetCommandQueue(), buffer, CL_FALSE, 0, bufferSize, map, lockAcquiredEvent == NULL ? 0 : 1, lockAcquiredEvent == NULL ? nullptr : &lockAcquiredEvent, &copyForRenderingFinishedEvent));
		lock.UnlockRead({ copyForRenderingFinishedEvent });

		bufferGL.UnmapBuffer();
		copyRequiredForRendering = false;
	}
}