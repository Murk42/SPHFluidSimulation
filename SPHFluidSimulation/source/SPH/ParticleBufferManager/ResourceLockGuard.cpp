#include "pch.h"
#include "SPH/ParticleBufferManager/ResourceLockGuard.h"

namespace SPH
{
	ResourceLockGuard::ResourceLockGuard()
		: unlockFunction(nullptr), resource(nullptr), userData(nullptr)
	{
	}
	ResourceLockGuard::ResourceLockGuard(UnlockFunction unlockFunction, void* resource, void* userData)
		: unlockFunction(unlockFunction), resource(resource), userData(userData)
	{
	}
	ResourceLockGuard::ResourceLockGuard(ResourceLockGuard&& other) noexcept
		: unlockFunction(other.unlockFunction), resource(other.resource), userData(other.userData)
	{
		other.unlockFunction = nullptr;
		other.resource = nullptr;
		other.userData = nullptr;
	}
	ResourceLockGuard::~ResourceLockGuard()
	{
		if (unlockFunction != nullptr)
			Debug::Logger::LogError("SPH Library", "ResourceLockGuard destroyed without unlocking");
	}
	void ResourceLockGuard::Unlock(ArrayView<void*> waitEvents)
	{
		if (unlockFunction)
			unlockFunction(waitEvents, userData);

		unlockFunction = nullptr;
		resource = nullptr;
	}
	void* ResourceLockGuard::GetResource()
	{
		return resource;
	}
	const ResourceLockGuard& ResourceLockGuard::operator=(ResourceLockGuard&& other) noexcept
	{
		unlockFunction = other.unlockFunction;
		resource = other.resource;
		userData = other.userData;

		other.unlockFunction = nullptr;
		other.resource = nullptr;
		other.userData = nullptr;

		return *this;
	}
}