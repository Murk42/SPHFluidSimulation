#include "pch.h"
#include "SPH/ParticleBufferManager/CPUParticleBufferManager.h"

namespace SPH
{
	CPUParticleBufferLockGuard::CPUParticleBufferLockGuard()
		: buffer(nullptr)
	{
	}
	CPUParticleBufferLockGuard::CPUParticleBufferLockGuard(UnlockFunction&& unlockFunction, void* buffer)
		: unlockFunction(std::move(unlockFunction)), buffer(buffer)
	{
	}
	CPUParticleBufferLockGuard::CPUParticleBufferLockGuard(CPUParticleBufferLockGuard&& other) noexcept
		: unlockFunction(std::move(unlockFunction)), buffer(other.buffer)
	{
		other.buffer = nullptr;
	}
	CPUParticleBufferLockGuard::~CPUParticleBufferLockGuard()
	{	
		if (unlockFunction && buffer != nullptr)			
			Debug::Logger::LogFatal("SPH Library", "A particle buffer lock guard has not been unlocked");			
	}
	void CPUParticleBufferLockGuard::Unlock()
	{
		if (unlockFunction)
		{
			unlockFunction();
			unlockFunction = UnlockFunction();
		}
		else if (buffer == nullptr)
			Debug::Logger::LogFatal("SPH Library", "Unlocking a particle buffer lock guard that isn't locked");
		else
			Debug::Logger::LogFatal("SPH Library", "Unlocking a particle buffer lock guard that has already been unlocked");
	}
	void* CPUParticleBufferLockGuard::GetBuffer()
	{
		return buffer;
	}
	const CPUParticleBufferLockGuard& CPUParticleBufferLockGuard::operator=(CPUParticleBufferLockGuard&& other) noexcept
	{
		unlockFunction = std::move(unlockFunction);
		buffer = other.buffer;

		other.buffer = nullptr;

		return *this;
	}
}