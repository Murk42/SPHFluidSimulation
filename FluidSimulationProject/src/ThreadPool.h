#pragma once



class ThreadPool
{
public:
	ThreadPool();

	void AllocateThreads(uintMem threads);

	template<std::invocable<uintMem, uintMem> F> 
	void RunTask(uintMem begin, uintMem end, const F& task)
	{	
		if (threads.Empty())
		{
			task(begin, end);
		}
		else
		{
			uintMem count = end - begin;
			uintMem offset = begin;
			for (uint i = 0; i < threads.Count(); ++i)
			{
				uintMem elementCount = count / (threads.Count() - i);
				threads[i].Run(task, uintMem(offset), uintMem(offset + elementCount));
				count -= elementCount;
				offset += elementCount;
			}
		}
	}

	bool IsAnyRunning();
	uintMem WaitForAll(float timeout);

	inline uintMem ThreadCount() const { return threads.Count(); }
private:
	Array<Blaze::Thread> threads;
};