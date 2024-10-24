#include "pch.h"
#include "System.h"

#include "JSONParsing.h"


#include "SPH/ParticleGenerator/FilledBoxParticleGenerator.h"
#include "SPH/ParticleGenerator/BoxShellParticleGenerator.h"

namespace SPH
{
	template<typename T>
	SPH::ParticleGenerator<T>* CreateFilledBoxParticleGenerator(const JSON& json)
	{
		SPH::FilledBoxParticleParameters params;
		params.randomOffsetIntensity = json["randomOffsetIntensity"];
		params.particlesPerUnit = json["particlesPerUnit"];
		params.spawnVolumeOffset = ConvertVec3f(json["spawnVolumeOffset"]);
		params.spawnVolumeSize = ConvertVec3f(json["spawnVolumeSize"]);

		return new SPH::FilledBoxParticleGenerator<T>(params);
	}

	template<typename T>
	SPH::ParticleGenerator<T>* CreateBoxShellParticleGenerator(const JSON& json)
	{
		SPH::BoxShellParticleParameters params;
		params.particleDistance = json["particleDistance"];
		params.randomOffsetIntensity = json["randomOffsetIntensity"];
		params.spawnVolumeOffset = ConvertVec3f(json["spawnVolumeOffset"]);
		params.spawnVolumeSize = ConvertVec3f(json["spawnVolumeSize"]);

		return new SPH::BoxShellParticleGenerator<T>(params);
	}

	template<typename T>
	std::shared_ptr<SPH::ParticleGenerator<T>> CreateGenerator(const JSON& json)
	{
		std::string generatorType = json["generatorType"];
		if (generatorType == "FilledBoxParticleGenerator")
			return std::shared_ptr<SPH::ParticleGenerator<T>>(CreateFilledBoxParticleGenerator<T>(json));
		else if (generatorType == "BoxShellParticleGenerator")
			return std::shared_ptr<SPH::ParticleGenerator<T>>(CreateBoxShellParticleGenerator<T>(json));
		return nullptr;
	}

    void SystemInitParameters::ParseJSON(const JSON& json)
    {
		try {			
			dynamicParticleGenerationParameters.generator = CreateGenerator<DynamicParticle>(json["dynamicParticleGenerationParameters"]);
			staticParticleGenerationParameters.generator = CreateGenerator<StaticParticle>(json["staticParticleGenerationParameters"]);

			auto& particleBehaviourParametersJSON = json["particleBehaviourParameters"];
			particleBehaviourParameters.particleMass = particleBehaviourParametersJSON["particleMass"];
			particleBehaviourParameters.gasConstant = particleBehaviourParametersJSON["gasConstant"];
			particleBehaviourParameters.elasticity = particleBehaviourParametersJSON["elasticity"];
			particleBehaviourParameters.viscosity = particleBehaviourParametersJSON["viscosity"];
			Vec3f gravity = ConvertVec3f(particleBehaviourParametersJSON["gravity"]);
			particleBehaviourParameters.gravityX = gravity.x;
			particleBehaviourParameters.gravityY = gravity.y;
			particleBehaviourParameters.gravityZ = gravity.z;
			particleBehaviourParameters.restDensity = particleBehaviourParametersJSON["restDensity"];
			particleBehaviourParameters.maxInteractionDistance = particleBehaviourParametersJSON["maxInteractionDistance"];

			auto& particleBoundParametersJSON = json["particleBoundParameters"];
			particleBoundParameters.bounded = particleBoundParametersJSON["bounded"];

			if (particleBoundParameters.bounded)
			{
				particleBoundParameters.boxOffset = ConvertVec3f(particleBoundParametersJSON["boxOffset"]);
				particleBoundParameters.boxSize = ConvertVec3f(particleBoundParametersJSON["boxSize"]);
				particleBoundParameters.wallElasticity = particleBoundParametersJSON["wallElasticity"];
				particleBoundParameters.floorAndRoofElasticity = particleBoundParametersJSON["floorAndRoofElasticity"];
				particleBoundParameters.bounded = particleBoundParametersJSON["bounded"];
				particleBoundParameters.boundedByRoof = particleBoundParametersJSON["boundedByRoof"];
				particleBoundParameters.boundedByWalls = particleBoundParametersJSON["boundedByWalls"];
			}
				
			bufferCount = json["bufferCount"];
			hashesPerDynamicParticle = json["hashesPerDynamicParticle"];
			hashesPerStaticParticle = json["hashesPerStaticParticle"];
						
			if (json.contains("implementationSpecifics"))
			{
				auto& implementationSpecificsJSON = json["implementationSpecifics"];
				for (auto it = implementationSpecificsJSON.begin(); it != implementationSpecificsJSON.end(); ++it)
				{
					auto& specifics = implementationSpecifics.Insert(ConvertString(it.key())).iterator->value;					
					auto& specificsJSON = it.value();

					for (auto it = specificsJSON.begin(); it != specificsJSON.end(); ++it)
					{
						if (it.value().is_string())
							specifics.Insert<String>(String(it.key().c_str(), it.key().size()), ConvertString(it.value()));
						else if (it.value().is_number_float())
							specifics.Insert<float>(String(it.key().c_str(), it.key().size()), (float)it.value());
						else if (it.value().is_number_unsigned())
							specifics.Insert<uint64>(String(it.key().c_str(), it.key().size()), (uint64)it.value());
						else if (it.value().is_number_integer())
							specifics.Insert<int64>(String(it.key().c_str(), it.key().size()), (int64)it.value());
						else if (it.value().is_array() && it.value().size() == 3 && it.value()[0].is_number_float() && it.value()[1].is_number_float() && it.value()[2].is_number_float())
							specifics.Insert<Vec3f>(String(it.key().c_str(), it.key().size()), ConvertVec3f(it.value()));
						else if (it.value().is_boolean())
							specifics.Insert<bool>(String(it.key().c_str(), it.key().size()), (bool)it.value());
						else
							Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type");
					}
				}
			}

		} catch (nlohmann::json::parse_error& ex)
		{ 
			Debug::Logger::LogWarning("Client", "Failed to parse system init parameters JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			*this = {};
		}
    }
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<bool>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<bool>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");		
	}
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<Vec3f>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<Vec3f>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");
	}
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<uint64>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<uint64>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");
	}
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<int64>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<int64>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");
	}
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<float>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<float>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");

	}
	void SystemInitParameters::ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<String>&& entry) const
	{
		auto it = map.Find(entry.name);
		if (it.IsNull())
			return;
		if (auto* ptr = it.GetValue<String>())
			entry.value = *ptr;
		else
			Debug::Logger::LogWarning("Client", "Unrecognized implementation specifics entry value type. Name: \"" + entry.name + "\"");
	}
	void System::Initialize(const SystemInitParameters& initParams, ParticleBufferSet& bufferSet)
	{
		Clear();

		Array<DynamicParticle> dynamicParticles;
		Array<StaticParticle> staticParticles;


		if (initParams.staticParticleGenerationParameters.generator)
			initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		if (initParams.dynamicParticleGenerationParameters.generator)
			initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);

		bufferSet.Initialize(initParams.bufferCount, dynamicParticles);

		staticParticlesGeneratedEventDispatcher.Call({ staticParticles });

		CreateStaticParticlesBuffers(staticParticles, initParams.hashesPerStaticParticle, initParams.particleBehaviourParameters.maxInteractionDistance);
		CreateDynamicParticlesBuffers(bufferSet, initParams.hashesPerDynamicParticle, initParams.particleBehaviourParameters.maxInteractionDistance);
		InitializeInternal(initParams);
	}
}