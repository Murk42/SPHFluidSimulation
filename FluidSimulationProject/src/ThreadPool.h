#pragma once

class ThreadPool
{
public:
	ThreadPool();

	void AllocateThreads(uintMem threads);

	template<typename ... Args, std::invocable<uintMem, uintMem, Args...> F> 
	void RunTask(const F& task, Args&& ... args)
	{	
		if (threads.Empty())		
			task(0, 1);
		else		
			for (uintMem i = 0; i < threads.Count(); ++i)			
				threads[i].Run(task, (uintMem)i, threads.Count(), std::forward<Args>(args)...);									
	}

	bool IsAnyRunning();
	uintMem WaitForAll(float timeout);

	inline uintMem ThreadCount() const { return threads.Count(); }
private:
	Array<Blaze::Thread> threads;
};