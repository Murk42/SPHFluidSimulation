#include "pch.h"
#include "SPH/System/System.h"

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

	void System::Initialize(const SystemParameters& initParams, ParticleBufferSet& bufferSet, Array<DynamicParticle> dynamicParticles, Array<StaticParticle> staticParticles)
	{
		Clear();

		//Array<DynamicParticle> dynamicParticles;
		//Array<StaticParticle> staticParticles;
		//
		//
		//if (initParams.staticParticleGenerationParameters.generator)
		//	initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		//if (initParams.dynamicParticleGenerationParameters.generator)
		//	initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);
		//
		//bufferSet.Initialize(initParams.bufferCount, dynamicParticles);


		bufferSet.SetDynamicParticles(dynamicParticles);
		CreateDynamicParticlesBuffers(bufferSet, initParams.particleBehaviourParameters.maxInteractionDistance);
		CreateStaticParticlesBuffers(staticParticles, initParams.particleBehaviourParameters.maxInteractionDistance);
		InitializeInternal(initParams);
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
}