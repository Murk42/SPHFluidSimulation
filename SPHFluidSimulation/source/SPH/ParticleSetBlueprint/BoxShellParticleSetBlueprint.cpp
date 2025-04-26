#include "pch.h"
#include "SPH/ParticleSetBlueprint/BoxShellParticleSetBlueprint.h"
#include "JSONParsing.h"

namespace SPH
{
    BoxShellParticleParticleSetBlueprint::BoxShellParticleParticleSetBlueprint()
    {
    }
    void BoxShellParticleParticleSetBlueprint::SetProperties(const BoxShellParticleParticleSetBlueprintProperties& properties)
    {
		this->properties = properties;
    }
    void BoxShellParticleParticleSetBlueprint::Load(StringView string)
    {
		try {
			JSON json = JSON::parse(std::string_view(string.Ptr(), string.Count()));

			properties.particleDistance = json["particleDistance"];
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
			Debug::Logger::LogWarning("Client", "Failed to parse box shell particle set blueprint parameters JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			properties = BoxShellParticleParticleSetBlueprintProperties();
		}
    }
    void BoxShellParticleParticleSetBlueprint::AppendParticlePositions(Array<Vec3f>& positions)
    {
		Vec3i particleGridSize = Vec3i(properties.spawnVolumeSize / properties.particleDistance) + Vec3i(1);
		uintMem particleCount = (
			particleGridSize.x * particleGridSize.y +
			particleGridSize.x * particleGridSize.z +
			particleGridSize.y * particleGridSize.z +
			-2 * particleGridSize.x +
			-2 * particleGridSize.y +
			-2 * particleGridSize.z +
			4) * 2;

		positions.ReserveAdditional(particleCount);
		
		for (int x = 1; x < particleGridSize.x - 1; x++)
			for (int y = 1; y < particleGridSize.y - 1; y++)
			{
				positions.AddBack(Vec3f((float)x, (float)y, 0.0f) * properties.particleDistance);
				positions.AddBack(Vec3f((float)x, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			}

		for (int x = 1; x < particleGridSize.x - 1; x++)
			for (int z = 1; z < particleGridSize.z - 1; z++)
			{
				positions.AddBack(Vec3f((float)x, 0.0f, (float)z) * properties.particleDistance);
				positions.AddBack(Vec3f((float)x, (float)particleGridSize.y - 1.0f, (float)z) * properties.particleDistance);
			}

		for (int y = 1; y < particleGridSize.y - 1; y++)
			for (int z = 1; z < particleGridSize.z - 1; z++)
			{
				positions.AddBack(Vec3f(0.0f, (float)y, (float)z) * properties.particleDistance);
				positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)y, (float)z) * properties.particleDistance);
			}

		for (int x = 1; x < particleGridSize.x - 1; x++)
		{
			positions.AddBack(Vec3f((float)x, 0.0f, 0.0f) * properties.particleDistance);
			positions.AddBack(Vec3f((float)x, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
			positions.AddBack(Vec3f((float)x, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			positions.AddBack(Vec3f((float)x, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		}

		for (int y = 1; y < particleGridSize.y - 1; y++)
		{
			positions.AddBack(Vec3f(0.0f, (float)y, 0.0f) * properties.particleDistance);
			positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)y, 0.0f) * properties.particleDistance);
			positions.AddBack(Vec3f(0.0f, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		}

		for (int z = 1; z < particleGridSize.z - 1; z++)
		{
			positions.AddBack(Vec3f(0.0f, 0.0f, z) * properties.particleDistance);
			positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, z) * properties.particleDistance);
			positions.AddBack(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, z) * properties.particleDistance);
			positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, z) * properties.particleDistance);
		}

		positions.AddBack(Vec3f(0.0f, 0.0f, 0.0f) * properties.particleDistance);
		positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, 0.0f) * properties.particleDistance);
		positions.AddBack(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
		positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
		positions.AddBack(Vec3f(0.0f, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		positions.AddBack(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		positions.AddBack(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);

		Random::SetSeed(properties.seed);
		for (uintMem i = positions.Count() - particleCount; i < positions.Count(); ++i)
			positions[i] += properties.spawnVolumeOffset + Vec3f(Random::Float(), Random::Float(), Random::Float()) * properties.randomOffsetIntensity * properties.particleDistance;
    }
}