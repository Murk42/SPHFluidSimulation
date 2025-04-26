#pragma once
#include "SPH/ThreadPool.h"

namespace SPH
{
	class ThreadParallelTaskManager;

	class ThreadContext
	{
	public:
		ThreadContext();
		ThreadContext(ThreadContext&& other) noexcept;

		inline uintMem GetThreadCount() const { return threadCount; }
		inline uintMem GetThreadIndex() const { return threadIndex; }

		void SyncThreads() const;

		ThreadContext& operator=(ThreadContext&& other) noexcept;
	private:
		ThreadContext(ThreadParallelTaskManager& manager, uintMem threadIndex, uintMem threadCount);

		void SyncThreads(std::unique_lock<std::mutex>& lock) const;

		uintMem threadIndex;
		uintMem threadCount;
		ThreadParallelTaskManager* manager;

		friend class ThreadParallelTaskManager;
	};
	 
	class ThreadParallelTaskManager
	{
	public:		
		template<typename Task>
		using TaskFunction = void(*)(const ThreadContext&, Task&);

		ThreadParallelTaskManager();
		~ThreadParallelTaskManager();

		void AllocateThreads(uintMem threads);

		void FinishTasks();
		template<typename Task>
		void EnqueueTask(TaskFunction<Task> function, Task&& task);
		template<typename Task>
		bool TryEnqueueTask(TaskFunction<Task> function, Task&& task);

		uintMem ThreadCount() const { return threadPool.ThreadCount(); }
	private:
		struct TaskDataBase
		{
			virtual void Execute(const ThreadContext&) { };
		};
		template<typename Task>
		struct TaskData : TaskDataBase
		{
			TaskFunction<Task> function;
			Task task;

			TaskData(TaskData&& other) noexcept : function(other.function), task(std::move(other.task)) { }
			TaskData(TaskFunction<Task> function, Task&& task) : function(function), task(std::move(task)) { }

			void Execute(const ThreadContext& context) override { function(context, task); }
		};

		ThreadPool threadPool;		

		std::mutex stateMutex;
		std::condition_variable stateCV;
		VirtualList<TaskDataBase> tasks;
		bool exit;
		uintMem threadIdleCount;		

		mutable std::mutex syncMutex;
		mutable std::condition_variable syncCV;
		mutable uintMem threadSyncCount1;
		mutable uintMem threadSyncCount2;

		static void SimulationThreadFunc(ThreadContext context);

		friend class ThreadContext;
	};
	template<typename Task>
	inline void ThreadParallelTaskManager::EnqueueTask(TaskFunction<Task> function, Task&& task)
	{
		if (threadPool.ThreadCount() == 0)
			function(ThreadContext(*this, 0, 1), task);
		else
		{
			std::unique_lock lk{ stateMutex };
			stateCV.wait(lk, [&]() { return tasks.Count() < 3; });

			tasks.AddBack<TaskData<Task>>(function, std::move(task));

			stateCV.notify_all();
		}
	}
	template<typename Task>
	inline bool ThreadParallelTaskManager::TryEnqueueTask(TaskFunction<Task> function, Task&& task)
	{
		if (threadPool.ThreadCount() == 0)
			function(ThreadContext(*this, 0, 1), task);
		else
		{
			std::unique_lock lk{ stateMutex };

			if (tasks.Count() >= 3)
				return false;

			tasks.AddBack<TaskData<Task>>(function, std::move(task));

			stateCV.notify_all();
		}

		return true;
	}
}