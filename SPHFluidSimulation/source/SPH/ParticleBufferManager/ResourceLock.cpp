#include "pch.h"
#include "SPH/ParticleBufferSet/ResourceLock.h"

namespace SPH
{
	Lock::Lock()
		: ticketIDCounter(1), activeUsers(0), queue(0), queueSize(0)
	{

	}
	Lock::~Lock()
	{
		std::lock_guard lk{ mutex };

		if (queueSize > 0)
			Debug::Logger::LogFatal("SPH Library", "A lock has not been unlocked");
	}
	Lock::TicketID Lock::TryLockRead(const TimeInterval& timeInterval)
	{
		std::unique_lock lk{ mutex };

		if (queueSize == 0 || !PeekLastInQueue())
			if (!PushToQueue(true))
			{
				Debug::Logger::LogWarning("SPH Library", "Lock queue filled up");
				return 0;
			}

		TicketID ticketID = ticketIDCounter;
		IncreaseCounter();

		if (cv.wait_for(lk, std::chrono::seconds(1) * timeInterval.ToSeconds(), [&]() { return ticketIDCounter == ticketID; }))
			return 0;

		++activeUsers;

		return ticketID;
	}
	Lock::TicketID Lock::TryLockReadWrite(const TimeInterval& timeInterval)
	{
		std::unique_lock lk{ mutex };

		if (!PushToQueue(false))
		{
			Debug::Logger::LogWarning("SPH Library", "Lock queue filled up");
			return 0;
		}

		TicketID ticketID = ticketIDCounter;
		IncreaseCounter();

		if (!cv.wait_for(lk, std::chrono::seconds(1) * timeInterval.ToSeconds(), [&]() { return ticketIDCounter - queueSize == ticketID; }))
			return 0;

		++activeUsers;

		return ticketID;
	}
	void Lock::Unlock(TicketID ticketID)
	{
		if (ticketID == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Unlocking a lock with a 0 ticket, meaning the lock wasnt succesfully locked in the first place");
			return;
		}

		std::lock_guard lk{ mutex };

		if (queueSize != 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Unlocking a lock that was not locked by any object");
			return;
		}

		if (ticketID != ticketIDCounter - queueSize)
		{
			Debug::Logger::LogFatal("SPH Library", "Unlocking a lock with a invalid ticket");
			return;
		}

		--this->activeUsers;

		if (this->activeUsers == 0)
		{
			PopFromQueue();
			cv.notify_all();
		}
	}	
	void Lock::IncreaseCounter()
	{
		ticketIDCounter = (ticketIDCounter % (std::numeric_limits<TicketID>::max() - 1)) + 1;
	}
	bool Lock::PushToQueue(bool value)
	{
		if (queueSize == std::numeric_limits<decltype(queue)>::digits)
			return false;

		queue |= static_cast<uint64>(value) << queueSize;
		++queueSize;

		return true;
	}
	bool Lock::PopFromQueue()
	{
		bool value = static_cast<bool>(queue & 1);

		queue >>= 1;
		--queueSize;

		return value;
	}
	bool Lock::PeekLastInQueue()
	{
		return (queue >> (queueSize - 1)) & 1;
	}
}