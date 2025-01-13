#include "pch.h"
#include "SPH/ParticleBufferManager/RenderableCPUParticleBufferManager.h"

#include "GL/glew.h"

namespace SPH
{
	/*
	void WaitFence(Graphics::OpenGLWrapper::Fence& fence)
	{
		auto fenceState = fence.BlockClient(2);

		switch (fenceState)
		{
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::FenceNotManager:
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
		fence = Graphics::OpenGLWrapper::Fence();
	}
	RenderableCPUParticleBufferManager::RenderableCPUParticleBufferManager()
		: staticParticleBuffer(0), dynamicParticleCount(0), staticParticleCount(0), staticParticlesMap(nullptr)
	{
		buffers = Array<Buffer>(3, *this);
		currentBuffer = buffers.Count() - 1;

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.ManagerVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.ManagerVertexAttributeDivisor(0, 1);
	}
	void RenderableCPUParticleBufferManager::Clear()
	{
		for (auto& buffer : buffers)
			buffer.Clear();

		currentBuffer = 0;
		dynamicParticleCount = 0;
		staticParticleCount = 0;


		staticParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(StaticParticle), 0);

		if (staticParticlesMap != nullptr)
		{
			staticParticleBuffer.UnmapBuffer();
			staticParticlesMap = nullptr;
		}
		staticParticleBuffer.Release();
	}
	void RenderableCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableCPUParticleBufferManager::ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		buffers[currentBuffer].ManagerDynamicParticles(dynamicParticles.Ptr());
		for (uintMem i = 0; i < buffers.Count(); ++i)
			if (i != currentBuffer)
			buffers[i].ManagerDynamicParticles(nullptr);
	}
	void RenderableCPUParticleBufferManager::ManagerStaticParticles(ArrayView<StaticParticle> staticParticles)
	{

		if (staticParticlesMap != nullptr)
			staticParticleBuffer.UnmapBuffer();

		this->staticParticleCount = staticParticles.Count();

		if (staticParticles.Empty())
		{
			staticParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(StaticParticle), 0);

			staticParticleBuffer.Release();
			return;
		}

		staticParticleBuffer = decltype(staticParticleBuffer)();
		staticParticleBuffer.Allocate(staticParticles.Ptr(),staticParticles.Count() * sizeof(StaticParticle), Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);

		staticParticleVertexArray.ManagerVertexAttributeBuffer(0, &staticParticleBuffer, sizeof(StaticParticle), 0);

		staticParticlesMap = (StaticParticle*)staticParticleBuffer.MapBufferRange(0, staticParticles.Count() * sizeof(StaticParticle), Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);
		memcpy(staticParticlesMap, staticParticles.Ptr(), staticParticles.Count() * sizeof(StaticParticle));
		staticParticleBuffer.FlushBufferRange(0, staticParticles.Count() * sizeof(StaticParticle));
	}

	CPUParticleReadBufferHandle& RenderableCPUParticleBufferManager::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	CPUParticleWriteBufferHandle& RenderableCPUParticleBufferManager::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}
	CPUParticleWriteBufferHandle& RenderableCPUParticleBufferManager::GetReadWriteBufferHandle()
	{
		return buffers[currentBuffer];
	}
	const StaticParticle* RenderableCPUParticleBufferManager::GetStaticParticles()
	{
		return staticParticlesMap;
	}

	ParticleRenderBufferHandle& RenderableCPUParticleBufferManager::GetRenderBufferHandle()
	{
		return buffers[currentBuffer];
	}
	Graphics::OpenGLWrapper::VertexArray& RenderableCPUParticleBufferManager::GetStaticParticleVertexArray()
	{
		return staticParticleVertexArray;
	}

	uintMem RenderableCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	uintMem RenderableCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticleCount;
	}

	RenderableCPUParticleBufferManager::Buffer::Buffer(const RenderableCPUParticleBufferManager& bufferManager) :
		bufferManager(bufferManager), dynamicParticlesMap(nullptr), dynamicParticleBuffer(0)
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
	RenderableCPUParticleBufferManager::Buffer::~Buffer()
	{
		Clear();
	}
	void RenderableCPUParticleBufferManager::Buffer::Clear()
	{
		{
			std::unique_lock lk{ stateMutex };

			writeSync.WaitInactive();
			readSync.WaitInactive();
			WaitFence(renderingFinishedFence);
		}

		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(0, nullptr, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(1, nullptr, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(2, nullptr, sizeof(DynamicParticle), 0);

		if (dynamicParticlesMap != nullptr)
		{
			dynamicParticleBuffer.UnmapBuffer();
			dynamicParticlesMap = nullptr;
		}
		dynamicParticleBuffer.Release();

	}
	void RenderableCPUParticleBufferManager::Buffer::ManagerDynamicParticles(const DynamicParticle* particles)
	{
		if (bufferManager.dynamicParticleCount == 0)
		{
			Clear();
			return;
		}

		if (dynamicParticlesMap != nullptr)
			dynamicParticleBuffer.UnmapBuffer();

		dynamicParticleBuffer = decltype(dynamicParticleBuffer)();

		dynamicParticleBuffer.Allocate(
			particles, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);
		dynamicParticlesMap = (DynamicParticle*)dynamicParticleBuffer.MapBufferRange(
			0, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush
		);

		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(0, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(1, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.ManagerVertexAttributeBuffer(2, &dynamicParticleBuffer, sizeof(DynamicParticle), 0);
	}
	CPUSync& RenderableCPUParticleBufferManager::Buffer::GetReadSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		readSync.MarkStart();
		return readSync;
	}
	CPUSync& RenderableCPUParticleBufferManager::Buffer::GetWriteSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		WaitRender();
		writeSync.MarkStart();
		return writeSync;
	}
	void RenderableCPUParticleBufferManager::Buffer::StartRender()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		dynamicParticleBuffer.FlushBufferRange(0, sizeof(DynamicParticle) * bufferManager.dynamicParticleCount);
	}
	void RenderableCPUParticleBufferManager::Buffer::FinishRender()
	{
		renderingFinishedFence.ManagerFence();
		dynamicParticleBuffer.Invalidate();
	}
	void RenderableCPUParticleBufferManager::Buffer::WaitRender()
	{
		WaitFence(renderingFinishedFence);
	}
	*/
	RenderableCPUParticleBufferManager::RenderableCPUParticleBufferManager() 
		: dynamicParticlesCount(0), staticParticlesCount(0), staticParticlesMap(0), currentBuffer(0)
	{
		staticParticlesVA.EnableVertexAttribute(0);
		staticParticlesVA.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticlesVA.SetVertexAttributeDivisor(0, 1);

		buffers = Array<Buffer>(3);

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
	}
	RenderableCPUParticleBufferManager::~RenderableCPUParticleBufferManager()
	{
		Clear();
	}
	void RenderableCPUParticleBufferManager::Clear()
	{
		CleanDynamicParticlesBuffers();		

		CleanStaticParticlesBuffer();
	}
	void RenderableCPUParticleBufferManager::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	void RenderableCPUParticleBufferManager::AllocateDynamicParticles(uintMem count)
	{
		CleanDynamicParticlesBuffers();

		if (count == 0)		
			return;		
		
		dynamicParticlesCount = count;
		dynamicParticlesBuffer = decltype(dynamicParticlesBuffer)();
		dynamicParticlesBuffer.Allocate(nullptr, sizeof(StaticParticle) * count * buffers.Count(),
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);

		DynamicParticle* dynamicParticlesMap = (DynamicParticle*)dynamicParticlesBuffer.MapBufferRange(0, sizeof(DynamicParticle) * count * buffers.Count(), Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			buffers[i].dynamicParticlesMap = dynamicParticlesMap + sizeof(DynamicParticle) * count * i;

			buffers[i].dynamicParticlesVA.SetVertexAttributeBuffer(0, &dynamicParticlesBuffer, sizeof(DynamicParticle), sizeof(DynamicParticle) * count * i);
			buffers[i].dynamicParticlesVA.SetVertexAttributeBuffer(1, &dynamicParticlesBuffer, sizeof(DynamicParticle), sizeof(DynamicParticle) * count * i);
			buffers[i].dynamicParticlesVA.SetVertexAttributeBuffer(2, &dynamicParticlesBuffer, sizeof(DynamicParticle), sizeof(DynamicParticle) * count * i);
		}

	}
	void RenderableCPUParticleBufferManager::AllocateStaticParticles(uintMem count)
	{
		CleanStaticParticlesBuffer();		

		if (count == 0)
			return;
		
		staticParticlesCount = count;
		staticParticlesBuffer = decltype(staticParticlesBuffer)();
		staticParticlesBuffer.Allocate(nullptr, sizeof(StaticParticle) * count,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Read | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write,
			Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent
		);
		staticParticlesMap = (StaticParticle*)staticParticlesBuffer.MapBufferRange(0, sizeof(StaticParticle) * count, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		staticParticlesVA.SetVertexAttributeBuffer(0, &staticParticlesBuffer, sizeof(StaticParticle), 0);
	}
	uintMem RenderableCPUParticleBufferManager::GetDynamicParticleCount()
	{
		return dynamicParticlesCount;
	}
	uintMem RenderableCPUParticleBufferManager::GetStaticParticleCount()
	{
		return staticParticlesCount;
	}
	CPUParticleBufferLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesActiveRead(const TimeInterval& timeInterval)
	{
		auto& buffer = buffers[currentBuffer];

		uint64 lockTicket = buffer.dynamicParticlesLock.TryLockRead(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffer.dynamicParticlesMap);
	}
	CPUParticleBufferLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesAvailableRead(const TimeInterval& timeInterval, uintMem* index)
	{
		for (uintMem i = 0; i < buffers.Count(); ++i)
		{
			auto& buffer = buffers[(currentBuffer + buffers.Count() - i) % buffers.Count()];

			uint64 lockTicket = buffer.dynamicParticlesLock.TryLockRead(i == buffers.Count() - 1 ? timeInterval : TimeInterval::Zero());

			if (lockTicket != 0)
			{
				if (index != nullptr)
					*index = i;

				return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffers[currentBuffer].dynamicParticlesMap);
			}
		}

		return CPUParticleBufferLockGuard();
	}
	CPUParticleBufferLockGuard RenderableCPUParticleBufferManager::LockDynamicParticlesReadWrite(const TimeInterval& timeInterval)
	{
		auto& buffer = buffers[currentBuffer];

		uint64 lockTicket = buffer.dynamicParticlesLock.TryLockReadWrite(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		{
			std::lock_guard lk{ buffer.dynamicParticlesFencesMutex };

			for (auto& fence : buffer.dynamicParticleFences)
				fence.BlockClient(timeInterval.ToSeconds());

			buffer.dynamicParticleFences.Clear();
		}

		return CPUParticleBufferLockGuard([this, &buffer = buffer, lockTicket = lockTicket]() { buffer.dynamicParticlesLock.Unlock(lockTicket); }, buffer.dynamicParticlesMap);
	}
	CPUParticleBufferLockGuard RenderableCPUParticleBufferManager::LockStaticParticlesRead(const TimeInterval& timeInterval)
	{
		uint64 lockTicket = staticParticlesLock.TryLockRead(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();		

		return CPUParticleBufferLockGuard([this, lockTicket = lockTicket]() { staticParticlesLock.Unlock(lockTicket); }, staticParticlesMap);
	}
	CPUParticleBufferLockGuard RenderableCPUParticleBufferManager::LockStaticParticlesReadWrite(const TimeInterval& timeInterval)
	{
		auto lockTicket = staticParticlesLock.TryLockReadWrite(timeInterval);

		if (lockTicket == 0)
			return CPUParticleBufferLockGuard();

		{
			std::lock_guard lk{ staticParticlesFencesMutex };

			for (auto& fence : staticParticleFences)
				fence.BlockClient(timeInterval.ToSeconds());

			staticParticleFences.Clear();
		}

		return CPUParticleBufferLockGuard([this, lockTicket = lockTicket]() { staticParticlesLock.Unlock(lockTicket); }, staticParticlesMap);		
	}

	Graphics::OpenGLWrapper::VertexArray& RenderableCPUParticleBufferManager::GetDynamicParticlesVertexArray(uintMem index)
	{
		return buffers[index].dynamicParticlesVA;
	}

	Graphics::OpenGLWrapper::VertexArray& RenderableCPUParticleBufferManager::GetStaticParticlesVertexArray()
	{
		return staticParticlesVA;
	}
	
	void RenderableCPUParticleBufferManager::CleanDynamicParticlesBuffers()
	{
		if (dynamicParticlesCount == 0)
			return;

		for (auto& buffer : buffers)
		{
			buffer.dynamicParticlesVA.SetVertexAttributeBuffer(0, nullptr, 0, 0);
			buffer.dynamicParticlesVA.SetVertexAttributeBuffer(1, nullptr, 0, 0);
			buffer.dynamicParticlesVA.SetVertexAttributeBuffer(2, nullptr, 0, 0);

			buffer.dynamicParticlesMap = nullptr;

			buffer.dynamicParticleFences.Clear();
		}

		if (dynamicParticlesCount != 0)
			dynamicParticlesBuffer.UnmapBuffer();
		dynamicParticlesBuffer.Release();
		dynamicParticlesCount = 0;
	}
	void RenderableCPUParticleBufferManager::CleanStaticParticlesBuffer()
	{
		if (staticParticlesCount == 0)
			return;

		staticParticlesVA.SetVertexAttributeBuffer(0, nullptr, 0, 0);
		staticParticlesMap = nullptr;
		staticParticleFences.Clear();

		if (staticParticlesCount != 0)
			staticParticlesBuffer.UnmapBuffer();

		staticParticlesBuffer.Release();
		staticParticlesCount = 0;
	}
}