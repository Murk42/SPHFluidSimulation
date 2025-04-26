#include "pch.h"
#include "SPH/ParticleBufferManager/OfflineGPUParticleBufferManager.h"
#include "OpenCLDebug.h"
#include <CL/cl.h>

namespace SPH
{	
	OfflineGPUParticleBufferManager::OfflineGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue)
		: clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), staticParticlesLock(clCommandQueue), currentBuffer(0), dynamicParticlesBuffer(NULL), staticParticlesBuffer(NULL), dynamicParticlesCount(0), staticParticlesCount(0)
	{
		buffers = Array<DynamicParticlesSubBuffer>(3, [clCommandQueue = clCommandQueue](DynamicParticlesSubBuffer* buffer, uintMem index) {
			std::construct_at(buffer, DynamicParticlesSubBuffer{ clCommandQueue, NULL });
			});
	}
	OfflineGPUParticleBufferManager::~OfflineGPUParticleBufferManager()
	{
		Clear();
	}
	void OfflineGPUParticleBufferManager::Clear()
	{
		CleanDynamicParticlesBuffers();
		CleanStaticParticlesBuffer();		
	}
	void OfflineGPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineGPUParticleBufferManager::AllocateDynamicParticles(uintMem count, DynamicParticle* particles)
	{
		cl_int ret;

		CleanDynamicParticlesBuffers();

		if (count == 0)		
			return;

		currentBuffer = 0;

		uint subBufferAlign = 0;
		CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(uint), &subBufferAlign, NULL));
		uintMem subBufferSize = (sizeof(DynamicParticle) * count + subBufferAlign - 1) / subBufferAlign * subBufferAlign; //Round up to a multiple of subBufferAlign

		dynamicParticlesCount = count;
		dynamicParticlesBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (particles == nullptr ? 0 : CL_MEM_USE_HOST_PTR), subBufferSize * buffers.Count(), particles, &ret);
		CL_CHECK();
		CL_CALL(clEnqueueWriteBuffer(clCommandQueue, dynamicParticlesBuffer, CL_TRUE, 0, subBufferSize, particles, 0, nullptr, nullptr));

		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			cl_buffer_region region{
				.origin = subBufferSize * i,
				.size = subBufferSize
			};
			buffers[i].dynamicParticlesView = clCreateSubBuffer(dynamicParticlesBuffer, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret);
			CL_CHECK();
		}		
	}
	void OfflineGPUParticleBufferManager::AllocateStaticParticles(uintMem count, StaticParticle* particles)
	{
		cl_int ret;

		CleanStaticParticlesBuffer();

		if (count == 0)
			return;

		staticParticlesCount = count;
		staticParticlesBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (particles == nullptr ? 0 : CL_MEM_USE_HOST_PTR), sizeof(StaticParticle) * count, particles, &ret);
		CL_CHECK();
	}
	uintMem OfflineGPUParticleBufferManager::GetDynamicParticleBufferCount() const
	{
		return buffers.Count();
	}
	uintMem OfflineGPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticlesCount;
	}
	uintMem OfflineGPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticlesCount;
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::LockDynamicParticlesForRead(void* signalEvent)
	{
		auto& buffer = buffers[(currentBuffer + buffers.Count() - 1) % buffers.Count()];

		buffer.dynamicParticlesLock.LockRead((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockRead(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer.dynamicParticlesView,& buffer.dynamicParticlesLock);
	}	
	ResourceLockGuard OfflineGPUParticleBufferManager::LockDynamicParticlesForWrite(void* signalEvent)
	{
		auto& buffer = buffers[currentBuffer];

		buffer.dynamicParticlesLock.LockWrite((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockWrite(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, buffer.dynamicParticlesView, &buffer.dynamicParticlesLock);		
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::LockStaticParticlesForRead(void* signalEvent)
	{
		staticParticlesLock.LockRead((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockRead(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, staticParticlesBuffer, & staticParticlesLock);
	}
	ResourceLockGuard OfflineGPUParticleBufferManager::LockStaticParticlesForWrite(void* signalEvent)
	{
		staticParticlesLock.LockWrite((cl_event*)signalEvent);

		return ResourceLockGuard([](ArrayView<void*> waitEvents, void* userData) {
			((OpenCLLock*)userData)->UnlockWrite(ArrayView<cl_event>((cl_event*)waitEvents.Ptr(), waitEvents.Count()));
			}, staticParticlesBuffer, &staticParticlesLock);
	}
	void OfflineGPUParticleBufferManager::FlushAllOperations()
	{
		for (auto& buffer : buffers)
		{			
			buffer.dynamicParticlesLock.WaitForReadToFinish();
			buffer.dynamicParticlesLock.WaitForWriteToFinish();
		}
		staticParticlesLock.WaitForReadToFinish();
		staticParticlesLock.WaitForWriteToFinish();
	}
	void OfflineGPUParticleBufferManager::CleanDynamicParticlesBuffers()
	{		
		if (dynamicParticlesCount == 0)
			return;

		for (auto& buffer : buffers)
		{
			CL_CALL(clReleaseMemObject(buffer.dynamicParticlesView));
			buffer.dynamicParticlesView = NULL;
		}

		CL_CALL(clReleaseMemObject(dynamicParticlesBuffer));
		dynamicParticlesBuffer = NULL;
		dynamicParticlesCount = 0;
	}
	void OfflineGPUParticleBufferManager::CleanStaticParticlesBuffer()
	{		
		if (staticParticlesCount == 0)
			return;

		CL_CALL(clReleaseMemObject(staticParticlesBuffer));
		staticParticlesBuffer = NULL;
		staticParticlesCount = 0;
	}	
	/*
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence);

	OfflineGPUParticleBufferManager::OfflineGPUParticleBufferManager(cl_context clContext, cl_command_queue queue) :
		clContext(clContext), queue(queue), currentBuffer(0), dynamicParticleCount(0), staticParticleCount(0), staticParticleBuffer(nullptr)
	{
		buffers = Array<Buffer>(3, *this);
		currentBuffer = buffers.Count() - 1;
	}	
	void OfflineGPUParticleBufferManager::Clear()
	{
		for (auto& buffer : buffers)
			buffer.Clear();

		currentBuffer = 0;
		dynamicParticleCount = 0;
		staticParticleCount = 0;

		if (staticParticleBuffer != nullptr)
		{
			clReleaseMemObject(staticParticleBuffer);
			staticParticleBuffer = nullptr;
		}
	}
	void OfflineGPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void OfflineGPUParticleBufferManager::ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers[currentBuffer].ManagerDynamicParticles(dynamicParticles.Ptr());
		for (uintMem i = 0; i < buffers.Count(); ++i)
			if (currentBuffer != i)
				buffers[currentBuffer].ManagerDynamicParticles(nullptr);
	}
	void OfflineGPUParticleBufferManager::ManagerStaticParticles(ArrayView<StaticParticle> staticParticles)
	{
		if (staticParticleBuffer != nullptr)
			clReleaseMemObject(staticParticleBuffer);
		staticParticleCount = staticParticles.Count();

		if (staticParticles.Empty())
		{
			staticParticleBuffer = nullptr;			
			return;
		}

		cl_int ret = 0;
		staticParticleBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
		CL_CHECK();

	}
	GPUParticleReadBufferHandle& OfflineGPUParticleBufferManager::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	GPUParticleWriteBufferHandle& OfflineGPUParticleBufferManager::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}
	GPUParticleWriteBufferHandle& OfflineGPUParticleBufferManager::GetReadWriteBufferHandle()
	{
		return buffers[currentBuffer];
	}
	const cl_mem& OfflineGPUParticleBufferManager::GetStaticParticleBuffer()
	{
		return staticParticleBuffer;
	}

	uintMem OfflineGPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	uintMem OfflineGPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticleCount;
	}

	OfflineGPUParticleBufferManager::Buffer::Buffer(const OfflineGPUParticleBufferManager& bufferManager) :
		bufferManager(bufferManager), writeFinishedEvent(nullptr), copyFinishedEvent(nullptr), readFinishedEvent(nullptr), dynamicParticleBufferCL(nullptr)
	{
	}
	OfflineGPUParticleBufferManager::Buffer::~Buffer()
	{
		Clear();
	}
	void OfflineGPUParticleBufferManager::Buffer::Clear()
	{
		auto WaitAndFreeEvent = [](cl_event& event) {
			cl_int ret = 0;
			if (event)
			{
				CL_CALL(clWaitForEvents(1, &event));
				clReleaseEvent(event);
				event = nullptr;
			}
			};		

		WaitAndFreeEvent(writeFinishedEvent);
		WaitAndFreeEvent(copyFinishedEvent);
		WaitAndFreeEvent(readFinishedEvent);

		if (dynamicParticleBufferCL != nullptr)
		{
			clReleaseMemObject(dynamicParticleBufferCL);
			dynamicParticleBufferCL = nullptr;
		}
	}
	void OfflineGPUParticleBufferManager::Buffer::ManagerDynamicParticles(const DynamicParticle* particles)	
	{
		if (bufferManager.dynamicParticleCount == 0)
		{
			Clear();

			return;
		}

		if (dynamicParticleBufferCL != nullptr)
			clReleaseMemObject(dynamicParticleBufferCL);

		cl_int ret = 0;					
		dynamicParticleBufferCL = clCreateBuffer(bufferManager.clContext, CL_MEM_READ_WRITE | (particles != nullptr ? CL_MEM_COPY_HOST_PTR : 0), sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, (void*)particles, &ret);
		CL_CHECK();		
	}
	void OfflineGPUParticleBufferManager::Buffer::StartRead(cl_event* finishedEvent)
	{
		cl_int ret = 0;
				
		if (writeFinishedEvent != nullptr)
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.queue, 1, &writeFinishedEvent, finishedEvent));		
	}
	void OfflineGPUParticleBufferManager::Buffer::FinishRead(ArrayView<cl_event> waitEvents)
	{
		cl_int ret = 0;
		
		if (readFinishedEvent)
			CL_CALL(clReleaseEvent(readFinishedEvent));

		if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.queue, waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent));
	}
	void OfflineGPUParticleBufferManager::Buffer::StartWrite(cl_event* finishedEvent)
	{
		cl_int ret = 0;			

		uintMem count = 0;
		cl_event waitEvents[3]{ };
		if (copyFinishedEvent) waitEvents[count++] = copyFinishedEvent;
		if (writeFinishedEvent) waitEvents[count++] = writeFinishedEvent;
		if (readFinishedEvent) waitEvents[count++] = readFinishedEvent;

		if (count != 0)
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.queue, count, count == 0 ? nullptr : waitEvents, finishedEvent));
	}
	void OfflineGPUParticleBufferManager::Buffer::FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering)
	{
		cl_int ret = 0;

		if (writeFinishedEvent)
			CL_CALL(clReleaseEvent(writeFinishedEvent));
		
		if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.queue, waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent));
	}	
	*/
}