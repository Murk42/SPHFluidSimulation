#pragma once

using namespace Blaze;

namespace SPH
{
	struct Particle
	{
		Vec3f position;
		float pressure;
		Vec3f velocity;
		uint32 hash;
		Vec4f color;
	};

	struct DynamicParticleGenerationParameters
	{
		Vec3f spawnVolumeOffset;
		Vec3f spawnVolumeSize;

		//# of particles per cubic meter
		float particlesPerUnit;		

		float randomOffsetIntensity;
	};

	struct StaticParticleGenerationParameters
	{
		Vec3f boxSize;
		Vec3f boxOffset;
		float particleDistance;		
	};

	struct NeighborData
	{
		Particle* particle;
		float sqrDistance;
	};

	struct SystemInitParameters
	{
		DynamicParticleGenerationParameters dynamicParticleGenerationParameters;
		StaticParticleGenerationParameters staticParticleGenerationParameters;
			
		//Particle behaviour constants
		float particleMass;
		float gasConstant;
		float elasticity;
		float viscosity;

		//Particle simulation parameters
		Vec3f boundingBoxSize;
		float restDensity;
		float maxInteractionDistance;

		//Graphics
		uintMem bufferCount;
	};

    float SmoothingKernelConstant(float h);
    float SmoothingKernelD0(float r, float h, float c);
    float SmoothingKernelD1(float r, float h, float c);
    float SmoothingKernelD2(float r, float h, float c);

	class System
	{
	public:
		virtual ~System() { }

		virtual void Initialize(const SystemInitParameters&) = 0;

		virtual void Update(float dt) = 0;

		virtual StringView SystemImplementationName() = 0;			
		
		virtual uintMem GetParticleCount() const = 0;

		virtual Array<Particle> GetParticles() { return { }; }
		virtual Array<uintMem> FindNeighbors(Array<Particle>& particles, Vec3f position) { return {}; }
	protected:
		Array<Particle> GenerateParticles(
			const DynamicParticleGenerationParameters& dynamicParticles,
			const StaticParticleGenerationParameters& staticParticles,
			uintMem& dynamicParticleCount
		);
	};

	class LocalSystem : public System
	{
	public:
		//inline Array<Particle>& GetParticles() { return particleData; }
		inline uintMem GetDynamicParticleCount() { return dynamicParticleCount; }
		inline uintMem GetStaticParticleCount() { return staticParticleCount; }
	protected:
		Array<Particle> particleData;
		Array<uint> hashMap;
		Array<uint> particleMap;
		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
		float maxInteractionDistance;

		void CalculateParticleHashes();
		void GenerateHashMap();

		Vec3i GetCell(const Vec3f&) const;
		uint32 GetHash(Vec3i cell) const;

		template<std::invocable<NeighborData&> T>
		void ForEachNeighbor(Vec3f position, const T& body);
	};

	template<std::invocable<NeighborData&> T>
	inline void LocalSystem::ForEachNeighbor(Vec3f position, const T& body)
	{
		Vec3i cell = GetCell(position);

		float radiusSqr = maxInteractionDistance * maxInteractionDistance;

		Vec3i beginCell = cell - Vec3i(1);
		Vec3i endCell = cell + Vec3i(2);

#ifdef SPATIAL_HASHING_DEBUG
		Set<Particle*> p;
#endif
		Vec3i otherCell;
		for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
			for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
				for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
				{
					uint32 otherHash = GetHash(otherCell);

					uint beginIndex = hashMap[otherHash];
					uint endIndex = hashMap[otherHash + 1];

					for (uint i = beginIndex; i < endIndex; ++i)
					{
						Particle& otherParticle = particleData[i];

#ifdef SPATIAL_HASHING_DEBUG
						if (!p.Find(&otherParticle).IsNull())
							Debug::Breakpoint();

						p.Insert(&otherParticle);
#endif


						float sqrDistance = (position - otherParticle.position).SqrLenght();

						if (sqrDistance < radiusSqr)
							body(NeighborData{
								.particle = &otherParticle,
								.sqrDistance = sqrDistance
								});
					}
				}
	}
}