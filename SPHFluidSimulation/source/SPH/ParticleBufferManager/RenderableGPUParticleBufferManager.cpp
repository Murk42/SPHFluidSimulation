#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableGPUParticleBufferManager.h"
#include "GL/glew.h"
#include <CL/cl.h>
#include "OpenCLDebug.h"

namespace SPH
{
	/*
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence);

	RenderableGPUParticleBufferManager::RenderableGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue) :
		clContext(clContext), clCommandQueue(clCommandQueue), currentBuffer(0), staticParticleBufferCL(nullptr), dynamicParticleCount(0), staticParticleCount(0), staticParticleBufferGL(0)
	{
		CLGLInterop = CheckForExtensions(clDevice, { "cl_khr_gl_sharing" });

		buffers = Array<Buffer>(3, *this);
		currentBuffer = buffers.Count() - 1;

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.ManagerVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.ManagerVertexAttributeDivisor(0, 1);
	}
	void RenderableGPUParticleBufferManager::Clear()
	{
		for (auto& buffer : buffers)
			buffer.Clear();

		currentBuffer = 0;
		dynamicParticleCount = 0;
		staticParticleCount = 0;

		staticParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(StaticParticle), 0);

		if (staticParticleBufferCL!= nullptr)
			clReleaseMemObject(staticParticleBufferCL);
		staticParticleBufferCL = nullptr;
		staticParticleBufferGL.Release();

	}
	void RenderableGPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableGPUParticleBufferManager::ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers[currentBuffer].ManagerDynamicParticles(dynamicParticles.Ptr());
		for (uintMem i = 0; i < buffers.Count(); ++i)
			if (i != currentBuffer)
				buffers[i].ManagerDynamicParticles(nullptr);
	}
	void RenderableGPUParticleBufferManager::ManagerStaticParticles(ArrayView<StaticParticle> staticParticles)
	{
		cl_int ret = 0;

		staticParticleCount = staticParticles.Count();

		if (staticParticleBufferCL != nullptr)
			ret = clReleaseMemObject(staticParticleBufferCL);

		CL_CHECK();

		if (staticParticles.Empty())
		{
			staticParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(StaticParticle), 0);

			staticParticleBufferCL = nullptr;
			staticParticleBufferGL.Release();
			return;
		}

		staticParticleBufferGL = decltype(staticParticleBufferGL)();
		staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(DynamicParticle) * staticParticles.Count());

		if (CLGLInterop)
			staticParticleBufferCL = clCreateFromGLBuffer(clContext, CL_MEM_READ_WRITE, staticParticleBufferGL.GetHandle(), &ret);
		else
			staticParticleBufferCL = clCreateBuffer(clContext, CL_MEM_READ_WRITE | (staticParticles.Ptr() != nullptr ? CL_MEM_COPY_HOST_PTR : 0), sizeof(DynamicParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);

		staticParticleVertexArray.ManagerVertexAttributeBuffer(0, &staticParticleBufferGL, sizeof(StaticParticle), 0);

		CL_CHECK();
	}

	GPUParticleReadBufferHandle& RenderableGPUParticleBufferManager::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	GPUParticleWriteBufferHandle& RenderableGPUParticleBufferManager::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}
	GPUParticleWriteBufferHandle& RenderableGPUParticleBufferManager::GetReadWriteBufferHandle()
	{
		return buffers[currentBuffer];
	}
	const cl_mem& RenderableGPUParticleBufferManager::GetStaticParticleBuffer()
	{
		return staticParticleBufferCL;
	}

	ParticleRenderBufferHandle& RenderableGPUParticleBufferManager::GetRenderBufferHandle()
	{
		return buffers[currentBuffer];
	}
	Graphics::OpenGLWrapper::VertexArray& RenderableGPUParticleBufferManager::GetStaticParticleVertexArray()
	{
		return staticParticleVertexArray;
	}

	uintMem RenderableGPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	uintMem RenderableGPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticleCount;
	}

	RenderableGPUParticleBufferManager::Buffer::Buffer(const RenderableGPUParticleBufferManager& bufferManager) :
		bufferManager(bufferManager), dynamicParticleBufferMap(nullptr), dynamicParticleBufferGL(0),
		readFinishedEvent(nullptr), writeFinishedEvent(nullptr), copyFinishedEvent(nullptr),

		dynamicParticleBufferCL(nullptr)
	{
		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.ManagerVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.ManagerVertexAttributeDivisor(0, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.ManagerVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.ManagerVertexAttributeDivisor(1, 1);
		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.ManagerVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.ManagerVertexAttributeDivisor(2, 1);
	}
	RenderableGPUParticleBufferManager::Buffer::~Buffer()
	{
		Clear();
	}
	void RenderableGPUParticleBufferManager::Buffer::Clear()
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

		WaitFence(renderingFence);

		WaitAndFreeEvent(writeFinishedEvent);
		WaitAndFreeEvent(copyFinishedEvent);
		WaitAndFreeEvent(readFinishedEvent);

		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(1, nullptr, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(2, nullptr, sizeof(DynamicParticle), 0);

		if (dynamicParticleBufferCL != nullptr)
		{
			clReleaseMemObject(dynamicParticleBufferCL);
			dynamicParticleBufferCL = nullptr;
		}
		dynamicParticleBufferGL.Release();
	}
	void RenderableGPUParticleBufferManager::Buffer::ManagerDynamicParticles(const DynamicParticle* particles)
	{
		if (bufferManager.dynamicParticleCount == 0)
		{
			Clear();
			return;
		}

		if (dynamicParticleBufferCL != nullptr)
			clReleaseMemObject(dynamicParticleBufferCL);

		cl_int ret = 0;

		dynamicParticleBufferGL = decltype(dynamicParticleBufferGL)();
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(0, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(1, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(2, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);


		if (bufferManager.CLGLInterop)
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, particles, GL_DYNAMIC_STORAGE_BIT);
			dynamicParticleBufferCL = clCreateFromGLBuffer(bufferManager.clContext, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
			CL_CHECK();
		}
		else
		{
			glNamedBufferStorage(dynamicParticleBufferGL.GetHandle(), sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, particles, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
			dynamicParticleBufferMap = glMapNamedBufferRange(dynamicParticleBufferGL.GetHandle(), 0, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

			dynamicParticleBufferCL = clCreateBuffer(bufferManager.clContext, CL_MEM_READ_WRITE | (particles != nullptr ? CL_MEM_COPY_HOST_PTR : 0), sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, (void*)particles, &ret);
			CL_CHECK();
		}
	}
	void RenderableGPUParticleBufferManager::Buffer::StartRead(cl_event* finishedEvent)
	{
		cl_int ret = 0;

		if (bufferManager.CLGLInterop)
		{
			CL_CALL(clEnqueueAcquireGLObjects(bufferManager.clCommandQueue, 1, &dynamicParticleBufferCL, writeFinishedEvent != nullptr ? 1 : 0, writeFinishedEvent != nullptr ? &writeFinishedEvent : nullptr, finishedEvent));
		}
		else if (writeFinishedEvent != nullptr)
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.clCommandQueue, 1, &writeFinishedEvent, finishedEvent));
	}
	void RenderableGPUParticleBufferManager::Buffer::FinishRead(ArrayView<cl_event> waitEvents)
	{
		cl_int ret = 0;

		if (readFinishedEvent)
			CL_CALL(clReleaseEvent(readFinishedEvent));

		if (bufferManager.CLGLInterop)
		{
			CL_CALL(clEnqueueReleaseGLObjects(bufferManager.clCommandQueue, 1, &dynamicParticleBufferCL, waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent));
		}
		else if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.clCommandQueue, waitEvents.Count(), waitEvents.Ptr(), &readFinishedEvent));
	}
	void RenderableGPUParticleBufferManager::Buffer::StartWrite(cl_event* finishedEvent)
	{
		cl_int ret = 0;

		WaitFence(renderingFence);

		uintMem count = 0;
		cl_event waitEvents[3]{ };
		if (copyFinishedEvent) waitEvents[count++] = copyFinishedEvent;
		if (writeFinishedEvent) waitEvents[count++] = writeFinishedEvent;
		if (readFinishedEvent) waitEvents[count++] = readFinishedEvent;

		if (bufferManager.CLGLInterop)
		{
			CL_CALL(clEnqueueAcquireGLObjects(bufferManager.clCommandQueue, 1, &dynamicParticleBufferCL, count, count == 0 ? nullptr : waitEvents, finishedEvent));
		}
		else if (count != 0)
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.clCommandQueue, count, count == 0 ? nullptr : waitEvents, finishedEvent));
	}
	void RenderableGPUParticleBufferManager::Buffer::FinishWrite(ArrayView<cl_event> waitEvents, bool prepareForRendering)
	{
		cl_int ret = 0;

		if (writeFinishedEvent)
			CL_CALL(clReleaseEvent(writeFinishedEvent));

		if (bufferManager.CLGLInterop)
		{
			CL_CALL(clEnqueueReleaseGLObjects(bufferManager.clCommandQueue, 1, &dynamicParticleBufferCL, waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent));
		}
		else if (prepareForRendering)
		{
			//TODO fix this to signal copy event
			CL_CALL(clEnqueueReadBuffer(bufferManager.clCommandQueue, dynamicParticleBufferCL, CL_FALSE, 0, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount, dynamicParticleBufferMap, waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent))
		}
		else if (!waitEvents.Empty())
			CL_CALL(clEnqueueMarkerWithWaitList(bufferManager.clCommandQueue, waitEvents.Count(), waitEvents.Ptr(), &writeFinishedEvent));
	}
	void RenderableGPUParticleBufferManager::Buffer::StartRender()
	{
		cl_int ret = 0;

		if (bufferManager.CLGLInterop)
		{
			if (writeFinishedEvent != nullptr)
			{
				CL_CALL(clWaitForEvents(1, &writeFinishedEvent));
				CL_CALL(clReleaseEvent(writeFinishedEvent));
				writeFinishedEvent = nullptr;
			}
		}
		else
		{
			if (copyFinishedEvent != nullptr)
			{
				CL_CALL(clWaitForEvents(1, &copyFinishedEvent));
				CL_CALL(clReleaseEvent(copyFinishedEvent));
				copyFinishedEvent = nullptr;
			}

			glFlushMappedNamedBufferRange(dynamicParticleBufferGL.GetHandle(), 0, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount);
			renderingFence.ManagerFence();
		}
	}
	void RenderableGPUParticleBufferManager::Buffer::FinishRender()
	{
		if (bufferManager.CLGLInterop)
			renderingFence.ManagerFence();

		dynamicParticleBufferGL.Invalidate();
	}
	void RenderableGPUParticleBufferManager::Buffer::WaitRender()
	{
		WaitFence(renderingFence);
	}
	*/
	RenderableGPUParticleBufferManager::RenderableGPUParticleBufferManager(cl_context clContext, cl_device_id clDevice, cl_command_queue commandQueue)
		: clContext(clContext), staticParticlesLock(commandQueue), currentBuffer(0), dynamicParticlesBufferCL(NULL), staticParticlesBufferCL(NULL), dynamicParticlesCount(0), staticParticlesCount(0)
	{
		buffers = Array<Buffer>(3, [commandQueue = commandQueue](Buffer* buffer, uintMem index) {
			std::construct_at(buffer, commandQueue);
			});

		for (auto& buffer : buffers)
		{
			buffer.dynamicParticlesVA.EnableVertexAttribute(0);
			buffer.dynamicParticlesVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
			buffer.dynamicParticlesVA.SetVertexAttributeDivisor(0, 1);
			buffer.dynamicParticlesVA.EnableVertexAttribute(1);
			buffer.dynamicParticlesVA.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
			buffer.dynamicParticlesVA.SetVertexAttributeDivisor(1, 1);
			buffer.dynamicParticlesVA.EnableVertexAttribute(2);
			buffer.dynamicParticlesVA.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
			buffer.dynamicParticlesVA.SetVertexAttributeDivisor(2, 1);
		}

		staticParticlesVA.EnableVertexAttribute(0);
		staticParticlesVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticlesVA.SetVertexAttributeDivisor(0, 1);

		CLGLInteropSupported = CheckForExtensions(clDevice, { "cl_khr_gl_sharing" });
	}
	RenderableGPUParticleBufferManager::~RenderableGPUParticleBufferManager()
	{
		Clear();
	}
	void RenderableGPUParticleBufferManager::Clear()
	{
		CleanDynamicParticlesBuffers();
		CleanStaticParticlesBuffer();
	}
	void RenderableGPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableGPUParticleBufferManager::AllocateDynamicParticles(uintMem count)
	{
		cl_int ret;

		CleanDynamicParticlesBuffers();

		if (count == 0)
			return;

		dynamicParticlesCount = count;
		dynamicParticlesBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(DynamicParticle) * count * buffers.Count(), nullptr, &ret);
		CL_CHECK();

		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			cl_buffer_region region{
				.origin = sizeof(DynamicParticle) * count * i,
				.size = sizeof(DynamicParticle) * count
			};
			buffers[i].dynamicParticlesView = clCreateSubBuffer(dynamicParticlesBuffer, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &ret);
			CL_CHECK();
		}
	}
	void RenderableGPUParticleBufferManager::AllocateStaticParticles(uintMem count)
	{
	}
	uintMem RenderableGPUParticleBufferManager::GetDynamicParticleCount()
	{
		return uintMem();
	}
	uintMem RenderableGPUParticleBufferManager::GetStaticParticleCount()
	{
		return uintMem();
	}
	GPUParticleBufferLockGuard RenderableGPUParticleBufferManager::LockDynamicParticlesActiveRead(cl_event* signalEvent)
	{
		return GPUParticleBufferLockGuard();
	}
	GPUParticleBufferLockGuard RenderableGPUParticleBufferManager::LockDynamicParticlesAvailableRead(cl_event* signalEvent, uintMem* index)
	{
		return GPUParticleBufferLockGuard();
	}
	GPUParticleBufferLockGuard RenderableGPUParticleBufferManager::LockDynamicParticlesReadWrite(cl_event* signalEvent)
	{
		return GPUParticleBufferLockGuard();
	}
	GPUParticleBufferLockGuard RenderableGPUParticleBufferManager::LockStaticParticlesRead(cl_event* signalEvent)
	{
		return GPUParticleBufferLockGuard();
	}
	GPUParticleBufferLockGuard RenderableGPUParticleBufferManager::LockStaticParticlesReadWrite(cl_event* signalEvent)
	{
		return GPUParticleBufferLockGuard();
	}
	Graphics::OpenGLWrapper::VertexArray& RenderableGPUParticleBufferManager::GetDynamicParticlesVertexArray(uintMem index)
	{
		// TODO: insert return statement here
	}
	Graphics::OpenGLWrapper::VertexArray& RenderableGPUParticleBufferManager::GetStaticParticlesVertexArray()
	{
		// TODO: insert return statement here
	}
	void RenderableGPUParticleBufferManager::CleanDynamicParticlesBuffers()
	{
	}
	void RenderableGPUParticleBufferManager::CleanStaticParticlesBuffer()
	{
		cl_int ret;

		if (staticParticlesCount == 0)
			return;
		CL_CALL(clReleaseMemObject(staticParticlesBufferCL));

		staticParticlesBufferCL = NULL;

		staticParticlesVA.SetVertexAttributeBuffer(0, nullptr, 0, 0);		
		staticParticleFences.Clear();		

		staticParticlesBuffer.Release();

		staticParticlesCount = 0;
	}
}