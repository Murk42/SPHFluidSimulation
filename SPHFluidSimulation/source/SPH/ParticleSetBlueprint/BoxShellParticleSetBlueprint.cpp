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
	uintMem BoxShellParticleParticleSetBlueprint::GetParticleCount() const
	{
		Vec3<uintMem> particleGridSize = Vec3<uintMem>(properties.spawnVolumeSize / properties.particleDistance) + Vec3<uintMem>(1);
		return (
			particleGridSize.x * particleGridSize.y +
			particleGridSize.x * particleGridSize.z +
			particleGridSize.y * particleGridSize.z +
			-2 * particleGridSize.x +
			-2 * particleGridSize.y +
			-2 * particleGridSize.z +
			4) * 2;
	}
	void BoxShellParticleParticleSetBlueprint::WriteParticlesPositions(const WriteFunction<Vec3f> writeFunction, void* userData) const
	{
		Vec3<uintMem> particleGridSize = Vec3<uintMem>(properties.spawnVolumeSize / properties.particleDistance) + Vec3<uintMem>(1);
		uintMem particleCount = (
			particleGridSize.x * particleGridSize.y +
			particleGridSize.x * particleGridSize.z +
			particleGridSize.y * particleGridSize.z +
			-2 * particleGridSize.x +
			-2 * particleGridSize.y +
			-2 * particleGridSize.z +
			4) * 2;

		Random::SetSeed(properties.seed);

		uintMem index = 0;
		auto writeWithRandomOffset = [&](const Vec3f& pos) {
			Vec3f randomOffset = Vec3f(Random::Float(), Random::Float(), Random::Float()) * properties.randomOffsetIntensity * properties.particleDistance;
			writeFunction(index, pos + randomOffset + properties.spawnVolumeOffset, userData);
			++index;
			};

		for (int x = 1; x < particleGridSize.x - 1; x++)
			for (int y = 1; y < particleGridSize.y - 1; y++)
			{				
				writeWithRandomOffset(Vec3f((float)x, (float)y, 0.0f) * properties.particleDistance);
				writeWithRandomOffset(Vec3f((float)x, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			}

		for (int x = 1; x < particleGridSize.x - 1; x++)
			for (int z = 1; z < particleGridSize.z - 1; z++)
			{
				writeWithRandomOffset(Vec3f((float)x, 0.0f, (float)z) * properties.particleDistance);
				writeWithRandomOffset(Vec3f((float)x, (float)particleGridSize.y - 1.0f, (float)z) * properties.particleDistance);
			}

		for (int y = 1; y < particleGridSize.y - 1; y++)
			for (int z = 1; z < particleGridSize.z - 1; z++)
			{
				writeWithRandomOffset(Vec3f(0.0f, (float)y, (float)z) * properties.particleDistance);
				writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)y, (float)z) * properties.particleDistance);
			}

		for (int x = 1; x < particleGridSize.x - 1; x++)
		{
			writeWithRandomOffset(Vec3f((float)x, 0.0f, 0.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)x, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)x, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)x, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		}

		for (int y = 1; y < particleGridSize.y - 1; y++)
		{
			writeWithRandomOffset(Vec3f(0.0f, (float)y, 0.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)y, 0.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f(0.0f, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)y, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		}

		for (int z = 1; z < particleGridSize.z - 1; z++)
		{
			writeWithRandomOffset(Vec3f(0.0f, 0.0f, z) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, z) * properties.particleDistance);
			writeWithRandomOffset(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, z) * properties.particleDistance);
			writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, z) * properties.particleDistance);
		}

		writeWithRandomOffset(Vec3f(0.0f, 0.0f, 0.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, 0.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, 0.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f(0.0f, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, 0.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f(0.0f, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
		writeWithRandomOffset(Vec3f((float)particleGridSize.x - 1.0f, (float)particleGridSize.y - 1.0f, (float)particleGridSize.z - 1.0f) * properties.particleDistance);
	}    
}