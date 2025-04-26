#pragma once
#include "SPH/Core/ParticleSetBlueprint.h"

namespace SPH
{
	struct BoxShellParticleParticleSetBlueprintProperties
	{
		Vec3f spawnVolumeSize;
		Vec3f spawnVolumeOffset;
		float particleDistance;
		float randomOffsetIntensity;
		uint32 seed;
	};	
	class BoxShellParticleParticleSetBlueprint : public ParticleSetBlueprint
	{
	public:
		BoxShellParticleParticleSetBlueprint();

		void SetProperties(const BoxShellParticleParticleSetBlueprintProperties&);

		void Load(StringView string) override;

		void AppendParticlePositions(Array<Vec3f>& positions) override;
	private:
		BoxShellParticleParticleSetBlueprintProperties properties;
	};	
}