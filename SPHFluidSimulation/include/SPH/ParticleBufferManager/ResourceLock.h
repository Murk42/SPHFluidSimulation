#pragma once
#include "BlazeEngineCore/BlazeEngineCore.h"

namespace SPH
{
	class Lock
	{
	public:
		using TicketID = uint8;
		Lock();
		~Lock();

		TicketID TryLockRead(const TimeInterval& timeInterval);
		TicketID TryLockReadWrite(const TimeInterval& timeInterval);
		void Unlock(TicketID ticketID);
	private:
		//False means a write waiting place
		uint64 queue;
		uint8 queueSize;

		uint activeUsers;		
		TicketID ticketIDCounter;

		std::condition_variable cv;
		std::mutex mutex;

		void IncreaseCounter();
		bool PushToQueue(bool value);
		bool PopFromQueue();
		bool PeekLastInQueue();
	};
}