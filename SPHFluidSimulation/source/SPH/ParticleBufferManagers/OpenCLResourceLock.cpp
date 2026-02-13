#include "pch.h"
#include "SPH/ParticleBufferManagers/OpenCLResourceLock.h"
#include "OpenCLDebug.h"
#include "CL/cl.h"

namespace SPH
{
	OpenCLLock::OpenCLLock(cl_command_queue commandQueue)
		: commandQueue(commandQueue), lockState(0)
	{
	}
	OpenCLLock::~OpenCLLock()
	{
		if (!writeLockFinishedSignalEvents.Empty())
		{
			Debug::Logger::LogWarning("SPH Library", "Lock destroyed but there are write lock events that haven't been waited on.");
			for (const auto& event : writeLockFinishedSignalEvents)
				clReleaseEvent(event);
		}

		if (!readLockFinishedSignalEvents.Empty())
		{
			Debug::Logger::LogWarning("SPH Library", "Lock destroyed but there are read lock events that haven't been waited on.");
			for (const auto& event : readLockFinishedSignalEvents)
				clReleaseEvent(event);
		}

		if (lockState != 0)
			Debug::Logger::LogFatal("SPH Library", "A locked lock is being destroyed.");
	}
	bool OpenCLLock::HasWriteFinished()
	{
		bool allFinished = true;
		for (auto& event : writeLockFinishedSignalEvents)
		{
			cl_int status;

			CL_CALL(clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, NULL), false);

			if (status != CL_COMPLETE)
			{
				allFinished = false;
				break;
			}
		}

		return allFinished;
	}
	void OpenCLLock::WaitForWriteToFinish()
	{
		if (!writeLockFinishedSignalEvents.Empty())
		{
			CL_CALL(clWaitForEvents((cl_uint)writeLockFinishedSignalEvents.Count(), writeLockFinishedSignalEvents.Ptr()));

			for (const auto& event : writeLockFinishedSignalEvents)
				clReleaseEvent(event);

			writeLockFinishedSignalEvents.Clear();
		}
	}
	void OpenCLLock::WaitForReadToFinish()
	{
		if (!readLockFinishedSignalEvents.Empty())
		{
			CL_CALL(clWaitForEvents((cl_uint)readLockFinishedSignalEvents.Count(), readLockFinishedSignalEvents.Ptr()));

			for (const auto& event : readLockFinishedSignalEvents)
				clReleaseEvent(event);

			readLockFinishedSignalEvents.Clear();
		}
	}
	void OpenCLLock::LockRead(cl_event* signalEvent)
	{
		if (lockState != 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Trying to lock a lock for reading but it isn't unlocked.");
			return;
		}

		lockState = 1;

		if (writeLockFinishedSignalEvents.Empty())		
			*signalEvent = NULL;		
		else
		{
			CL_CALL(clEnqueueBarrierWithWaitList(commandQueue, (cl_uint)writeLockFinishedSignalEvents.Count(), writeLockFinishedSignalEvents.Ptr(), signalEvent));

			//Dont release events here because they might be waited by the second lock read
		}
	}	
	void OpenCLLock::LockWrite(cl_event* signalEvent)
	{
		if (lockState != 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Trying to lock a lock for writing but it isn't unlocked.");
			return;
		}

		lockState = 2;

		if (readLockFinishedSignalEvents.Empty())
			if (writeLockFinishedSignalEvents.Empty())
				*signalEvent = NULL;
			else
			{
				CL_CALL(clEnqueueBarrierWithWaitList(commandQueue, (cl_uint)writeLockFinishedSignalEvents.Count(), writeLockFinishedSignalEvents.Ptr(), signalEvent));
			}
		else
		{			
			CL_CALL(clEnqueueBarrierWithWaitList(commandQueue, (cl_uint)readLockFinishedSignalEvents.Count(), readLockFinishedSignalEvents.Ptr(), signalEvent));

			for (const auto& event : readLockFinishedSignalEvents)
				clReleaseEvent(event);

			readLockFinishedSignalEvents.Clear();
		}	

		for (const auto& event : writeLockFinishedSignalEvents)
			clReleaseEvent(event);

		writeLockFinishedSignalEvents.Clear();		
	}
	void OpenCLLock::UnlockRead(ArrayView<cl_event> waitEvents)
	{
		if (lockState == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Trying to unlock an unlocked lock.");
			return;
		}
		else if (lockState == 2)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock for reading that is locked for writing");
			return;
		}

		lockState = 0;

		for (auto event : waitEvents)
			clRetainEvent(event);

		readLockFinishedSignalEvents.Append(waitEvents);
	}
	void OpenCLLock::UnlockWrite(ArrayView<cl_event> waitEvents)
	{
		if (lockState == 0)
		{
			Debug::Logger::LogFatal("SPH Library", "Trying to unlock an unlocked lock.");
			return;
		}
		else if (lockState == 1)
		{
			Blaze::Debug::Logger::LogFatal("SPH Library", "Unlocking a lock for writing that is locked for reading");
			return;
		}

		lockState = 0;

		for (auto event : waitEvents)
			clRetainEvent(event);

		writeLockFinishedSignalEvents = waitEvents;
	}
}