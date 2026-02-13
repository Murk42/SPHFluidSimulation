#include "pch.h"
#include "SPH/Concurrency/ThreadParallelTaskManager.h"

namespace SPH
{
	ThreadContext::ThreadContext()
		: manager(nullptr), threadIndex(0), threadCount(0)
	{
	}
	ThreadContext::ThreadContext(ThreadContext&& other) noexcept
		: manager(other.manager), threadIndex(other.threadIndex), threadCount(other.threadCount)
	{
		other.manager = nullptr;
		other.threadCount = 0;
		other.threadIndex = 0;
	}
	void ThreadContext::SyncThreads() const
	{
		if (manager == nullptr)
		{
			Debug::Logger::LogFatal("SPH Library", "Invalid thread context, manager is nullptr.");
			return;
		}

		if (manager->threadPool.ThreadCount() == 0)
			return;

		std::unique_lock<std::mutex> lock{ manager->stateMutex };
		SyncThreads(lock);
	}
	ThreadContext& ThreadContext::operator=(ThreadContext&& other) noexcept
	{
		manager = other.manager;
		threadIndex = other.threadIndex;
		threadCount = other.threadCount;

		other.manager = nullptr;
		other.threadIndex = 0;
		other.threadCount = 0;

		return *this;
	}
	ThreadContext::ThreadContext(ThreadParallelTaskManager& manager, uintMem threadIndex, uintMem threadCount)
		: manager(&manager), threadIndex(threadIndex), threadCount(threadCount)
	{
	}
	void ThreadContext::SyncThreads(std::unique_lock<std::mutex>& lock) const
	{
		++manager->threadSyncCount1;
		manager->syncCV.notify_all();
		manager->syncCV.wait(lock, [&]() { return manager->threadSyncCount1 % manager->threadPool.ThreadCount() == 0; });

		++manager->threadSyncCount2;
		manager->syncCV.notify_all();
		manager->syncCV.wait(lock, [&]() { return manager->threadSyncCount2 % manager->threadPool.ThreadCount() == 0; });
	}
	ThreadParallelTaskManager::ThreadParallelTaskManager() 
		: threadIdleCount(0), threadSyncCount1(0), threadSyncCount2(0), exit(false)
	{		
	}
	ThreadParallelTaskManager::~ThreadParallelTaskManager()
	{
		if (threadPool.ThreadCount() == 0)
			return;

		if (!threadPool.IsAnyRunning())
			return;

		{
			std::lock_guard lock{ stateMutex };
			exit = true;
			stateCV.notify_all();
		}

		if (threadPool.WaitForAll(1.0f) != threadPool.ThreadCount())
			Debug::Logger::LogWarning("Client", "Threads didn't exit on time");
	}
	void ThreadParallelTaskManager::AllocateThreads(uintMem threads)
	{			
		threadPool.WaitForAll(5.0f);		
		threadPool.AllocateThreads(threads);
		if (threads != 0)
			threadPool.RunTask([](uintMem threadIndex, uintMem threadCount, ThreadParallelTaskManager& manager) -> uint {
				manager.SimulationThreadFunc(ThreadContext(manager, threadIndex, threadCount));
				return 0;
				}, *this);
	}
	void ThreadParallelTaskManager::FinishTasks()
	{
		if (!threadPool.IsAnyRunning())
			return;

		std::unique_lock<std::mutex> lock{ stateMutex };
		stateCV.wait(lock, [&]() { return threadIdleCount == threadPool.ThreadCount(); });
	}	
	void ThreadParallelTaskManager::SimulationThreadFunc(ThreadContext context)
	{
		if (context.manager == nullptr)
		{
			Debug::Logger::LogFatal("SPH Library", "Invalid thread context, manager is nullptr.");
			return;
		}

		auto& manager = *context.manager;

			
		std::unique_lock<std::mutex> lock{ manager.stateMutex };
		while (true)
		{			
			++manager.threadIdleCount;
			manager.stateCV.notify_all();
			manager.stateCV.wait(lock, [&]() { return manager.exit || !manager.tasks.Empty(); });
			--manager.threadIdleCount;

			if (manager.exit)
				break;

			auto taskIt = manager.tasks.FirstIterator();
			TaskDataBase* task = taskIt.GetValue<TaskDataBase>();			
			lock.unlock();

			task->Execute(context);

			lock.lock();
			context.SyncThreads(lock); 

			if (context.threadIndex == 0)
			{
				manager.tasks.EraseFirst();
				manager.stateCV.notify_all();				
			}

			context.SyncThreads(lock);
		}
	}
}