#include "pch.h"
#include "SPH/ParticleSetBlueprints/BoxShellParticleSetBlueprint.h"
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
		properties = BoxShellParticleParticleSetBlueprintProperties();

		auto json = JSON::FromString(string);			

		properties.particleDistance = JSON::Expect<float>(json, "particleDistance");
		properties.randomOffsetIntensity = JSON::Expect<float>(json, "randomOffsetIntensity");
		properties.spawnVolumeOffset = JSON::Expect<Vec3f>(json, "spawnVolumeOffset");
		properties.spawnVolumeSize = JSON::Expect<Vec3f>(json, "spawnVolumeSize");

		if (JSON::HasEntry(json, "seed"))			
			properties.seed = JSON::Expect<uint32>(json, "seed");
		else
			properties.seed = 0;
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