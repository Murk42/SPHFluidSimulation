#pragma once
#include "OpenCLContext.h"
#include "SPH/System/System.h"

//#define DEBUG_BUFFERS_GPU

namespace SPH
{
	class SystemGPU;

	struct ParticleBufferSet
	{
		Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;		

		Graphics::OpenGLWrapper::Fence readFinishedFence;
		cl::Event writeFinishedEvent;
		cl::Event readFinishedEvent;

		ParticleBufferSet();
		virtual ~ParticleBufferSet() { }

		virtual Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() = 0;
		virtual cl::Buffer& GetDynamicParticleBufferCL() = 0;

		virtual void StartRender(cl::CommandQueue& queue) = 0;
		virtual void EndRender(cl::CommandQueue& queue) = 0;
		virtual void StartSimulationRead(cl::CommandQueue& queue) = 0;
		virtual void StartSimulationWrite(cl::CommandQueue& queue) = 0;
		virtual void EndSimulationRead(cl::CommandQueue& queue) = 0;
		virtual void EndSimulationWrite(cl::CommandQueue& queue) = 0;
	};

	struct ParticleBufferSetInterop : ParticleBufferSet
	{		
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticleBufferGL;
		cl::BufferGL dynamicParticleBufferCL;

		ParticleBufferSetInterop(OpenCLContext& CLContext, const Array<DynamicParticle>& dynamicParticles);

		Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() override { return dynamicParticleBufferGL; }
		cl::Buffer& GetDynamicParticleBufferCL() override { return dynamicParticleBufferCL; }

		void StartRender(cl::CommandQueue& queue) override;
		void EndRender(cl::CommandQueue& queue) override;
		void StartSimulationRead(cl::CommandQueue& queue) override;
		void StartSimulationWrite(cl::CommandQueue& queue) override;
		void EndSimulationRead(cl::CommandQueue& queue) override;
		void EndSimulationWrite(cl::CommandQueue& queue) override;
	};

	struct ParticleBufferSetNoInterop : ParticleBufferSet
	{		
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleBufferGL;
		cl::Buffer dynamicParticleBufferCL;
		void* dynamicParticleBufferMap;
		const uintMem dynamicParticleCount;		

		ParticleBufferSetNoInterop(OpenCLContext& CLContext, const Array<DynamicParticle>& dynamicParticles);

		Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() override { return dynamicParticleBufferGL; }
		cl::Buffer& GetDynamicParticleBufferCL() override { return dynamicParticleBufferCL; }

		void StartRender(cl::CommandQueue& queue) override;
		void EndRender(cl::CommandQueue& queue) override;
		void StartSimulationRead(cl::CommandQueue& queue) override;
		void StartSimulationWrite(cl::CommandQueue& queue) override;
		void EndSimulationRead(cl::CommandQueue& queue) override;
		void EndSimulationWrite(cl::CommandQueue& queue) override;
	};

	class SystemGPU : public System
	{
	public:

		SystemGPU(OpenCLContext& clContext, Graphics::OpenGL::GraphicsContext_OpenGL& glContext);
		~SystemGPU();

		void Clear() override;		
		void Update(float dt, uint simulationSteps) override;

		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "GPU"; };

		void StartRender() override;
		Graphics::OpenGLWrapper::VertexArray* GetDynamicParticlesVertexArray() override;
		Graphics::OpenGLWrapper::VertexArray* GetStaticParticlesVertexArray() override;
		void EndRender() override;

		void EnableProfiling(bool enable) override { this->profiling = enable; }
		SystemProfilingData GetProfilingData() override { return { lastTimePerStep_ns }; }
		float GetSimulationTime() override { return simulationTime; }
	private:
		OpenCLContext& clContext;
		Graphics::OpenGL::GraphicsContext_OpenGL& glContext;

		//Wether the OpenCL implementation will handle sync or the user should
		bool userOpenCLOpenGLSync;

		//SystemInitParameters initParams;		

		cl::CommandQueue queue;

		cl::Program partialSumProgram;
		cl::Program SPHProgram;

		cl::Kernel computeParticleHashesKernel;
		cl::Kernel scanOnComputeGroupsKernel;
		cl::Kernel addToComputeGroupArraysKernel;
		cl::Kernel computeParticleMapKernel;
		cl::Kernel updateParticlesPressureKernel;
		cl::Kernel updateParticlesDynamicsKernel;
		cl::Kernel reorderParticlesKernel;

		Graphics::OpenGLWrapper::VertexArray staticParticleVertexArray;
		Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticleBufferGL;

		uintMem renderBufferSetIndex;
		uintMem simulationReadBufferSetIndex;
		uintMem simulationWriteBufferSetIndex;
		ParticleBufferSet* renderBufferSet;
		ParticleBufferSet* simulationReadBufferSet;
		ParticleBufferSet* simulationWriteBufferSet;

		Array<ParticleBufferSet*> bufferSetsPointers;		

		cl::Buffer dynamicParticleWriteHashMapBuffer;		
		cl::Buffer dynamicParticleReadHashMapBuffer;
		cl::Buffer particleMapBuffer;
		cl::Buffer staticParticleBuffer;
		cl::Buffer staticHashMapBuffer;
		cl::Buffer simulationParametersBuffer;
		cl::Buffer dynamicParticleIntermediateBuffer;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleHashMapSize;		

		uintMem scanKernelElementCountPerGroup;				
		uintMem hashMapBufferGroupSize;

		uintMem stepCount;

		bool profiling;		
		float simulationTime;
		uint64 lastTimePerStep_ns;

#ifdef DEBUG_BUFFERS_GPU
		float debugMaxInteractionDistance;
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#endif						
		void LoadKernels();		
		void CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance) override;
		void CreateDynamicParticlesBuffers(Array<DynamicParticle>& dynamicParticles, uintMem bufferCount, uintMem hashesPerDynamicParticle, float maxInteractionDistance) override;
		void InitializeInternal(const SystemInitParameters& initParams) override;

		void EnqueueComputeParticleHashesKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel();
		void EnqueueUpdateParticlesDynamicsKernel(float deltaTime);
		void EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize, uintMem offset);
		void EnqueueReorderParticles(cl::Buffer& inArray, cl::Buffer& outArray, cl::Buffer& particleMap, cl_event* finishedEvent);

#ifdef DEBUG_BUFFERS_GPU
		void DebugParticles(ParticleBufferSet& bufferSet);
		void DebugPrePrefixSumHashes(ParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer);
		void DebugHashes(ParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer);
#endif

		friend struct ParticleBufferSetInterop;
		friend struct ParticleBufferSetNoInterop;
	};
}