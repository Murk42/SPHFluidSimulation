#pragma once
#include "ParticleGenerator.h"

namespace SPH
{
	struct BoxShellParticleParameters
	{
		Vec3f spawnVolumeSize;
		Vec3f spawnVolumeOffset;
		float particleDistance;
		float randomOffsetIntensity;
	};
	template<PositionedParticle T>
	class BoxShellParticleGenerator : public ParticleGenerator<T>
	{
	public:
		BoxShellParticleGenerator(const BoxShellParticleParameters& parameters);

		void Generate(Array<T>& particles) override;
	private:
		BoxShellParticleParameters parameters;
	};

	template<PositionedParticle T>
	inline BoxShellParticleGenerator<T>::BoxShellParticleGenerator(const BoxShellParticleParameters& parameters) :
		parameters(parameters)
	{

	}

	template<PositionedParticle T>
	inline void BoxShellParticleGenerator<T>::Generate(Array<T>& particles)
	{
		Vec3i staticParticleGridSize = Vec3i(parameters.spawnVolumeSize / parameters.particleDistance) + Vec3i(1);
		uintMem staticParticleCount = (
			staticParticleGridSize.x * staticParticleGridSize.y +
			staticParticleGridSize.x * staticParticleGridSize.z +
			staticParticleGridSize.y * staticParticleGridSize.z +
			-2 * staticParticleGridSize.x +
			-2 * staticParticleGridSize.y +
			-2 * staticParticleGridSize.z +
			4) * 2;

		particles.Resize(staticParticleCount);

		T* particleIt = particles.Ptr();
		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
			for (int y = 1; y < staticParticleGridSize.y - 1; y++)
			{
				(particleIt++)->position = Vec3f(x, y, 0) * parameters.particleDistance;
				(particleIt++)->position = Vec3f(x, y, staticParticleGridSize.z - 1) * parameters.particleDistance;
			}

		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
			for (int z = 1; z < staticParticleGridSize.z - 1; z++)
			{
				(particleIt++)->position = Vec3f(x, 0, z) * parameters.particleDistance;
				(particleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1, z) * parameters.particleDistance;
			}

		for (int y = 1; y < staticParticleGridSize.y - 1; y++)
			for (int z = 1; z < staticParticleGridSize.z - 1; z++)
			{
				(particleIt++)->position = Vec3f(0, y, z) * parameters.particleDistance;
				(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y, z) * parameters.particleDistance;
			}

		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
		{
			(particleIt++)->position = Vec3f(x, 0, 0) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1, 0) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(x, 0, staticParticleGridSize.z - 1) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * parameters.particleDistance;
		}

		for (int y = 1; y < staticParticleGridSize.y - 1; y++)
		{
			(particleIt++)->position = Vec3f(0, y, 0) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y, 0) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(0, y, staticParticleGridSize.z - 1) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y, staticParticleGridSize.z - 1) * parameters.particleDistance;
		}

		for (int z = 1; z < staticParticleGridSize.z - 1; z++)
		{
			(particleIt++)->position = Vec3f(0, 0, z) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0, z) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(0, staticParticleGridSize.y - 1, z) * parameters.particleDistance;
			(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, z) * parameters.particleDistance;
		}

		(particleIt++)->position = Vec3f(0, 0, 0) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0, 0) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(0, staticParticleGridSize.y - 1, 0) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, 0) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(0, 0, staticParticleGridSize.z - 1) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0, staticParticleGridSize.z - 1) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(0, staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * parameters.particleDistance;
		(particleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * parameters.particleDistance;

		for (uint i = 0; i < staticParticleCount; ++i)
			particles[i].position += parameters.spawnVolumeOffset + Vec3f(Random::Float(), Random::Float(), Random::Float()) * parameters.randomOffsetIntensity;
	}
}