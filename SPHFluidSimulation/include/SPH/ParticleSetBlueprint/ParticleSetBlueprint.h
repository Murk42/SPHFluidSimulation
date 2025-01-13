#pragma once

namespace SPH
{
	class ParticleSetBlueprint
	{
	public:
		virtual ~ParticleSetBlueprint() { }
		virtual void Load(StringView string) = 0;
		virtual void AppendParticlePositions(Array<Vec3f>& particles) = 0;
	private:
	};

	class ParticleSetBlueprintVelocityOverride : public ParticleSetBlueprint
	{
	public:
		virtual void AppendParticleVelocities(Array<Vec3f>& particles) = 0;
	};
}