#include "pch.h"
#include "SPH/ParticleBufferManager/GPUParticleBufferManager.h"

namespace SPH
{
	GPUParticleBufferLockGuard::GPUParticleBufferLockGuard()
		: buffer(nullptr)
	{
	}
	GPUParticleBufferLockGuard::GPUParticleBufferLockGuard(UnlockFunction&& unlockFunction, cl_mem buffer)
		: unlockFunction(std::move(unlockFunction)), buffer(buffer)
	{
	}
	GPUParticleBufferLockGuard::GPUParticleBufferLockGuard(GPUParticleBufferLockGuard&& other) noexcept
		: unlockFunction(std::move(unlockFunction)), buffer(other.buffer)
	{
		other.buffer = nullptr;
	}
	GPUParticleBufferLockGuard::~GPUParticleBufferLockGuard()
	{		
		if (unlockFunction && buffer != nullptr)
			Debug::Logger::LogFatal("SPH Library", "A particle buffer lock guard has not been unlocked");
	}
	void GPUParticleBufferLockGuard::Unlock(ArrayView<cl_event> waitEvents)
	{
		if (unlockFunction)
		{
			unlockFunction(waitEvents);
			unlockFunction = UnlockFunction();
		}
		else if (buffer == nullptr)
			Debug::Logger::LogFatal("SPH Library", "Unlocking a particle buffer lock guard that isn't locked");
		else
			Debug::Logger::LogFatal("SPH Library", "Unlocking a particle buffer lock guard that has already been unlocked");
	}
	cl_mem GPUParticleBufferLockGuard::GetBuffer()
	{
		return buffer;
	}
	const GPUParticleBufferLockGuard& GPUParticleBufferLockGuard::operator=(GPUParticleBufferLockGuard&& other) noexcept
	{
		unlockFunction = std::move(unlockFunction);
		buffer = other.buffer;

		other.buffer = nullptr;

		return *this;
	}
}