#pragma once
#include "ParticleGenerator.h"

namespace SPH
{
	struct FilledBoxParticleParameters
	{
		Vec3f spawnVolumeSize;
		Vec3f spawnVolumeOffset;
		float particlesPerUnit;
		float randomOffsetIntensity;
	};
	template<PositionedParticle T>
	class FilledBoxParticleGenerator : public ParticleGenerator<T>
	{
	public:
		FilledBoxParticleGenerator(const FilledBoxParticleParameters& parameters);

		void Generate(Array<T>& particles) override;
	private:
		FilledBoxParticleParameters parameters;

		Vec3f Clamp(const Vec3f& v, const Vec3f& min, const Vec3f& max) const;
	};

	template<PositionedParticle T>
	inline FilledBoxParticleGenerator<T>::FilledBoxParticleGenerator(const FilledBoxParticleParameters& parameters) :
		parameters(parameters)
	{

	}

	template<PositionedParticle T>
	inline void FilledBoxParticleGenerator<T>::Generate(Array<T>& particles)
	{
		float linearParticleDensity = Math::Pow(parameters.particlesPerUnit, 1.0f / 3);
		Vec3<uintMem> dynamicParticleGridSize{ parameters.spawnVolumeSize * linearParticleDensity };
		Vec3f dynamicParticleDistance = parameters.spawnVolumeSize / Vec3f(dynamicParticleGridSize - Vec3<uintMem>(1));

		if (dynamicParticleGridSize.x == 1) dynamicParticleDistance.x = 0.0f;
		if (dynamicParticleGridSize.y == 1) dynamicParticleDistance.y = 0.0f;
		if (dynamicParticleGridSize.z == 1) dynamicParticleDistance.z = 0.0f;

		uintMem staticParticleCount = 0;

		particles.Resize(dynamicParticleGridSize.x * dynamicParticleGridSize.y * dynamicParticleGridSize.z);

		Random::SetSeed(1024);
		T* particleIt = particles.Ptr();
		for (int i = 0; i < dynamicParticleGridSize.x; i++)
			for (int j = 0; j < dynamicParticleGridSize.y; j++)
				for (int k = 0; k < dynamicParticleGridSize.z; k++)
				{
					Vec3f randomOffset = Vec3f(Random::Float(-1, 1), Random::Float(-1, 1), Random::Float(-1, 1)) * dynamicParticleDistance * parameters.randomOffsetIntensity;

					particleIt->position =
						Clamp(Vec3f(i, j, k) * dynamicParticleDistance + randomOffset + parameters.spawnVolumeOffset,
							parameters.spawnVolumeOffset, parameters.spawnVolumeOffset + parameters.spawnVolumeSize);

					++particleIt;
				}
	}

	template<PositionedParticle T>
	inline Vec3f FilledBoxParticleGenerator<T>::Clamp(const Vec3f& v, const Vec3f& min, const Vec3f& max) const
	{
		return Vec3f(
			std::clamp(v.x, min.x, max.x),
			std::clamp(v.y, min.y, max.y),
			std::clamp(v.z, min.z, max.z)
		);
	}
}