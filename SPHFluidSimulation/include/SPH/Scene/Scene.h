#pragma once
#include "SPH/ParticleSetBlueprint/ParticleSetBlueprint.h"
#include "SPH/kernels/CL_CPP_SPHDeclarations.h"
#include "SPH/System/System.h"

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
		
		void InitializeSystem(System& system, ParticleBufferSet& bufferSet);
	private:
		bool validScene;
		SystemParameters systemParameters;
		Map<String, Array<ParticleSetBlueprint*>> layers;

		static Map<String, std::function<ParticleSetBlueprint* ()>> particleSetBlueprintCreators;

		void MarkAsInvalid();
	};
}