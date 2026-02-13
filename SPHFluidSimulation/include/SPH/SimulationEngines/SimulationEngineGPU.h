#pragma once
#include "SPH/Core/SimulationEngine.h"
#include "SPH/Core/SceneBlueprint.h"

namespace SPH
{	
	class ParticleBufferManager;

	//TODO check CL_DEVICE_PREFERRED_INTEROP_USER_SYNC when implementing CL GL interop

	class SystemGPUKernels
	{
	public:
		SystemGPUKernels(cl_context clContext, cl_device_id clDevice);
		~SystemGPUKernels();

		void EnqueueInclusiveScanKernels(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, uintMem dynamicParticlesHashMapGroupSize, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;		
		void EnqueuePrepareStaticParticlesHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueReorderStaticParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem particles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueReorderDynamicParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueFillDynamicParticleMapAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem particles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueUpdateParticlesPressureKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		void EnqueueUpdateParticlesDynamicsKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, float deltaTime, uint64 triangleCount, cl_mem triangles, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const;
		
		//Find the smallest hash map size that is greater than the target size but still be a power of in the 
		//scanKernelElementCountPerGroup. This way the hash map size is convenient for computation.
		void DetermineHashGroupSize(uintMem targetHashMapSize, uintMem& dynamicParticlesHashMapGroupSize, uintMem& hashMapSize) const;
	private:
		cl_device_id clDevice;
		cl_context clContext;
		
		cl_program program = nullptr;

		bool supportsNonUniformWorkGroups = false;
		
		cl_kernel inclusiveScanUpPassKernel = nullptr;
		cl_kernel inclusiveScanDownPassKernel = nullptr;		
		cl_kernel prepareStaticParticlesHashMapKernel = nullptr;
		cl_kernel reorderStaticParticlesAndFinishHashMapKernel = nullptr;
		cl_kernel computeDynamicParticlesHashAndPrepareHashMapKernel = nullptr;
		cl_kernel reorderDynamicParticlesAndFinishHashMapKernel = nullptr;
		cl_kernel fillDynamicParticleMapAndFinishHashMapKernel = nullptr;
		cl_kernel updateParticlesPressureKernel = nullptr;
		cl_kernel updateParticlesDynamicsKernel = nullptr;

		uintMem inclusiveScanUpPassKernelWorkGroupSize = 0;
		uintMem inclusiveScanDownPassKernelWorkGroupSize = 0;		
		uintMem prepareStaticParticlesHashMapKernelWorkGroupSize = 0;
		uintMem reorderStaticParticlesAndFinishHashMapKernelWorkGroupSize = 0;
		uintMem computeDynamicParticlesHashAndPrepareHashMapKernelWorkGroupSize = 0;
		uintMem reorderDynamicParticlesAndFinishHashMapKernelWorkGroupSize = 0;
		uintMem fillDynamicParticleMapAndFinishHashMapKernelWorkGroupSize = 0;
		uintMem updateParticlesPressureKernelWorkGroupSize = 0;
		uintMem updateParticlesDynamicsKernelWorkGroupSize = 0;
		
		void Load();
	};

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

		SystemGPUKernels kernels;
				
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