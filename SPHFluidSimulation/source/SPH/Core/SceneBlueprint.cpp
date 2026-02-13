#include "pch.h"
#include "SPH/Core/SceneBlueprint.h"
#include "JSONParsing.h"
#include "SPH/ParticleSetBlueprints/BoxShellParticleSetBlueprint.h"
#include "SPH/ParticleSetBlueprints/FilledBoxParticleSetBlueprint.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace SPH
{
	static Map<String, std::function<ParticleSetBlueprint* ()>> particleSetBlueprintCreators = { {
		{ "BoxShell", []() { return new BoxShellParticleParticleSetBlueprint(); }},
		{ "FilledBox", []() { return new FilledBoxParticleSetBlueprint(); } }
	} };

	static void GetJSONParticleBehaviourParameters(const nlohmann::json& json, ParticleBehaviourParameters& parameters)
	{
		try {
			parameters.particleMass = JSON::Expect<float>(json, "particleMass");
			parameters.gasConstant = JSON::Expect<float>(json, "gasConstant");
			parameters.elasticity = JSON::Expect<float>(json, "elasticity");
			parameters.viscosity = JSON::Expect<float>(json, "viscosity");
			Vec3f gravity = JSON::Expect<Vec3f>(json, "gravity");
			parameters.gravityX = gravity.x;
			parameters.gravityY = gravity.y;
			parameters.gravityZ = gravity.z;
			parameters.restDensity = JSON::Expect<float>(json, "restDensity");
			parameters.maxInteractionDistance = JSON::Expect<float>(json, "maxInteractionDistance");
		}
		catch (...)
		{
			throw;
		}
	}
	static void GetJSONParticleSetBlueprints(const nlohmann::json& json, Map<String, Array<ParticleSetBlueprint*>>& layers)
	{
		try
		{
			for (auto& jsonParticleSetBlueprint : json)
			{
				String layerName = JSON::Expect<String>(jsonParticleSetBlueprint, "layerName");
				String particleSetBlueprintTypeName = JSON::Expect<String>(jsonParticleSetBlueprint, "particleSetBlueprintType");

				auto particleSetBlueprintCreatorFunctionIt = particleSetBlueprintCreators.Find(particleSetBlueprintTypeName);
				if (particleSetBlueprintCreatorFunctionIt.IsNull())
				{
					Debug::Logger::LogWarning("SPH Library", "No particle group registered as \"" + particleSetBlueprintTypeName + "\" found");
					continue;
				}

				auto layerIt = layers.Insert(layerName).iterator;
				ParticleSetBlueprint*& particleSetBlueprint = *layerIt->value.AddBack();

				String particleSetBlueprintProperties = JSON::AsString(JSON::Expect<const nlohmann::json&>(jsonParticleSetBlueprint, "properties"));
				particleSetBlueprint = particleSetBlueprintCreatorFunctionIt->value();
				particleSetBlueprint->Load(particleSetBlueprintProperties);
			}
		}
		catch (...)
		{
			throw;
		}
	}
	static void GetJSONIndexedTriangleMesh(const nlohmann::json& json, Graphics::BasicIndexedMesh& mesh)
	{
		try
		{
			Path path = Path(JSON::Expect<String>(json, "path"));

			if (!path.Exists())
			{
				Debug::Logger::LogError("SPH Library", Format("Path to boundary mesh is not a valid path: \"{}\"", path));
				return;
			}

			if (!path.IsFile())
			{
				Debug::Logger::LogError("SPH Library", Format("Path to boundary mesh is not a path to a file: \"{}\"", path));
				return;
			}

			String objectName;

			if (JSON::HasEntry(json, "objectName"))
				objectName = JSON::Expect<String>(json, "objectName");

			mesh.Load(path, objectName);
		}
		catch (...)
		{
			throw;
		}
	}
	static void GetJSONOtherParameters(const nlohmann::json& json, Blaze::Map<String, String>& map)
	{
		try
		{
			for (auto jsonOtherParameterIt = json.begin(); jsonOtherParameterIt != json.end(); ++jsonOtherParameterIt)
			{
				std::string key = jsonOtherParameterIt.key();
				map.Insert(StringView(key.data(), key.size()), JSON::Expect<String>(jsonOtherParameterIt.value()));
			}
		}
		catch(...)
		{
			throw;
		}
	}

	SceneBlueprint::SceneBlueprint()
		: validScene(false)
	{
		return;
	}
	SceneBlueprint::~SceneBlueprint()
	{
		for (auto& layer : layers)
			for (auto& particleSetBlueprint : layer.value)
				delete particleSetBlueprint;
	}
	Array<Vec3f> SceneBlueprint::GenerateLayerParticlePositions(StringView layerName)
	{
		auto it = layers.Find(layerName);

		if (it.IsNull())
			return { };

		Array<Vec3f> particles;

		for (auto& particleSetBlueprint : it->value)
			particleSetBlueprint->WriteParticlesPositions([](uintMem index, const Vec3f& ptr, void* userData) { ((Vec3f*)userData)[index] = ptr; }, particles.Ptr());

		return particles;
	}
	bool SceneBlueprint::LoadScene(const Path& path)
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
	bool SceneBlueprint::LoadScene(ReadSubStream& stream)
	{
		layers.Clear();

		try
		{
			auto json = JSON::FromStream(stream);

			GetJSONParticleBehaviourParameters(JSON::Expect<const nlohmann::json&>(json, "particleBehaviourParameters"), systemParameters.particleBehaviourParameters);

			if (JSON::HasEntry(json, "particleSetBlueprints"))
				GetJSONParticleSetBlueprints(json["particleSetBlueprints"], layers);

			if (JSON::HasEntry(json, "boundaryMesh"))
				GetJSONIndexedTriangleMesh(json["boundaryMesh"], mesh);

			if (JSON::HasEntry(json, "otherParameters"))
				GetJSONOtherParameters(json["otherParameters"], systemParameters.otherParameters);
		}
		catch (nlohmann::json::parse_error& ex)
		{
			Debug::Logger::LogWarning("Client", "Failed to parse scene blueprint JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));

			MarkAsInvalid();
			return false;
		}
		catch (...)
		{
			Debug::Logger::LogWarning("Client", "Failed to parse scene blueprint JSON");
			MarkAsInvalid();
			return false;
		}


		validScene = true;
		return true;
	}
	void SceneBlueprint::MarkAsInvalid()
	{
		*this = {};
	}
}