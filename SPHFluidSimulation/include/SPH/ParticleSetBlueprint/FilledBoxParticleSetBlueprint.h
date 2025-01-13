#pragma once
#include "ParticleSetBlueprint.h"

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

		void AppendParticlePositions(Array<Vec3f>& positions) override;		
	private:
		FilledBoxParticleSetBlueprintProperties properties;		
	};
}