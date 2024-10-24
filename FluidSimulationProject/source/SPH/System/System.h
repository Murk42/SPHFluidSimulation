#pragma once
#include "BlazeEngineCore/BlazeEngineCore.h"
#include "SPH/ParticleGenerator/ParticleGenerator.h"
#include "SPH/ParticleBufferSet/ParticleBufferSet.h"
#include "SPH/SPHFunctions.h"

namespace SPH
{
	struct DynamicParticleGenerationParameters
	{
		std::shared_ptr<ParticleGenerator<DynamicParticle>> generator;		
	};

	struct StaticParticleGenerationParameters
	{
		std::shared_ptr<ParticleGenerator<StaticParticle>> generator;
	};	
	

	struct SystemInitParameters
	{
		template<typename T>
		struct ImplementationSpecificsEntry
		{
			String name;
			T& value;
		};

		DynamicParticleGenerationParameters dynamicParticleGenerationParameters;
		StaticParticleGenerationParameters staticParticleGenerationParameters;
		ParticleBehaviourParameters particleBehaviourParameters;
		ParticleBoundParameters particleBoundParameters;

		//Graphics
		uintMem bufferCount;
		float hashesPerDynamicParticle;
		float hashesPerStaticParticle;

		Map<String, VirtualMap<String>> implementationSpecifics;

