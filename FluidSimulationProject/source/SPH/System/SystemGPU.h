#pragma once
#include "OpenCLContext.h"
#include "SPH/System/System.h"
#include <memory>

//#define DEBUG_BUFFERS_GPU

namespace SPH
{
	class GPUParticleBufferSet;

	class SystemGPU : public System
	{
	public:
		SystemGPU(OpenCLContext& clContext, cl::CommandQueue& queue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext);
		~SystemGPU();

		void Clear() override;		
		void Update(float dt, uint simulationSteps) override;

		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "GPU"; };		

		void EnableProfiling(bool enable) override { this->profiling = enable; }
		const SystemProfilingData& GetProfilingData() override { return systemProfilingData; }
		float GetSimulationTime() override { return simulationTime; }
	private:
		OpenCLContext& clContext;
		cl::CommandQueue& queue;
		Graphics::OpenGL::GraphicsContext_OpenGL& glContext;

		//Wether the OpenCL implementation will handle sync or the user should
		bool userOpenCLOpenGLSync;

		//SystemInitParameters initParams;				

		cl::Program partialSumProgram;
		cl::Program SPHProgram;

		cl::Kernel computeParticleHashesKernel;
		cl::Kernel scanOnComputeGroupsKernel;
		cl::Kernel addToComputeGroupArraysKernel;
		cl::Kernel computeParticleMapKernel;
		cl::Kernel updateParticlesPressureKernel;
		cl::Kernel updateParticlesDynamicsKernel;		

		uintMem computeParticleHashesKernelPreferredGroupSize;
		uintMem scanOnComputeGroupsKernelPreferredGroupSize;
		uintMem addToComputeGroupArraysKernelPreferredGroupSize;
		uintMem computeParticleMapKernelPreferredGroupSize;
		uintMem updateParticlesPressureKernelPreferredGroupSize;
		uintMem updateParticlesDynamicsKernelPreferredGroupSize;				
		bool nonUniformWorkGroupSizeSupported;
				
		cl::Buffer dynamicParticleWriteHashMapBuffer;		
		cl::Buffer dynamicParticleReadHashMapBuffer;
		cl::Buffer particleMapBuffer;
		cl::Buffer staticParticleBuffer;
		cl::Buffer staticHashMapBuffer;
		cl::Buffer simulationParametersBuffer;		

		GPUParticleBufferSet* particleBufferSet;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleHashMapSize;					

		uintMem hashMapBufferGroupSize;

		float reorderElapsedTime;
		float reorderTimeInterval;

		bool profiling;		
		bool detailedProfiling;
		SystemProfilingData systemProfilingData;
		float simulationTime;
		bool openCLChoosesGroupSize;
		bool useMaxGroupSize;				

#ifdef DEBUG_BUFFERS_GPU
		float debugMaxInteractionDistance;
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#endif						
		void LoadKernels();		
		void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance) override;
		void CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance) override;
		void InitializeInternal(const SystemInitParameters& initParams) override;

		void EnqueueComputeParticleHashesKernel(cl::Buffer& particles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueClearHashMap(ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueUpdateParticlesDynamicsKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(cl::Buffer& particles, cl::Buffer* orderedParticles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent);

#ifdef DEBUG_BUFFERS_GPU
		void DebugParticles(cl::Buffer& particles);
		void DebugPrePrefixSumHashes(cl::Buffer& particles, cl::Buffer& hashMapBuffer);
		void DebugInterPrefixSumHashes(cl::Buffer* particles, cl::Buffer& hashMapBuffer, uintMem layerCount = 0);		
		void DebugHashes(cl::Buffer& particles, cl::Buffer& hashMapBuffer);
#endif

		friend struct RenderableGPUParticleBufferSetWithGLInterop;
		friend struct RenderableGPUParticleBufferSetWithoutGLInterop;
	};
}