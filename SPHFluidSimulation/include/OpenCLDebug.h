#pragma once

void PrintOpenCLError(uint code);

#pragma warning (disable: 4003)
#define CL_CALL(x, r) { cl_int ret; if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; } }
#define CL_CHECK_RET(x, r) { cl_int ret; x; if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; } }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }

bool CheckForExtensions(cl_device_id device, const Set<String>&requiredExtensions);

void PrintDeviceInfo(cl_device_id device, WriteStream& stream);
void PrintKernelInfo(cl_kernel kernel, cl_device_id device, WriteStream& stream);

void GetDeviceVersion(cl_device_id device, uintMem& major, uintMem& minor);

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

struct Measurement
{
	String name;
	uint64 queuedTime;
	uint64 submitTime;
	uint64 startTime;
	uint64 endTime;
};
struct PendingMeasurement
{
	String name;
	cl_event event;
};

template<uintMem MaxMeasurements>
class PerformanceProfile
{
public:	

	PerformanceProfile();

	void AddPendingMeasurement(StringView name, cl_event event);

	Array<Measurement> GetMeasurements();
private:	
	PendingMeasurement pendingMeasurements[MaxMeasurements];
	uint pendingMeasurmentsCount;
};

template<uintMem MaxMeasurements>
inline PerformanceProfile<MaxMeasurements>::PerformanceProfile()
	: pendingMeasurements{}, pendingMeasurmentsCount(0)
{

}

template<uintMem MaxMeasurements>
inline void PerformanceProfile<MaxMeasurements>::AddPendingMeasurement(StringView name, cl_event event)
{
	if (pendingMeasurmentsCount == MaxMeasurements)
	{
		Debug::Logger::LogWarning("SPH Library", "All measuring slots taken!");
		return;
	}

	pendingMeasurements[pendingMeasurmentsCount++] = { name, event };
	CL_CALL(clRetainEvent(event));
}

template<uintMem MaxMeasurements>
inline auto PerformanceProfile<MaxMeasurements>::GetMeasurements() -> Array<Measurement>
{
	Array<Measurement> measurements(pendingMeasurmentsCount);

	for (uintMem i = 0; i < pendingMeasurmentsCount; ++i)
	{
		measurements[i].name = std::move(pendingMeasurements[i].name);

		CL_CALL(clWaitForEvents(1, &pendingMeasurements[i].event), {});

		CL_CALL(clGetEventProfilingInfo(pendingMeasurements[i].event, CL_PROFILING_COMMAND_QUEUED, sizeof(uint64), &measurements[i].queuedTime, nullptr), {});
		CL_CALL(clGetEventProfilingInfo(pendingMeasurements[i].event, CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64), &measurements[i].submitTime, nullptr), {});
		CL_CALL(clGetEventProfilingInfo(pendingMeasurements[i].event, CL_PROFILING_COMMAND_START, sizeof(uint64), &measurements[i].startTime, nullptr), {});
		CL_CALL(clGetEventProfilingInfo(pendingMeasurements[i].event, CL_PROFILING_COMMAND_END, sizeof(uint64), &measurements[i].endTime, nullptr), {});

		CL_CALL(clReleaseEvent(pendingMeasurements[i].event), {});
		pendingMeasurements[i].event = NULL;
	}

	pendingMeasurmentsCount = 0;

	return measurements;
}
