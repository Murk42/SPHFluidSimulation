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
	uintMem FilledBoxParticleSetBlueprint::GetParticleCount() const
	{		
		const Vec3<uintMem> gridSize{ properties.spawnVolumeSize * Math::Pow(properties.particlesPerUnit, 1.0f / 3) };
		return gridSize.x * gridSize.y * gridSize.z;
	}
	void FilledBoxParticleSetBlueprint::WriteParticlesPositions(const WriteFunction<Vec3f> writeFunction, void* userData) const
	{
		const float linearParticleDensity = Math::Pow(properties.particlesPerUnit, 1.0f / 3);
		const float particleDistance = 1.0f / linearParticleDensity;
		const Vec3<uintMem> gridSize{ properties.spawnVolumeSize * linearParticleDensity };

		const Vec3f particleOffset = Vec3f(particleDistance / 2) + properties.spawnVolumeOffset;		
		
		Random::SetSeed(properties.seed);
		const float offsetAmplitude = particleDistance / 2.0f * properties.randomOffsetIntensity;

		uintMem index = 0;
		for (int i = 0; i < gridSize.x; i++)
			for (int j = 0; j < gridSize.y; j++)
				for (int k = 0; k < gridSize.z; k++)
				{
					Vec3f position = Vec3f(i, j, k) * particleDistance + particleOffset;

					if (properties.randomOffsetIntensity != 0.0f)
						position += Vec3f(Random::Float(-1, 1), Random::Float(-1, 1), Random::Float(-1, 1))* offsetAmplitude;

					writeFunction(index, position, userData);
					++index;
				}
	}
}