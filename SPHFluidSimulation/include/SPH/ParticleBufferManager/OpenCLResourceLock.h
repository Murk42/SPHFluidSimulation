#pragma once

namespace SPH
{
	class OpenCLLock
	{
	public:		
		OpenCLLock(cl_command_queue commandQueue);
		~OpenCLLock();
		
		bool HasWriteFinished();	

		void WaitForWriteToFinish();
		void WaitForReadToFinish();

		void LockRead(cl_event* signalEvent);		
		void LockWrite(cl_event* signalEvent);		

		void UnlockRead(ArrayView<cl_event> waitEvents);
		void UnlockWrite(ArrayView<cl_event> waitEvents);

		inline cl_command_queue GetCommandQueue() const { return commandQueue; }
	private:		
		Array<cl_event> readLockFinishedSignalEvents;		
		Array<cl_event> writeLockFinishedSignalEvents;
		
		cl_command_queue commandQueue;		

		//0 - unlocked, 1 - read locked, 2 - write locked
		uint8 lockState;
	};
}