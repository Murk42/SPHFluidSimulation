#include "pch.h"
#include "SPH/ParticleBufferManager/OfflineGPUParticleBufferManager.h"
#include "OpenCLDebug.h"
#include <CL/cl.h>

namespace SPH
{	
	OfflineGPUParticleBufferManager::OfflineGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue)
		: clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), currentBuffer(0), bufferCL(NULL), bufferSize(0)
	{
	}
	OfflineGPUParticleBufferManager::~OfflineGPUParticleBufferManager()
	{
		Clear();
	}
	void OfflineGPUParticleBufferManager::Clear()
	{
		if (bufferSize == 0)
			return;

		buffers.Clear();		
		CL_CALL(clReleaseMemObject(bufferCL));
		bufferCL = NULL;
		bufferSize = 0;
	}
	void OfflineGPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineGPUParticleBufferManager::Allocate(uintMem newBufferSize, void* ptr, uintMem bufferCount)
	{
		Clear();

		if (newBufferSize == 0)
			return;

		if (bufferCount == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "bufferCount is 0");
			return;
		}

		buffers = Array<ParticlesBuffer>(bufferCount, clCommandQueue);
		bufferSize = newBufferSize;

		uintMem alignedBufferSize = bufferSize;
		if (buffers.Count() == 1)
		{
			CL_CHECK_RET(bufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (ptr == nullptr ? 0 : CL_MEM_COPY_HOST_PTR), bufferSize, ptr, &ret));
		}
		else
		{
			uint baseAddressAlign = 0;
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(uint), &baseAddressAlign, NULL));
			//Round up to a multiple of subBufferAlign
			uintMem alignedBufferSize = (bufferSize + baseAddressAlign - 1) / baseAddressAlign * baseAddressAlign;

			CL_CHECK_RET(bufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE, (buffers.Count() - 1) * alignedBufferSize + bufferSize, nullptr, &ret));

			//TODO lock here and make the write async by using events
			if (ptr != nullptr)
				CL_CALL(clEnqueueWriteBuffer(clCommandQueue, bufferCL, CL_TRUE, 0, bufferSize, ptr, 0, nullptr, nullptr));
		}

		for (uintMem i = 0; i < buffers.Count(); ++i)
			buffers[i].CreateBuffer(bufferCL, alignedBufferSize * i, bufferSize);
	}
	uintMem OfflineGPUParticleBufferManager::GetBufferCount() const
	{
		return buffers.Count();
	}
	uintMem OfflineGPUParticleBufferManager::GetBufferSize()
	{
		return bufferSize;
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::LockRead(void* signalEvent)
	{
		return buffers[currentBuffer].LockRead((cl_event*)signalEvent);
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::LockWrite(void* signalEvent)
	{
		return buffers[currentBuffer].LockWrite((cl_event*)signalEvent);
	}

	void OfflineGPUParticleBufferManager::FlushAllOperations()
	{
	}

	OfflineGPUParticleBufferManager::ParticlesBuffer::ParticlesBuffer(cl_command_queue clCommandQueue)
		: lock(clCommandQueue), buffer(NULL)
	{

	}
	void OfflineGPUParticleBufferManager::ParticlesBuffer::CreateBuffer(cl_mem parentBuffer, uintMem offset, uintMem size)
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
		}
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::ParticlesBuffer::LockRead(cl_event* signalEvent)
	{		
		lock.LockRead((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockRead(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer, &lock);
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::ParticlesBuffer::LockWrite(cl_event* signalEvent)
	{
		lock.LockWrite((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockWrite(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer, &lock);
	}
}