#include "pch.h"
#include "OfflineCPUParticleBufferSet.h"

#include "GL/glew.h"

namespace SPH
{
	OfflineCPUParticleBufferSet::OfflineCPUParticleBufferSet()
	{
	}
	void OfflineCPUParticleBufferSet::Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles)
	{
		dynamicParticleCount = dynamicParticles.Count();

		if (dynamicParticleBufferCount < 3)
		{
			Debug::Logger::LogWarning("Client", "Buffer count must be at least 3. It is set to 3");
			dynamicParticleBufferCount = 3;
		}

		buffers = Array<Buffer>(dynamicParticleBufferCount);

		buffers[0].Initialize(dynamicParticles.Ptr(), dynamicParticles.Count());
		for (uintMem i = 1; i < buffers.Count(); ++i)
			buffers[i].Initialize(nullptr, dynamicParticles.Count());

		currentBuffer = dynamicParticleBufferCount - 1;
	}
	void OfflineCPUParticleBufferSet::Clear()
	{
		buffers.Clear();
		currentBuffer = 0;
		dynamicParticleCount = 0;
	}
	void OfflineCPUParticleBufferSet::Advance()
	{
		currentBuffer = (currentBuffer + 1) % buffers.Count();
	}
	CPUParticleReadBufferHandle& OfflineCPUParticleBufferSet::GetReadBufferHandle()
	{
		return buffers[currentBuffer];
	}
	CPUParticleWriteBufferHandle& OfflineCPUParticleBufferSet::GetWriteBufferHandle()
	{
		return buffers[(currentBuffer + 1) % buffers.Count()];
	}	
	uintMem OfflineCPUParticleBufferSet::GetDynamicParticleCount()
	{
		return dynamicParticleCount;
	}
	OfflineCPUParticleBufferSet::Buffer::Buffer() :
		dynamicParticleCount(0)
	{
	}
	void OfflineCPUParticleBufferSet::Buffer::Initialize(const DynamicParticle* dynamicParticlesPtr, uintMem dynamicParticleCount)
	{
		{
			std::unique_lock lk{ stateMutex };

			writeSync.WaitInactive();
			readSync.WaitInactive();			
		}

		if (dynamicParticleCount == 0)
			return;

		if (dynamicParticlesPtr != nullptr)
			dynamicParticles = Array<DynamicParticle>(dynamicParticlesPtr, dynamicParticleCount);
		else
			dynamicParticles = Array<DynamicParticle>(dynamicParticleCount);

		this->dynamicParticleCount = dynamicParticleCount;
	}
	CPUSync& OfflineCPUParticleBufferSet::Buffer::GetReadSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();
		readSync.MarkStart();
		return readSync;
	}
	CPUSync& OfflineCPUParticleBufferSet::Buffer::GetWriteSync()
	{
		std::unique_lock lk{ stateMutex };
		writeSync.WaitInactive();		
		writeSync.MarkStart();
		return writeSync;
	}	
}