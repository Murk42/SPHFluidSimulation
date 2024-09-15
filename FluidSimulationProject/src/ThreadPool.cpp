#include "pch.h"
#include "ThreadPool.h"

ThreadPool::ThreadPool()
{
}

void ThreadPool::AllocateThreads(uintMem threadCount)
{
	threads.Resize(threadCount);	
}

bool ThreadPool::IsAnyRunning()
{
	for (auto& thread : threads)
		if (thread.IsRunning())
			return true;
	return false;
}

uintMem ThreadPool::WaitForAll(float timeout)
{
	uintMem count = 0;
	for (auto& thread : threads)
		count += thread.WaitToFinish(timeout) ? 1 : 0;
	return count;
}
