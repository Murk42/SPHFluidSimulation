#pragma once
#include "SPH/System/System.h"

namespace SPH
{	
	class ParticleBufferManager;

	//TODO check CL_DEVICE_PREFERRED_INTEROP_USER_SYNC when implementing CL GL interop

	class SystemGPU : public System
	{
	public:
		SystemGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext);
		~SystemGPU();

		void Clear() override;		
		void Initialize(const SystemParameters& parameters, ParticleBufferManager& particleBufferManager, Array<DynamicParticle> dynamicParticles, Array<StaticParticle> staticParticles) override;
		void Update(float dt, uint simulationStepCount) override;

		StringView SystemImplementationName() override { return "GPU"; };		

		float GetSimulationTime() override { return simulationTime; }
	private:
		cl_device_id clDevice;
		cl_context clContext;			
		Graphics::OpenGL::GraphicsContext_OpenGL& glContext;
		cl_command_queue clCommandQueue = nullptr;		

		cl_program partialSumProgram = nullptr;
		cl_program SPHProgram = nullptr;

		cl_kernel computeParticleHashesKernel = nullptr;
		cl_kernel scanOnComputeGroupsKernel = nullptr;
		cl_kernel addToComputeGroupArraysKernel = nullptr;
		cl_kernel incrementHashMapKernel = nullptr;
		cl_kernel computeParticleMapKernel = nullptr;
		cl_kernel updateParticlesPressureKernel = nullptr;
		cl_kernel updateParticlesDynamicsKernel = nullptr;

		uintMem computeParticleHashesKernelPreferredGroupSize = 0;
		uintMem scanOnComputeGroupsKernelPreferredGroupSize = 0;
		uintMem addToComputeGroupArraysKernelPreferredGroupSize = 0;
		uintMem incrementHashMapKernelPreferredGroupSize = 0;
		uintMem computeParticleMapKernelPreferredGroupSize = 0;
		uintMem updateParticlesPressureKernelPreferredGroupSize = 0;
		uintMem updateParticlesDynamicsKernelPreferredGroupSize = 0;
				
		cl_mem dynamicParticleHashMapBuffer = nullptr;
		cl_mem particleMapBuffer = nullptr;		
		cl_mem staticHashMapBuffer = nullptr;
		cl_mem particleBehaviourParametersBuffer = nullptr;

		ParticleBufferManager* particleBufferManager = nullptr;

		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleHashMapSize;

		uintMem hashMapBufferGroupSize;

		float reorderElapsedTime;
		float reorderTimeInterval;

		float simulationTime;				

#pragma region
		float debugMaxInteractionDistance;
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#pragma endregion DEBUG_BUFFERS_GPU

		void LoadKernels();		
		//Find the smallest hash map size that is greater than the target size but still be a power of in the 
		//scanKernelElementCountPerGroup. This way the hash map size is convenient for computation.
		static void DetermineHashGroupSize(cl_device_id clDevice, cl_kernel scanGroupKernel, cl_kernel addGroupKernel, uintMem targetHashMapSize, uintMem& hashMapBufferGroupSize, uintMem& hashMapSize);		
		void CalculateHashAndParticleMap(float maxInteractionDistance);

		void EnqueueComputeParticleHashesKernel(cl_mem particles, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueClearHashMapCommand(ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesDynamicsKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueIncrementHashMapKernel(cl_mem particleBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueuePartialSumKernels(ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(cl_mem particles, const cl_mem* orderedParticles, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);

#pragma region
		void DebugParticles(cl_mem particles);
		void DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer);
		void DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount = 0);		
		void DebugHashes(cl_mem particles, cl_mem hashMapBuffer);
#pragma endregion DEBUG_BUFFERS_GPU

		friend struct RenderableGPUParticleBufferSetWithGLInterop;
		friend struct RenderableGPUParticleBufferSetWithoutGLInterop;
	};
}