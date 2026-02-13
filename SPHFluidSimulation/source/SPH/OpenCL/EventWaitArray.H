#pragma once

namespace SPH
{

	template<uintMem MaxEvents>
	struct EventWaitArray
	{
	public:

		EventWaitArray() : events{ }, count(0) {}
		EventWaitArray(const std::initializer_list<cl_event>& events)
			: events{ }, count(0)
		{
			for (auto& event : events)
				if (event != NULL)
				{
					if (count == MaxEvents)
						Debug::Logger::LogFatal("SPH Library", "All events are exhausted");

					this->events[count++] = event;
				}
		}
		EventWaitArray(const std::initializer_list<cl::Event>& events)
			: events{ }, count(0)
		{
			for (auto& event : events)
				if (event() != NULL)
				{
					if (count == MaxEvents)
						Debug::Logger::LogFatal("SPH Library", "All events are exhausted");

					this->events[count++] = event();
				}
		}

		void Release()
		{
			for (uintMem i = 0; i < count; ++i)
				CL_CALL(clReleaseEvent(events[i]));

			count = 0;
		}

		cl_event* Ptr()
		{
			if (count == 0)
				return nullptr;

			return events;
		}
		uintMem Count()
		{
			return count;
		}

		inline operator cl_event* ()
		{
			while (count != MaxEvents && events[count] != NULL)
				++count;

			if (count == MaxEvents)
				Debug::Logger::LogFatal("SPH Library", "All events are exhausted");

			return &events[count];
		}
		inline operator ArrayView<cl_event>()
		{
			while (count != MaxEvents && events[count] != NULL)
				++count;
			return ArrayView<cl_event>(events, count);
		}
	private:
		cl_event events[MaxEvents];
		uintMem count;
	};
}