		void ParseJSON(const JSON& json);		
		template<typename ... Args>
		void ParseImplementationSpecifics(StringView implementationName, ImplementationSpecificsEntry<Args>&& ... args) const
		{
			auto it = implementationSpecifics.Find(implementationName);
			if (it.IsNull())
				return;			

			(ParseImplementationSpecificsEntry(it->value, std::move(args)), ...);
		}
	private:		
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<String>&& entry) const;
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<float>&& entry) const;
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<int64>&& entry) const;
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<uint64>&& entry) const;
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<Vec3f>&& entry) const;
		void ParseImplementationSpecificsEntry(const VirtualMap<String>& map, ImplementationSpecificsEntry<bool>&& entry) const;
	};

	template<typename T>
	concept ParticleWithHash = requires (const T & particle) { { particle.hash } -> std::same_as<const uint32&>; };
	template<typename T>
	concept ParticleWithPosition = requires (const T & particle) { { particle.position } -> std::same_as<const Vec3f&>; };
	template<typename T>
	concept ParticleWithVelocity = requires (const T & particle) { { particle.velocity } -> std::same_as<const Vec3f&>; };
	template<typename T>
	concept ParticleWithPressure = requires (const T & particle) { { particle.pressure } -> std::same_as<const float&>; };

	struct SystemProfilingData
	{
		double timePerStep_s;

		
	};

	class System
	{
	public:
		struct StaticParticlesGeneratedEvent
		{
			ArrayView<StaticParticle> staticParticles;
		};
		EventDispatcher<StaticParticlesGeneratedEvent> staticParticlesGeneratedEventDispatcher;

		virtual ~System() { }

		void Initialize(const SystemInitParameters& initializationParameters, ParticleBufferSet& bufferSet);
		virtual void Clear() = 0;
		virtual void Update(float dt, uint simulationSteps) = 0;

		virtual StringView SystemImplementationName() = 0;			
		
		virtual uintMem GetDynamicParticleCount() const = 0;
		virtual uintMem GetStaticParticleCount() const = 0;

		virtual void EnableProfiling(bool enable) = 0;
		virtual SystemProfilingData GetProfilingData() = 0;
		virtual float GetSimulationTime() = 0;

		inline ParticleBufferSet* GetParticleBufferSet() { return particleBufferSet; }
	protected:				
		virtual void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance) = 0;
		virtual void CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance) = 0;
		virtual void InitializeInternal(const SystemInitParameters&) = 0;

		template<typename T, typename F> requires std::invocable<F, const T&>
		static Array<T> GenerateHashMapAndReorderParticles(const Array<T>& particles, Array<uint32>& hashMap, const F& hashGetter);		

		template<typename T>
		static void DebugParticles(ArrayView<T> particles, float maxInteractionDistance, uintMem hashMapSize);
		template<typename T, typename H> requires ParticleWithHash<T>
		static void DebugPrePrefixSumHashes(ArrayView<T> particles, Array<H> hashMap);
		template<ParticleWithHash T, typename H>
		static void DebugHashAndParticleMap(ArrayView<T> particles, ArrayView<H> hashMap, ArrayView<uint32> particleMap);
		template<typename T, typename H, typename F> requires std::invocable<F, const T&>
		static void DebugHashAndParticleMap(ArrayView<T> particles, ArrayView<H> hashMap, ArrayView<uint32> particleMap, const F& hashGetter);
	private:
		ParticleBufferSet* particleBufferSet;
	};	
	
	template<typename T, typename F> requires std::invocable<F, const T&>
	inline Array<T> System::GenerateHashMapAndReorderParticles(const Array<T>& particles, Array<uint32>& hashMap, const F& hashGetter)
	{
		memset(hashMap.Ptr(), 0, sizeof(uint32) * hashMap.Count());

		for (const auto& particle : particles)
			++hashMap[hashGetter(particle)];

		uint indexSum = 0;
		for (auto& index : hashMap)
		{
			indexSum += index;
			index = indexSum;
		}

		Array<T> particlesOutput;
		particlesOutput.Resize(particles.Count());

		for (const auto& particle : particles)
			particlesOutput[--hashMap[hashGetter(particle)]] = particle;

		return particlesOutput;
	}	
	template<typename T> 
	inline void System::DebugParticles(ArrayView<T> particles, float maxInteractionDistance, uintMem hashMapSize)
	{
		for (auto& particle : particles)
		{
			if constexpr (ParticleWithPosition<T>)
			{
				if (isnan(particle.position.x) || isnan(particle.position.y) || isnan(particle.position.z))
				{
					Debug::Logger::LogDebug("Client", "One or more of particle position components is nan");
					__debugbreak();
				}

				if (isinf(particle.position.x) || isinf(particle.position.y) || isinf(particle.position.z))
				{
					Debug::Logger::LogDebug("Client", "One or more of particle position components is inf");
					__debugbreak();
				}				
			}

			if constexpr (ParticleWithPressure<T>)
			{
				if (isnan(particle.pressure))
				{
					Debug::Logger::LogDebug("Client", "Particle pressure is nan");
					__debugbreak();
				}

				if (isinf(particle.pressure))
				{
					Debug::Logger::LogDebug("Client", "Particle pressure is inf");
					__debugbreak();
				}
			}

			if constexpr (ParticleWithVelocity<T>)
			{
				if (isnan(particle.velocity.x) || isnan(particle.velocity.y) || isnan(particle.velocity.z))
				{
					Debug::Logger::LogDebug("Client", "One or more of particle velocity components is nan");
					__debugbreak();
				}

				if (isinf(particle.velocity.x) || isinf(particle.velocity.y) || isinf(particle.velocity.z))
				{
					Debug::Logger::LogDebug("Client", "One or more of particle velocity components is inf");
					__debugbreak();
				}
			}

			if constexpr (ParticleWithHash<T>)
			{
				Vec3i cell = GetCell(particle.position, maxInteractionDistance);
				uint32 hash = GetHash(cell) % hashMapSize;

				if (particle.hash != hash)
				{
					Debug::Logger::LogDebug("Client", "Invalid particle hash");
					__debugbreak();
				}				
			}
		}
	}
	template<typename T, typename H> requires ParticleWithHash<T>
	inline void System::DebugPrePrefixSumHashes(ArrayView<T> particles, Array<H> hashMap)
	{		
		for (auto& particle : particles)
			hashMap[particle.hash]--;

		for (uintMem i = 0; i < hashMap.Count() - 1; ++i)
			if (hashMap[i] != 0)
			{
				Debug::Logger::LogDebug("Client", "Pre prefix sum hash not valid");
				__debugbreak();
			}
	}
	template<ParticleWithHash T, typename H>
	inline void System::DebugHashAndParticleMap(ArrayView<T> particles, ArrayView<H> hashMap, ArrayView<uint32> particleMap)
	{
		DebugHashAndParticleMap<T, H>(particles, hashMap, particleMap, [](const T& particle) {
			return particle.hash;
			});
	}
	template<typename T, typename H, typename F> requires std::invocable<F, const T&>
	inline void System::DebugHashAndParticleMap(ArrayView<T> particles, ArrayView<H> hashMap, ArrayView<uint32> particleMap, const F& hashGetter)
	{
		if (hashMap[0] != 0)
		{
			Debug::Logger::LogDebug("Client", "Hash map first value is not 0");
			__debugbreak();
		}

		if (hashMap.Last() != particles.Count())
		{
			Debug::Logger::LogDebug("Client", "Hash map last value is not the number of particles");
			__debugbreak();
		}

		uint32 lastValue = 0;
		for (uintMem i = 1; i < hashMap.Count(); ++i)
		{
			uint32 value = hashMap[i];

			if (value < lastValue)
			{
				Debug::Logger::LogDebug("Client", "Hash map value smaller than the previous value");
				__debugbreak();
			}

			if (value > particles.Count())
			{
				Debug::Logger::LogDebug("Client", "Invalid hash map value, it's greater than the dynamicParticleCount");
				__debugbreak();
			}

			if (particleMap.Empty())
				for (uint32 j = lastValue; j < value; ++j)
				{
					if (hashGetter(particles[j]) != i - 1)
					{
						Debug::Logger::LogDebug("Client", "Invalid hash value");
						__debugbreak();
					}
				}
			else
				for (uint32 j = lastValue; j < value; ++j)
				{										
					if (hashGetter(particles[particleMap[j]]) != i - 1)
					{
						Debug::Logger::LogDebug("Client", "Invalid particleMap value");
						__debugbreak();
					}
				}

			lastValue = value;
		}
	}
}