#include "pch.h"
#include "SPH/ParticleBufferManager/CPUResourceLock.h"

namespace SPH
{
	CPULock::CPULock()
		: counter(0)
	{		
	}
	CPULock::~CPULock()
	{
		std::unique_lock lk{ mutex };

		if (counter != 0)
			Blaze::Debug::Logger::LogFatal("SPH Library", "A locked lock is being destroyed.");
	}
	void CPULock::LockRead()
	{
		std::unique_lock lk{ mutex };		

		cv.wait(lk, [&]() { return counter >= 0; });
		
		++counter;
	}
	void CPULock::LockWrite()
	{
		std::unique_lock lk{ mutex };		

		cv.wait(lk, [&]() { return counter == 0; });
		
		--counter;
	}
	void CPULock::UnlockRead()
	{
		std::unique_lock lk{ mutex };

		if (counter == 0)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock that is not locked");
			return;
		}
		else if (counter < 0)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock for reading that is locked for writing");
			return;
		}

		--counter;

		if (counter == 0)
			cv.notify_all();
	}
	void CPULock::UnlockWrite()
	{
		std::unique_lock lk{ mutex };

		if (counter == 0)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock that is not locked");
			return;
		}
		else if (counter > 0)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock for writing that is locked for reading");
			return;
		}

		++counter;
		cv.notify_all();
	}
	void CPULock::NotifyAll()
	{
		cv.notify_all();
	}
}
