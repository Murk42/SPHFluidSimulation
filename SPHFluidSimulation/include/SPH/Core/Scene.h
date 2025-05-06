#pragma once
#include "SPH/Core/ParticleSetBlueprint.h"
#include "SPH/Core/System.h"

namespace SPH
{	
	class Scene
	{
	public:	
		Scene();
		~Scene();

		bool LoadScene(const Path& path);
		bool LoadScene(ReadSubStream& stream);
		
		inline SystemParameters GetSystemParameters() const { return systemParameters; }

		Array<Vec3f> GenerateLayerParticlePositions(StringView layerName);
		template<typename Particle>
		void GenerateLayerParticles(StringView layerName, Array<Particle>& particles)
		{
			if (!validScene)
			{
				Debug::Logger::LogWarning("SPH Library", "Trying to initiaize a system with a invalid scene");
				return;
			}

			auto it = layers.Find(layerName);

			if (it.IsNull())
				return;
			
			uintMem count = 0;
			for (auto& blueprint : it->value)
				count += blueprint->GetParticleCount();

			uintMem index = particles.Count();
			particles.Resize(particles.Count() + count);

			for (auto& blueprint : it->value)
			{
				blueprint->WriteParticlesPositions([](uintMem index, const Vec3f& pos, void* userData) {
					((Particle*)userData)[index].position = pos;
					}, particles.Ptr() + index);
				index += blueprint->GetParticleCount();
			}
		}
	private:
		bool validScene;
		SystemParameters systemParameters;
		Map<String, Array<ParticleSetBlueprint*>> layers;

		static Map<String, std::function<ParticleSetBlueprint* ()>> particleSetBlueprintCreators;

		void MarkAsInvalid();
	};
}