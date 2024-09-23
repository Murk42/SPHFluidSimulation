#pragma once
#include "SPH/ParticleGenerator/ParticleGenerator.h"

namespace SPH
{
	struct DynamicParticle
	{
		Vec3f position;
		float pressure;
		Vec3f velocity;
		uint32 hash;	

		Vec4f color;
	};
	struct StaticParticle
	{
		Vec3f position;
		float pressure;

		Vec4f color;
	};

	struct DynamicParticleGenerationParameters
	{
		std::shared_ptr<ParticleGenerator<DynamicParticle>> generator;		
	};

	struct StaticParticleGenerationParameters
	{
		std::shared_ptr<ParticleGenerator<StaticParticle>> generator;
	};
	
	struct ParticleBehaviourParameters
	{
		//Particle dynamics constants
		float particleMass;
		float gasConstant;
		float elasticity;
		float viscosity;
		Vec3f gravity;

		//Particle simulation parameters		
		float restDensity;
		float maxInteractionDistance;		
	};

	struct ParticleBoundParameters
	{
		float wallElasticity;
		float floorAndRoofElasticity;
		bool bounded;
		bool boundedByRoof;
		bool boundedByWalls;
		Vec3f boxOffset;
		Vec3f boxSize;
	};

	struct SystemInitParameters
	{
		DynamicParticleGenerationParameters dynamicParticleGenerationParameters;
		StaticParticleGenerationParameters staticParticleGenerationParameters;
		ParticleBehaviourParameters particleBehaviourParameters;
		ParticleBoundParameters particleBoundParameters;

		//Graphics
		uintMem bufferCount;
		uint hashesPerParticle;
		uint hashesPerStaticParticle;
	};

	class System
	{
	public:
		virtual ~System() { }

		virtual void Initialize(const SystemInitParameters&) = 0;		

		virtual void Update(float dt) = 0;

		virtual StringView SystemImplementationName() = 0;			
		
		virtual uintMem GetDynamicParticleCount() const = 0;
		virtual uintMem GetStaticParticleCount() const = 0;

		virtual Array<DynamicParticle> GetParticles() { return { }; }
		virtual Array<uintMem> FindNeighbors(Array<DynamicParticle>& particles, Vec3f position) { return {}; }
	protected:		
		static uint32 GetHash(Vec3i cell);

		template<typename T, typename F> requires std::invocable<F, const T&>
		static void GenerateHashMap(Array<T>& particles, Array<uint32>& hashMap, const F& hashGetter);
	};	
	
	template<typename T, typename F> requires std::invocable<F, const T&>
	inline void System::GenerateHashMap(Array<T>& particles, Array<uint32>& hashMap, const F& hashGetter)
	{
		memset(hashMap.Ptr(), 0, sizeof(uint) * hashMap.Count());

		for (uintMem i = 0; i < particles.Count(); ++i)
			++hashMap[hashGetter(particles[i])];

		uint indexSum = 0;
		for (auto& index : hashMap)
		{
			indexSum += index;
			index = indexSum;
		}
	}
}