#include "pch.h"
#include "SPHSystem.h"

namespace SPH
{
    float SmoothingKernelConstant(float h)
    {
        return 30.0f / (Math::PI * Math::Pow(h, 5));
    }

    float SmoothingKernelD0(float r, float h, float c)
    {
        if (r >= h)
            return 0;

        float distance = h - r;
        return distance * distance * c;
    }

    float SmoothingKernelD1(float r, float h, float c)
    {
        if (r >= h)
            return 0;

        return 2 * (r - h) * c;
    }

    float SmoothingKernelD2(float r, float h, float c)
    {
        if (r >= h)
            return 0;

        return 2 * c;
    }

	Vec3f Clamp(const Vec3f& v, const Vec3f& min, const Vec3f& max)
	{
		return Vec3f(
			std::clamp(v.x, min.x, max.x),
			std::clamp(v.y, min.y, max.y),
			std::clamp(v.z, min.z, max.z)
		);
	}

    Array<Particle> System::GenerateParticles(
		const DynamicParticleGenerationParameters& dynamicParticles,
		const StaticParticleGenerationParameters& staticParticles,
		uintMem& dynamicParticleCount
	) {
		Array<Particle> particleData;

        float linearParticleDensity = Math::Pow(dynamicParticles.particlesPerUnit, 1.0f / 3);
        Vec3<uintMem> dynamicParticleGridSize{ dynamicParticles.spawnVolumeSize * linearParticleDensity };
        Vec3f dynamicParticleDistance = dynamicParticles.spawnVolumeSize / Vec3f(dynamicParticleGridSize - Vec3<uintMem>(1));

		dynamicParticleCount = dynamicParticleGridSize.x * dynamicParticleGridSize.y * dynamicParticleGridSize.z;		        
		
		/*
		Vec3i staticParticleGridSize = Vec3i(staticParticles.boxSize / staticParticles.particleDistance) + Vec3i(1);
		uintMem staticParticleCount = (
			staticParticleGridSize.x * staticParticleGridSize.y +
			staticParticleGridSize.x * staticParticleGridSize.z +
			staticParticleGridSize.y * staticParticleGridSize.z +
			-2 * staticParticleGridSize.x +
			-2 * staticParticleGridSize.y +
			-2 * staticParticleGridSize.z +
			4) * 2;
			*/
		uintMem staticParticleCount = 0;

		particleData.Resize(staticParticleCount + dynamicParticleCount);

        Random::SetSeed(1024);
		Particle* dynamicParticleIt = particleData.Ptr();
        for (int i = 0; i < dynamicParticleGridSize.x; i++) 
            for (int j = 0; j < dynamicParticleGridSize.y; j++) 
                for (int k = 0; k < dynamicParticleGridSize.z; k++) 
				{
                    Vec3f randomOffset = Vec3f(Random::Float(-1, 1), Random::Float(-1, 1), Random::Float(-1, 1)) * dynamicParticleDistance * dynamicParticles.randomOffsetIntensity;
                    					
					dynamicParticleIt->position = 
						Clamp(Vec3f(i, j, k) * dynamicParticleDistance + randomOffset + dynamicParticles.spawnVolumeOffset,
							dynamicParticles.spawnVolumeOffset, dynamicParticles.spawnVolumeOffset + dynamicParticles.spawnVolumeSize);					

					dynamicParticleIt->color = Vec4f(1, 1, 1, 1);

					++dynamicParticleIt;
                }     


		/*
		Particle* staticParticleIt = particleData.Ptr() + dynamicParticleCount;
		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
			for (int y = 1; y < staticParticleGridSize.y - 1; y++)
			{
				(staticParticleIt++)->position = Vec3f(x, y, 0                           ) * staticParticles.particleDistance;
				(staticParticleIt++)->position = Vec3f(x, y, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
			}

		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
			for (int z = 1; z < staticParticleGridSize.z - 1; z++)
			{
				(staticParticleIt++)->position = Vec3f(x, 0,                            z) * staticParticles.particleDistance;
				(staticParticleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1, z) * staticParticles.particleDistance;
			}

		for (int y = 1; y < staticParticleGridSize.y - 1; y++)
			for (int z = 1; z < staticParticleGridSize.z - 1; z++)
			{
				(staticParticleIt++)->position = Vec3f(0,                            y, z) * staticParticles.particleDistance;
				(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y, z) * staticParticles.particleDistance;
			}

		for (int x = 1; x < staticParticleGridSize.x - 1; x++)
		{
			(staticParticleIt++)->position = Vec3f(x,                            0,                            0) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1,                            0) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(x,                            0, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(x, staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
		}		

		for (int y = 1; y < staticParticleGridSize.y - 1; y++)
		{
			(staticParticleIt++)->position = Vec3f(0,                            y,                            0) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y,                            0) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(0,                            y, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, y, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
		}

		for (int z = 1; z < staticParticleGridSize.z - 1; z++)
		{
			(staticParticleIt++)->position = Vec3f(0,                            0,                            z) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0,                            z) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(0,                            staticParticleGridSize.y - 1, z) * staticParticles.particleDistance;
			(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, z) * staticParticles.particleDistance;
		}

		(staticParticleIt++)->position = Vec3f(0,                            0,                            0                           ) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0,                            0                           ) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(0,                            staticParticleGridSize.y - 1, 0                           ) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, 0                           ) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(0,                            0,                            staticParticleGridSize.z - 1) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, 0,                            staticParticleGridSize.z - 1) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(0,                            staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * staticParticles.particleDistance;
		(staticParticleIt++)->position = Vec3f(staticParticleGridSize.x - 1, staticParticleGridSize.y - 1, staticParticleGridSize.z - 1) * staticParticles.particleDistance;

		staticParticleIt = particleData.Ptr() + dynamicParticleCount;

		for (uint i = 0; i < staticParticleCount; ++i)
		{
			staticParticleIt[i].pressure = FLT_MAX;
			staticParticleIt[i].position += staticParticles.boxOffset;
		}		
		*/

		return particleData;
    }

