#include "pch.h"
#include "SPH/ParticleSetBlueprint/FilledBoxParticleSetBlueprint.h"
#include "JSONParsing.h"

namespace SPH
{	
	static Vec3f ClampVec3f(const Vec3f& v, const Vec3f& min, const Vec3f& max)
	{
		return Vec3f(
			std::clamp(v.x, min.x, max.x),
			std::clamp(v.y, min.y, max.y),
			std::clamp(v.z, min.z, max.z)
		);
	}

	FilledBoxParticleSetBlueprint::FilledBoxParticleSetBlueprint()
	{
	}
	void FilledBoxParticleSetBlueprint::SetProperties(const FilledBoxParticleSetBlueprintProperties& properties)
	{
		this->properties = properties;
	}	
	void FilledBoxParticleSetBlueprint::Load(StringView string)
	{		
		try {
			JSON json = JSON::parse(std::string_view(string.Ptr(), string.Count()));

			properties.particlesPerUnit = json["particlesPerUnit"];
			properties.randomOffsetIntensity = json["randomOffsetIntensity"];
			properties.spawnVolumeOffset = ConvertVec3f(json["spawnVolumeOffset"]);
			properties.spawnVolumeSize = ConvertVec3f(json["spawnVolumeSize"]);
			if (json.contains("seed"))
				properties.seed = json["seed"];
			else
				properties.seed = 0;
		}	
		catch (nlohmann::json::parse_error& ex)
		{
			Debug::Logger::LogWarning("Client", "Failed to parse filled box particle set blueprint parameters JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			properties = FilledBoxParticleSetBlueprintProperties();
		}
	}
	void FilledBoxParticleSetBlueprint::AppendParticlePositions(Array<Vec3f>& positions)
	{
		float linearParticleDensity = Math::Pow(properties.particlesPerUnit, 1.0f / 3);
		Vec3<uintMem> dynamicParticleGridSize{ properties.spawnVolumeSize * linearParticleDensity };
		Vec3f dynamicParticleDistance = properties.spawnVolumeSize / Vec3f(dynamicParticleGridSize - Vec3<uintMem>(1));

		if (dynamicParticleGridSize.x == 1) dynamicParticleDistance.x = 0.0f;
		if (dynamicParticleGridSize.y == 1) dynamicParticleDistance.y = 0.0f;
		if (dynamicParticleGridSize.z == 1) dynamicParticleDistance.z = 0.0f;

		uintMem staticParticleCount = 0;

		positions.ReserveAdditional(dynamicParticleGridSize.x * dynamicParticleGridSize.y * dynamicParticleGridSize.z);

		Random::SetSeed(properties.seed);		
		for (int i = 0; i < dynamicParticleGridSize.x; i++)
			for (int j = 0; j < dynamicParticleGridSize.y; j++)
				for (int k = 0; k < dynamicParticleGridSize.z; k++)
				{
					Vec3f randomOffset = Vec3f(Random::Float(-1, 1), Random::Float(-1, 1), Random::Float(-1, 1)) * dynamicParticleDistance * properties.randomOffsetIntensity;

					Vec3f position = ClampVec3f(Vec3f(i, j, k) * dynamicParticleDistance + randomOffset + properties.spawnVolumeOffset,
						properties.spawnVolumeOffset, properties.spawnVolumeOffset + properties.spawnVolumeSize);

					positions.AddBack(position);
				}
	}
}