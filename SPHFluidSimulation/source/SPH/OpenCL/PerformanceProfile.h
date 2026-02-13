#pragma once

namespace SPH
{

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

}