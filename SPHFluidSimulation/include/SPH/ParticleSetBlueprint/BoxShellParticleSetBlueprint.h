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

		uintMem GetParticleCount() const override;
		void WriteParticlesPositions(const WriteFunction<Vec3f> writeFunction, void* userData) const override;		
	private:
		BoxShellParticleParticleSetBlueprintProperties properties;
	};	
}