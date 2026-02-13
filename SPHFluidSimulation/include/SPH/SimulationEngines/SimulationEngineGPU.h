#pragma once
#include "SPH/Core/SimulationEngine.h"
#include "SPH/Core/SceneBlueprint.h"
#include "SPH/SimulationEngines/SimulationEngineGPUKernels.h"

namespace SPH
{	
	class ParticleBufferManager;

	//TODO check CL_DEVICE_PREFERRED_INTEROP_USER_SYNC when implementing CL GL interop

	class SimulationEngineGPU : public SimulationEngine
	{
	public:
		SimulationEngineGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue);
		~SimulationEngineGPU();

		void Clear() override;		
		void Initialize(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager) override;
		void Update(float dt, uint simulationStepCount) override;

		StringView SystemImplementationName() override { return "GPU"; };		

		float GetSimulationTime() override { return simulationTime; }
	private:
		cl_device_id clDevice = nullptr;
		cl_context clContext = nullptr;
		cl_command_queue clCommandQueue = nullptr;				

		bool initialized = true;

		SimulationEngineGPUKernels kernels;
				
		cl_mem dynamicParticlesHashMap = nullptr;
		cl_mem particleMapBuffer = nullptr;		
		cl_mem staticParticlesHashMap = nullptr;
		cl_mem particleBehaviourParametersBuffer = nullptr;

		ParticleBehaviourParameters particleBehaviourParameters;

		ParticleBufferManager* dynamicParticlesBufferManager = nullptr;
		ParticleBufferManager* staticParticlesBufferManager = nullptr;

		cl_mem triangles = nullptr;
		uintMem triangleCount = 0;

		uintMem dynamicParticlesHashMapSize = 0;
		uintMem staticParticlesHashMapSize = 0;		

		uintMem staticParticlesHashMapGroupSize = 0;
		uintMem dynamicParticlesHashMapGroupSize = 0;

		float reorderElapsedTime = 0;
		float reorderTimeInterval = FLT_MAX;

		float simulationTime = 0;

#pragma region		
		Array<StaticParticle> debugStaticParticlesArray;
		Array<uint32> debugStaticHashMapArray;
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#pragma endregion DEBUG_BUFFERS_GPU

		void InitializeStaticParticles(SceneBlueprint& scene, ParticleBufferManager& staticParticlesBufferManager);
		void InitializeDynamicParticles(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager);

		//Call this function to retrieve buffer values and break. Will work evend if DEBUG_BUFFERS_GPU isn't defined
		void InspectStaticBuffers(cl_mem particles);
#pragma region		
		static void DebugStaticParticles(cl_command_queue clCommandQueue, Array<StaticParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance);
		static void DebugDynamicParticles(cl_command_queue clCommandQueue, Array<DynamicParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance);
		static void DebugStaticParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<StaticParticle>& tempParticles, Array<uint32>& tempHashMap, cl_mem particles, cl_mem hashMap, float maxInteractionDistance);
		static void DebugDynamicParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<DynamicParticle>& tempParticles, Array<uint32>& tempHashMap, Array<uint32>& tempParticleMap, cl_mem particles, cl_mem hashMap, cl_mem particleMap);
#pragma endregion DEBUG_BUFFERS_GPU

		friend struct RenderableGPUParticleBufferSetWithGLInterop;
		friend struct RenderableGPUParticleBufferSetWithoutGLInterop;
	};
}