	void LocalSystem::CalculateParticleHashes()
	{		
		hashMap.Resize(particleData.Count() * 2 + 1);		

		for (auto& particle : particleData)
		{
			Vec3i cell = GetCell(particle.position);
			uint32 hash = GetHash(cell);			

			particle.hash = hash;
		}
	}

	void LocalSystem::GenerateHashMap()
	{
		memset(hashMap.Ptr(), 0, sizeof(uint) * hashMap.Count());

		for (uintMem i = 0; i < particleData.Count(); ++i)
			++hashMap[(uintMem)particleData[i].hash];

		uint indexSum = 0;
		for (auto& index : hashMap)
		{
			indexSum += index;
			index = indexSum;
		}

		//0 3 4 5 10 11
		//0 3 4 5 10 11	
		//3 4 5 10 11 11		

		particleMap.Resize(particleData.Count());

		uintMem lastCount = 0;
		for (uintMem i = 0; i < particleData.Count(); ++i)		
			particleMap[i] = --hashMap[particleData[i].hash];			

		//#ifdef SPATIAL_HASHING_DEBUG
		//	for (uintMem i = 0; i < hashMap.Count() - 1; ++i)
		//	{
		//		uintMem begin = hashMap[i];
		//		uintMem end = hashMap[i + 1];
		//
		//		for (uintMem j = begin; j < end; ++j)
		//		{
		//			auto& particle = particleData[j];
		//			Vec3i cell = GetCell(particle.position);
		//			Hash hash = GetHash(cell);
		//
		//			if (hash != i)
		//				Debug::Breakpoint();
		//		}
		//	}
		//#endif
	}

	Vec3i LocalSystem::GetCell(const Vec3f& position) const
	{
		return Vec3i(position / maxInteractionDistance);
	}

	uint32 LocalSystem::GetHash(Vec3i cell) const
	{
		cell.x = (cell.x % 3 + 3) % 3;
		cell.y = (cell.y % 3 + 3) % 3;
		cell.z = (cell.z % 3 + 3) % 3;

		return cell.x + (cell.y + cell.z * 3) * 3 % (hashMap.Count() - 1);
	}
}