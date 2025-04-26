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

		Array<Vec3f> GenerateLayerParticlePositions(StringView layerName);

		bool LoadScene(const Path& path);
		bool LoadScene(ReadSubStream& stream);
		
		void InitializeSystem(System& system, ParticleBufferManager& bufferManager);
	private:
		bool validScene;
		SystemParameters systemParameters;
		Map<String, Array<ParticleSetBlueprint*>> layers;

		static Map<String, std::function<ParticleSetBlueprint* ()>> particleSetBlueprintCreators;

		void MarkAsInvalid();
	};
}