#include "pch.h"
#include "SPH/Core/Scene.h"
#include "JSONParsing.h"
#include "SPH/ParticleSetBlueprint/BoxShellParticleSetBlueprint.h"
#include "SPH/ParticleSetBlueprint/FilledBoxParticleSetBlueprint.h"

namespace SPH
{	
	Map<String, std::function<ParticleSetBlueprint* ()>> Scene::particleSetBlueprintCreators = { {
		{ "BoxShell", []() { return new BoxShellParticleParticleSetBlueprint(); }},
		{ "FilledBox", []() { return new FilledBoxParticleSetBlueprint(); } }
	} };

	Scene::Scene()
		: validScene(false)
	{
	}

	Scene::~Scene()
	{
		for (auto& layer : layers)
			for (auto& particleSetBlueprint : layer.value)
				delete particleSetBlueprint;
	}

	Array<Vec3f> Scene::GenerateLayerParticlePositions(StringView layerName)
	{
		auto it = layers.Find(layerName);

		if (it.IsNull())
			return { };

		Array<Vec3f> particles;

		for (auto& particleSetBlueprint : it->value)
			particleSetBlueprint->AppendParticlePositions(particles);
		
		return particles;
	}
	bool Scene::LoadScene(const Path& path)
	{
		File file{ path, FileAccessPermission::Read };

		if (!file.IsOpen())
		{
			MarkAsInvalid();
			return false;
		}

		ReadSubStream subStream(file, 0, file.GetSize());
		return LoadScene(subStream);		
	}
	bool Scene::LoadScene(ReadSubStream& stream)
	{
		layers.Clear();

		std::string jsonString; 
		jsonString.resize(stream.GetSize());
		stream.Read(jsonString.data(), jsonString.size());

		try
		{
			JSON json = JSON::parse(jsonString);
			for (auto& jsonParticleSetBlueprint : json["particleSetBlueprints"])
			{
				String layerName = ConvertString(jsonParticleSetBlueprint["layerName"]);
				String particleSetBlueprintTypeName = ConvertString(jsonParticleSetBlueprint["particleSetBlueprintType"]);

				auto particleSetBlueprintCreatorFunctionIt = particleSetBlueprintCreators.Find(particleSetBlueprintTypeName);
				if (particleSetBlueprintCreatorFunctionIt.IsNull())
				{
					Debug::Logger::LogWarning("SPH Library", "No particle group registered as \"" + particleSetBlueprintTypeName + "\" found");
					continue;
				}

				auto layerIt = layers.Insert(layerName).iterator;
				ParticleSetBlueprint*& particleSetBlueprint = *layerIt->value.AddBack();

				String particleSetBlueprintProperties = ConvertString(jsonParticleSetBlueprint["properties"].dump());
				particleSetBlueprint = particleSetBlueprintCreatorFunctionIt->value();
				particleSetBlueprint->Load(particleSetBlueprintProperties);
			}
			
			auto& particleBehaviourParametersJSON = json["particleBehaviourParameters"];
			systemParameters.particleBehaviourParameters.particleMass = particleBehaviourParametersJSON["particleMass"];
			systemParameters.particleBehaviourParameters.gasConstant = particleBehaviourParametersJSON["gasConstant"];
			systemParameters.particleBehaviourParameters.elasticity = particleBehaviourParametersJSON["elasticity"];
			systemParameters.particleBehaviourParameters.viscosity = particleBehaviourParametersJSON["viscosity"];
			Vec3f gravity = ConvertVec3f(particleBehaviourParametersJSON["gravity"]);			
			systemParameters.particleBehaviourParameters.gravityX = gravity.x;
			systemParameters.particleBehaviourParameters.gravityY = gravity.y;
			systemParameters.particleBehaviourParameters.gravityZ = gravity.z;
			systemParameters.particleBehaviourParameters.restDensity = particleBehaviourParametersJSON["restDensity"];
			systemParameters.particleBehaviourParameters.maxInteractionDistance = particleBehaviourParametersJSON["maxInteractionDistance"];

			auto jsonOtherParameters = json["otherParameters"];
			for (auto jsonOtherParameterIt = jsonOtherParameters.begin(); jsonOtherParameterIt != jsonOtherParameters.end(); ++jsonOtherParameterIt)
				systemParameters.otherParameters.Insert(ConvertString(jsonOtherParameterIt.key()), ConvertString(jsonOtherParameterIt.value()));
		}
		catch (nlohmann::json::parse_error& ex)
		{
			Debug::Logger::LogWarning("Client", "Failed to parse system parameters JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));			

			MarkAsInvalid();			
			return false;
		}

		validScene = true;
		return true;
	}	
	void Scene::InitializeSystem(System& system, ParticleBufferManager& bufferManager)
	{
		if (!validScene)
		{
			Debug::Logger::LogWarning("SPH Library", "Trying to initiaize a system with a invalid scene");
			return;
		}

		auto dynamicParticlesPositions = GenerateLayerParticlePositions("dynamic");
		auto staticParticlesPositions = GenerateLayerParticlePositions("static");

		Array<DynamicParticle> dynamicParticles{ dynamicParticlesPositions.Count() };
		Array<StaticParticle> staticParticles{ staticParticlesPositions.Count() };	

		for (uintMem i = 0; i < dynamicParticles.Count(); ++i)
			dynamicParticles[i].position = dynamicParticlesPositions[i];

		for (uintMem i = 0; i < staticParticles.Count(); ++i)
			staticParticles[i].position = staticParticlesPositions[i];
				
		system.Initialize(systemParameters, bufferManager, std::move(dynamicParticles), std::move(staticParticles));
	}
	void Scene::MarkAsInvalid()
	{
		validScene = false;
		layers.Clear();
		*this = {};
	}
}