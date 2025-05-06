#include "pch.h"
#include "SPH/Core/System.h"
#include "SPH/kernels/SPHFunctions.h"

namespace SPH
{	
	bool SystemParameters::ParseParameter(StringView name, float& value) const
	{
		auto it = otherParameters.Find(name);
		
		if (!it.IsNull())
			if (Result result = StringParsing::Convert((StringView)it->value, value))
				Debug::Logger::LogWarning("Client", "Failed to convert SPH system parameter named \"" + name + "\"");
			else
				return true;

		return false;
	}

	bool SystemParameters::ParseParameter(StringView name, bool& value) const
	{
		auto it = otherParameters.Find(name);

		if (!it.IsNull())
		{			
			if (it->value == "true" || it->value == "1")
				value = true;
			else if (it->value == "false" || it->value == "0")
				value = false;
			else
				return false;

			return true;
		}

		return false;			
	}

	SystemProfilingData::SystemProfilingData() :
		timePerStep_s(0)
	{
	}
	SystemProfilingData::SystemProfilingData(const SystemProfilingData& other) :
		timePerStep_s(other.timePerStep_s)
	{
		for (auto it = other.implementationSpecific.FirstIterator(); it != other.implementationSpecific.BehindIterator(); ++it)
			if (const float* ptr = it.GetValue<float>())
			{
				String string = *it.GetKey();
				float f = *ptr;
				implementationSpecific.Insert<float>(std::move(string), std::move(f));
			}
	}
	SystemProfilingData::SystemProfilingData(SystemProfilingData&& other) noexcept : 
		timePerStep_s(other.timePerStep_s), implementationSpecific(std::move(other.implementationSpecific)) 
	{
	}
	SystemProfilingData& SystemProfilingData::operator=(const SystemProfilingData& other) noexcept
	{
		for (auto it = other.implementationSpecific.FirstIterator(); it != other.implementationSpecific.BehindIterator(); ++it)
			if (const float* ptr = it.GetValue<float>())
			{
				String string = *it.GetKey();
				float f = *ptr;
				implementationSpecific.Insert<float>(std::move(string), std::move(f));
			}
		return *this;
	}
	SystemProfilingData& SystemProfilingData::operator=(SystemProfilingData&& other) noexcept
	{
		timePerStep_s = other.timePerStep_s;
		implementationSpecific = std::move(other.implementationSpecific);
		return *this;
	}

	namespace Details
	{
		inline Vec3u GetCell(Vec3f position, float maxInteractionDistance);
		inline uint GetHash(Vec3u cell);
		float SmoothingKernelConstant(float h);
		inline float SmoothingKernelD0(float r, float maxInteractionDistance);
		inline float SmoothingKernelD1(float r, float maxInteractionDistance);
		inline float SmoothingKernelD2(float r, float maxInteractionDistance);
	}

	Vec3u System::GetCell(Vec3f position, float maxInteractionDistance)
	{
		return Details::GetCell(position, maxInteractionDistance);		
	}
	uint System::GetHash(Vec3u cell)
	{
		return Details::GetHash(cell);		
	}
	float System::SmoothingKernelConstant(float h)
	{		
		return Details::SmoothingKernelConstant(h);
	}
	float System::SmoothingKernelD0(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD0(r, maxInteractionDistance);
	}
	float System::SmoothingKernelD1(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD1(r, maxInteractionDistance);
	}
	float System::SmoothingKernelD2(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD2(r, maxInteractionDistance);
	}	
}