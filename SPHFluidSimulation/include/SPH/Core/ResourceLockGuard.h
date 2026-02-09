#pragma once

namespace SPH
{
	class ResourceLockGuard final
	{
	public:
		using UnlockFunction = void(*)(ArrayView<void*> waitEvents, void* userData);
		ResourceLockGuard();
		ResourceLockGuard(UnlockFunction unlockFunction, void* resource, void* userData);
		ResourceLockGuard(const ResourceLockGuard&) = delete;
		ResourceLockGuard(ResourceLockGuard&& other) noexcept;
		~ResourceLockGuard();

		void Unlock(ArrayView<void*> waitEvents);
		void* GetResource();

		const ResourceLockGuard& operator=(const ResourceLockGuard& other) = delete;
		const ResourceLockGuard& operator=(ResourceLockGuard&& other) noexcept;
	private:
		UnlockFunction unlockFunction;
		void* resource;
		void* userData;
	};
}