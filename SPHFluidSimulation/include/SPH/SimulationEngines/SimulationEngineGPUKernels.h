#pragma once

namespace SPH
{
	class SimulationEngineGPUKernels
	{
	public:
		SimulationEngineGPUKernels(cl_context clContext, cl_device_id clDevice);
		~SimulationEngineGPUKernels();

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
}