#pragma once
#include "SPH/System/System.h"

namespace SPH
{
	class GPUParticleBufferSet;

	class SystemGPU : public System
	{
	public:
		SystemGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue queue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext);
		~SystemGPU();

		void Clear() override;		
		void Update(float dt, uint simulationSteps) override;

		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "GPU"; };		

		void EnableProfiling(bool enable) override { this->profiling = enable; }
		const SystemProfilingData& GetProfilingData() override { return systemProfilingData; }
		float GetSimulationTime() override { return simulationTime; }

		static Set<String> GetRequiredOpenCLExtensions();
	private:
		cl_device_id clDevice;
		cl_context clContext;		
		Graphics::OpenGL::GraphicsContext_OpenGL& glContext;
		cl_command_queue queue = nullptr;

		//Wether the OpenCL implementation will handle sync or the user should
		bool userOpenCLOpenGLSync = false;

		//SystemInitParameters initParams;				

		cl_program partialSumProgram = nullptr;
		cl_program SPHProgram = nullptr;

		cl_kernel computeParticleHashesKernel = nullptr;
		cl_kernel scanOnComputeGroupsKernel = nullptr;
		cl_kernel addToComputeGroupArraysKernel = nullptr;
		cl_kernel increaseHashMapKernel = nullptr;
		cl_kernel computeParticleMapKernel = nullptr;
		cl_kernel updateParticlesPressureKernel = nullptr;
		cl_kernel updateParticlesDynamicsKernel = nullptr;

		uintMem computeParticleHashesKernelPreferredGroupSize = 0;
		uintMem scanOnComputeGroupsKernelPreferredGroupSize = 0;
		uintMem addToComputeGroupArraysKernelPreferredGroupSize = 0;
		uintMem increaseHashMapKernelPreferredGroupSize = 0;
		uintMem computeParticleMapKernelPreferredGroupSize = 0;
		uintMem updateParticlesPressureKernelPreferredGroupSize = 0;
		uintMem updateParticlesDynamicsKernelPreferredGroupSize = 0;
		bool nonUniformWorkGroupSizeSupported = 0;
				
		cl_mem dynamicParticleHashMapBuffer = nullptr;
		cl_mem particleMapBuffer = nullptr;		
		cl_mem staticHashMapBuffer = nullptr;
		cl_mem simulationParametersBuffer = nullptr;

		GPUParticleBufferSet* particleBufferSet = nullptr;

		uintMem dynamicParticleCount = 0;
		uintMem staticParticleCount = 0;
		uintMem dynamicParticleHashMapSize = 0;
		uintMem staticParticleHashMapSize = 0;

		uintMem hashMapBufferGroupSize = 0;

		float reorderElapsedTime = 0;
		float reorderTimeInterval = 0;

		bool profiling = false;
		bool detailedProfiling = false;
		SystemProfilingData systemProfilingData;
		float simulationTime = 0;
		bool openCLChoosesGroupSize = false;
		bool useMaxGroupSize = false;	

#pragma region
		float debugMaxInteractionDistance;
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#pragma endregion DEBUG_BUFFERS_GPU

		void LoadKernels();		
		void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, float maxInteractionDistance) override;
		void CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, float maxInteractionDistance) override;
		void InitializeInternal(const SystemParameters& initParams) override;

		void EnqueueComputeParticleHashesKernel(cl_mem particles, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueClearHashMap(ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesDynamicsKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueIncreaseHashMap(cl_mem particleBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
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