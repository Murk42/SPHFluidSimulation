#pragma once
#include "BlazeEngine/Graphics/Common/BasicIndexedMesh.h"
#include "SPH/Core/ParticleSetBlueprint.h"
#include "SPH/Core/SimulationEngine.h"
#include "SPH/Core/ParticleBufferManager.h"

namespace SPH
{
	class SceneBlueprint
	{
	public:
		SceneBlueprint();
		~SceneBlueprint();

		bool LoadScene(const Path& path);
		bool LoadScene(ReadSubStream& stream);

		inline ParticleSimulationParameters GetSystemParameters() const { return systemParameters; }

		inline const Graphics::BasicIndexedMesh& GetMesh() const { return mesh; }

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
		ParticleSimulationParameters systemParameters;
		Map<String, Array<ParticleSetBlueprint*>> layers;
		Graphics::BasicIndexedMesh mesh;

		void MarkAsInvalid();
	};
}