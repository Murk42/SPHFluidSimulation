#include "pch.h"
#include "SPH/ParticleBufferManager/OpenCLResourceLock.h"
#include "CL/cl.h"

namespace SPH
{
	OpenCLLock::OpenCLLock(cl_command_queue commandQueue)
		: commandQueue(commandQueue), queue(0), queueSize(0), ticketIDCounter(1), writeSignalEvent(NULL)
	{
	}
	OpenCLLock::~OpenCLLock()
	{
	}
	OpenCLLock::TicketID OpenCLLock::TryLockRead(cl_event* signalEvent)
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

		cv.wait(lk, [&]() { return writeSignalEvent != NULL; });

		if (writeSignalEvent)
			clEnqueueBarrierWithWaitList(commandQueue, 1, &writeSignalEvent, signalEvent);
		else		
			*signalEvent = NULL;
		
		readSignalEvents.AddBack((cl_event)NULL);

		return ticketID;
	}
	OpenCLLock::TicketID OpenCLLock::TryLockIfActiveRead(cl_event* signalEvent)
	{
		std::unique_lock lk{ mutex };

		if (queueSize == 0)
		{
			if (!PushToQueue(true))
			{
				Debug::Logger::LogWarning("SPH Library", "Lock queue filled up");
				return 0;
			}
		}
		else if (!PeekFirstInQueue() || queueSize > 1)
			return 0;

		TicketID ticketID = ticketIDCounter;
		IncreaseCounter();

		cv.wait(lk, [&]() { return writeSignalEvent != NULL; });

		if (writeSignalEvent)
			clEnqueueBarrierWithWaitList(commandQueue, 1, &writeSignalEvent, signalEvent);
		else
			*signalEvent = NULL;

		readSignalEvents.AddBack((cl_event)NULL);

		return ticketID;
	}
	OpenCLLock::TicketID OpenCLLock::TryLockReadWrite(cl_event* signalEvent)
	{
		std::unique_lock lk{ mutex };

		bool previousInQueue = PeekLastInQueue();

		if (!PushToQueue(false))
		{
			Debug::Logger::LogWarning("SPH Library", "Lock queue filled up");
			return 0;
		}

		TicketID ticketID = ticketIDCounter;
		IncreaseCounter();

		if (previousInQueue)
		{
			for (auto& readSignalEvent : readSignalEvents)			
				cv.wait(lk, [&]() { return readSignalEvent != NULL; });
			
			clEnqueueBarrierWithWaitList(commandQueue, readSignalEvents.Count(), readSignalEvents.Ptr(), signalEvent);
		}
		else
		{
			cv.wait(lk, [&]() { return writeSignalEvent != NULL; });

			clEnqueueBarrierWithWaitList(commandQueue, 1, &writeSignalEvent, signalEvent);
		}

		writeSignalEvent = NULL;

		return ticketID;
	}
	void OpenCLLock::Unlock(TicketID ticketID, ArrayView<cl_event> waitEvents)
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

		if (waitEvents.Empty())
		{
			Debug::Logger::LogFatal("SPH Library", "Unlocking a lock with a empty wait event list");
			return;
		}			

		if (PeekFirstInQueue())
		{
			for (auto& readSignalEvent : readSignalEvents)
				if (readSignalEvent == NULL)
				{					
					if (waitEvents.Count() == 1)					
						readSignalEvent = waitEvents.First();											
					else					
						clEnqueueBarrierWithWaitList(commandQueue, waitEvents.Count(), waitEvents.Ptr(), &readSignalEvent);					
					
					break;
				}

			if (readSignalEvents.Last() != NULL)
				PopFromQueue();
		}
		else
		{
			if (waitEvents.Count() == 1)			
				writeSignalEvent = waitEvents.First();			
			else
				clEnqueueBarrierWithWaitList(commandQueue, waitEvents.Count(), waitEvents.Ptr(), &writeSignalEvent);

			PopFromQueue();
		}

		cv.notify_all();
	}	
	void OpenCLLock::IncreaseCounter()
	{
		ticketIDCounter = (ticketIDCounter % (std::numeric_limits<TicketID>::max() - 1)) + 1;
	}
	bool OpenCLLock::PushToQueue(bool value)
	{
		if (queueSize == std::numeric_limits<decltype(queue)>::digits)
			return false;

		queue |= static_cast<uint64>(value) << queueSize;
		++queueSize;

		return true;
	}
	bool OpenCLLock::PopFromQueue()
	{
		bool value = static_cast<bool>(queue & 1);

		queue >>= 1;
		--queueSize;

		return value;
	}
	bool OpenCLLock::PeekLastInQueue()
	{
		return (queue >> (queueSize - 1)) & 1;
	}
	bool OpenCLLock::PeekFirstInQueue()
	{
		return queue & 1;
	}
}