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

		} catch (nlohmann::json::parse_error& ex)
		{ 
			Debug::Logger::LogWarning("Client", "Failed to parse system init parameters JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			*this = {};
		}
    }
	void System::Initialize(const SystemInitParameters& initParams)
	{
		Clear();

		Array<DynamicParticle> dynamicParticles;
		Array<StaticParticle> staticParticles;

		if (initParams.staticParticleGenerationParameters.generator)
			initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		if (initParams.dynamicParticleGenerationParameters.generator)
			initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);

		CreateStaticParticlesBuffers(staticParticles, initParams.hashesPerStaticParticle, initParams.particleBehaviourParameters.maxInteractionDistance);
		CreateDynamicParticlesBuffers(dynamicParticles, initParams.bufferCount, initParams.hashesPerDynamicParticle, initParams.particleBehaviourParameters.maxInteractionDistance);
		InitializeInternal(initParams);
	}
}