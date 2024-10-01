#pragma once
#include "OpenCLContext.h"
#include "SPH/System/System.h"

//#define VISUALIZE_NEIGHBOURS

namespace SPH
{
	struct ParticleBufferSet
	{
		Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;
		Graphics::OpenGLWrapper::VertexArray staticParticleVertexArray;

		Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticleBufferGL;

		Graphics::OpenGLWrapper::Fence readFinishedFence;
		cl::Event writeFinishedEvent;

		ParticleBufferSet(const Array<StaticParticle>& staticParticles);
		virtual ~ParticleBufferSet() { }

		virtual Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() = 0;

		virtual cl::Buffer& GetDynamicParticleBufferCL() = 0;
#ifdef VISUALIZE_NEIGHBOURS
		virtual cl::Buffer& GetDynamicParticleColorBufferCL() = 0;
		virtual cl::Buffer& GetStaticParticleColorBufferCL() = 0;
#endif
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

#ifdef VISUALIZE_NEIGHBOURS
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticleColorBufferGL;
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer staticParticleColorBufferGL;
		cl::BufferGL dynamicParticleColorBufferCL;
		cl::BufferGL staticParticleColorBufferCL;
#endif	

		ParticleBufferSetInterop(OpenCLContext& clContext, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles);

		Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() override { return dynamicParticleBufferGL; }
		cl::Buffer& GetDynamicParticleBufferCL() override { return dynamicParticleBufferCL; }

		void StartRender(cl::CommandQueue& queue) override;
		void EndRender(cl::CommandQueue& queue) override;
		void StartSimulationRead(cl::CommandQueue& queue) override;
		void StartSimulationWrite(cl::CommandQueue& queue) override;
		void EndSimulationRead(cl::CommandQueue& queue) override;
		void EndSimulationWrite(cl::CommandQueue& queue) override;

#ifdef VISUALIZE_NEIGHBOURS
		cl::Buffer& GetDynamicParticleColorBufferCL() override { return dynamicParticleColorBufferCL; }
		cl::Buffer& GetStaticParticleColorBufferCL() override { return staticParticleColorBufferCL; }
#endif
	};

	struct ParticleBufferSetNoInterop : ParticleBufferSet
	{
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleBufferGL;
		cl::Buffer dynamicParticleBufferCL;

		void* dynamicParticleBufferMap;
#ifdef VISUALIZE_NEIGHBOURS
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleColorBufferGL;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer staticParticleColorBufferGL;
		cl::Buffer dynamicParticleColorBufferCL;
		cl::Buffer staticParticleColorBufferCL;

		void* dynamicParticleColorBufferMap;
		void* staticParticleColorBufferMap;
#endif	

		const uintMem dynamicParticleCount;
		const uintMem staticParticleCount;

		ParticleBufferSetNoInterop(OpenCLContext& clContext, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles);

		Graphics::OpenGLWrapper::GraphicsBuffer& GetDynamicParticleBufferGL() override { return dynamicParticleBufferGL; }
		cl::Buffer& GetDynamicParticleBufferCL() override { return dynamicParticleBufferCL; }

		void StartRender(cl::CommandQueue& queue) override;
		void EndRender(cl::CommandQueue& queue) override;
		void StartSimulationRead(cl::CommandQueue& queue) override;
		void StartSimulationWrite(cl::CommandQueue& queue) override;
		void EndSimulationRead(cl::CommandQueue& queue) override;
		void EndSimulationWrite(cl::CommandQueue& queue) override;

#ifdef VISUALIZE_NEIGHBOURS
		cl::Buffer& GetDynamicParticleColorBufferCL() override { return dynamicParticleColorBufferCL; }
		cl::Buffer& GetStaticParticleColorBufferCL() override { return staticParticleColorBufferCL; }
#endif
	};

	class SystemGPU : public System
	{
	public:
		SystemGPU(OpenCLContext& clContext);
		~SystemGPU();

		void Initialize(const SystemInitParameters& initParams) override;		

		void Update(float dt) override;

		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "GPU"; };

		void StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray();
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray();
		void EndRender();
	private:
		//Wether the OpenCL implementation will handle sync or the user should
		bool userOpenCLOpenGLSync;

		SystemInitParameters initParams;		

		OpenCLContext& clContext;

		cl::CommandQueue queue;

		cl::Program partialSumProgram;
		cl::Program SPHProgram;

		cl::Kernel computeParticleHashesKernel;
		cl::Kernel scanOnComputeGroupsKernel;
		cl::Kernel addToComputeGroupArraysKernel;
		cl::Kernel computeParticleMapKernel;
		cl::Kernel updateParticlesPressureKernel;
		cl::Kernel updateParticlesDynamicsKernel;

		uintMem renderBufferSetIndex;
		uintMem simulationReadBufferSetIndex;
		uintMem simulationWriteBufferSetIndex;
		ParticleBufferSet* renderBufferSet;
		ParticleBufferSet* simulationReadBufferSet;
		ParticleBufferSet* simulationWriteBufferSet;

		Array<ParticleBufferSet*> bufferSetsPointers;		

		cl::Buffer hashMapBuffer;
		cl::Buffer newHashMapBuffer;
		cl::Buffer particleMapBuffer;
		cl::Buffer staticParticleBuffer;
		cl::Buffer staticHashMapBuffer;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
		uintMem dynamicHashMapSize;
		//uintMem staticHashMapSize;

		uintMem scanKernelElementCountPerGroup;		
		bool swapHashMaps;

		float particleMoveElapsedTime;

		void IncrementRenderBufferSet();
		void IncrementSimulationBufferSets();

		void LoadKernels();		
		void GenerateParticles(Array<DynamicParticle>& dynamicParticles, Array<StaticParticle>& staticParticles);
		void CreateBuffers(const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, uintMem dynamicHashMapSize, const Array<uint32>& staticHashMap);
		void CalculateDynamicParticleHashes(uintMem dynamicHashMapSize, uintMem dynamicParticleCount);
		Array<uint32> CalculateStaticParticleHashes(Array<StaticParticle>& staticParticles, uintMem staticHashMapSize);

		void EnqueueComputeParticleHashesKernel(ParticleBufferSet* bufferSet, cl::Buffer& hashMapBuffer, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(ParticleBufferSet* bufferSet, cl::Buffer& hashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel();
		void EnqueueUpdateParticlesDynamicsKernel(float deltaTime);
		void EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize);
	};
}