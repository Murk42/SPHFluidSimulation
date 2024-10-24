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
		SystemProfilingData GetProfilingData() override { return { lastTimePerStep_s }; }
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

		uintMem stepCount;
		uint64 reorderStepCount;

		bool profiling;		
		float simulationTime;
		double lastTimePerStep_s;

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

		void EnqueueComputeParticleHashesKernel(cl::Buffer& particles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(cl::Buffer& particles, cl::Buffer* orderedParticles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer);
		void EnqueueUpdateParticlesDynamicsKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, float deltaTime);
		void EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize, uintMem offset);		

#ifdef DEBUG_BUFFERS_GPU
		void DebugParticles(GPUParticleBufferSet& bufferSet);
		void DebugPrePrefixSumHashes(GPUParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer);
		void DebugHashes(GPUParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer);
#endif

		friend struct RenderableGPUParticleBufferSetWithGLInterop;
		friend struct RenderableGPUParticleBufferSetWithoutGLInterop;
	};
}