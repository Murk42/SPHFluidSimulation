#pragma once
#include <mutex>
#include "BlazeEngineCore/BlazeEngineCoreDefines.h"

namespace SPH
{
	class CPULock
	{
	public:
		CPULock();
		~CPULock();

		void LockRead();
		void LockWrite();
		template<typename F>
		void LockWrite(const F& condition);

		void UnlockRead();
		void UnlockWrite();

		void NotifyAll();
	private:		
		std::condition_variable cv;
		std::mutex mutex;
		Blaze::int32 counter;
	};
	template<typename F>
	inline void CPULock::LockWrite(const F& condition)
	{
		std::unique_lock lk{ mutex };

		cv.wait(lk, [&]() { return counter == 0 && condition(); });

		--counter;		
	}
}