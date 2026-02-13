#include "pch.h"
#include "SPH/Core/SimulationEngine.h"
#include "SPH/Kernels/SPHFunctions.h"

namespace SPH
{
	bool ParticleSimulationParameters::ParseParameter(StringView name, float& value) const
	{
		auto it = otherParameters.Find(name);

		if (!it.IsNull())
			if (!it->value.ConvertToDecimal(value, FloatStringConvertFormat::General))
				Debug::Logger::LogWarning("Client", "Failed to convert SPH system parameter named \"" + name + "\"");
			else
				return true;

		return false;
	}

	bool ParticleSimulationParameters::ParseParameter(StringView name, bool& value) const
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

	namespace Details
	{
		inline Vec3u GetCell(Vec3f position, float maxInteractionDistance);
		inline uint GetHash(Vec3u cell);
		float SmoothingKernelConstant(float h);
		inline float SmoothingKernelD0(float r, float maxInteractionDistance);
		inline float SmoothingKernelD1(float r, float maxInteractionDistance);
		inline float SmoothingKernelD2(float r, float maxInteractionDistance);
	}

	Vec3u SimulationEngine::GetCell(Vec3f position, float maxInteractionDistance)
	{
		return Details::GetCell(position, maxInteractionDistance);
	}
	uint SimulationEngine::GetHash(Vec3u cell)
	{
		return Details::GetHash(cell);
	}
	float SimulationEngine::SmoothingKernelConstant(float h)
	{
		return Details::SmoothingKernelConstant(h);
	}
	float SimulationEngine::SmoothingKernelD0(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD0(r, maxInteractionDistance);
	}
	float SimulationEngine::SmoothingKernelD1(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD1(r, maxInteractionDistance);
	}
	float SimulationEngine::SmoothingKernelD2(float r, float maxInteractionDistance)
	{
		return Details::SmoothingKernelD2(r, maxInteractionDistance);
	}
}