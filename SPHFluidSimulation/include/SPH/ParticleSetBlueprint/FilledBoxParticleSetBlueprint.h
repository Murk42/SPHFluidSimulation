#pragma once
#include "SPH/Core/ParticleSetBlueprint.h"

namespace SPH
{
	struct FilledBoxParticleSetBlueprintProperties
	{
		Vec3f spawnVolumeSize;
		Vec3f spawnVolumeOffset;
		float particlesPerUnit;
		float randomOffsetIntensity;
		uint32 seed;
	};	
	class FilledBoxParticleSetBlueprint : public ParticleSetBlueprint
	{
	public:
		FilledBoxParticleSetBlueprint();

		void SetProperties(const FilledBoxParticleSetBlueprintProperties&);

		void Load(StringView string) override;

		uintMem GetParticleCount() const override;
		void WriteParticlesPositions(const WriteFunction<Vec3f> writeFunction, void* userData) const override;
	private:
		FilledBoxParticleSetBlueprintProperties properties;		
	};
}