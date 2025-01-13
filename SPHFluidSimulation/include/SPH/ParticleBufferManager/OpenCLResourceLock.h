#pragma once

namespace SPH
{
	class OpenCLLock
	{
	public:
		using TicketID = uint8;
		OpenCLLock(cl_command_queue commandQueue);
		~OpenCLLock();

		TicketID TryLockRead(cl_event* signalEvent);
		TicketID TryLockIfActiveRead(cl_event* signalEvent);
		TicketID TryLockReadWrite(cl_event* signalEvent);
		void Unlock(TicketID ticketID, ArrayView<cl_event> waitEvents);
	private:
		cl_command_queue commandQueue;
		Array<cl_event> readSignalEvents;
		cl_event writeSignalEvent;		

		//False means a write waiting place
		uint64 queue;
		uint8 queueSize;
				
		TicketID ticketIDCounter;

		std::condition_variable cv;
		std::mutex mutex;

		void IncreaseCounter();
		bool PushToQueue(bool value);
		bool PopFromQueue();
		bool PeekLastInQueue();
		bool PeekFirstInQueue();
	};
}