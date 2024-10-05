#pragma once
#include "OpenCLContext.h"
#include "SPH/System/System.h"

//#define VISUALIZE_NEIGHBOURS
//#define DEBUG_ARRAYS

namespace SPH
{
	class SystemGPU;

	struct ParticleBufferSet
	{
		Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;		

		Graphics::OpenGLWrapper::Fence readFinishedFence;
		cl::Event writeFinishedEvent;
		cl::Event readFinishedEvent;

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
		SystemGPU& system;
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticleBufferGL;
		cl::BufferGL dynamicParticleBufferCL;

#ifdef VISUALIZE_NEIGHBOURS
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticleColorBufferGL;
		Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer staticParticleColorBufferGL;
		cl::BufferGL dynamicParticleColorBufferCL;
		cl::BufferGL staticParticleColorBufferCL;
#endif	

		ParticleBufferSetInterop(SystemGPU& system, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles);

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
		SystemGPU& system;
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

		ParticleBufferSetNoInterop(SystemGPU& system, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles);

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
		SystemGPU(OpenCLContext& clContext, Graphics::OpenGL::GraphicsContext_OpenGL& glContext);
		~SystemGPU();

		void Clear();
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
		OpenCLContext& clContext;
		Graphics::OpenGL::GraphicsContext_OpenGL& glContext;

		//Wether the OpenCL implementation will handle sync or the user should
		bool userOpenCLOpenGLSync;

		SystemInitParameters initParams;		

		cl::CommandQueue queue;

		cl::Program partialSumProgram;
		cl::Program SPHProgram;

		cl::Kernel computeParticleHashesKernel;
		cl::Kernel scanOnComputeGroupsKernel;
		cl::Kernel addToComputeGroupArraysKernel;
		cl::Kernel computeParticleMapKernel;
		cl::Kernel updateParticlesPressureKernel;
		cl::Kernel updateParticlesDynamicsKernel;

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

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
		uintMem dynamicParticleHashMapSize;
		uintMem staticParticleHashMapSize;
		//uintMem staticHashMapSize;

		uintMem scanKernelElementCountPerGroup;				

		float particleMoveElapsedTime;

#ifdef DEBUG_ARRAYS
		Array<DynamicParticle> debugParticlesArray;
		Array<uint32> debugHashMapArray;
		Array<uint32> debugParticleMapArray;
#endif

		ParticleSimulationParameters simulationParameters;

		void IncrementRenderBufferSet();
		void IncrementSimulationBufferSets();

		void LoadKernels();		
		void GenerateParticles(Array<DynamicParticle>& dynamicParticles, Array<StaticParticle>& staticParticles);
		void CreateBuffers(const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, uintMem dynamicParticleHashMapSize, const Array<uint32>& staticParticleHashMap);
		void CalculateDynamicParticleHashes(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, uintMem dynamicParticleHashMapSize, float maxInteractionDistance);
		Array<uint32> CalculateStaticParticleHashes(Array<StaticParticle>& staticParticles, uintMem staticHashMapSize, float maxInteractionDistance);

		void EnqueueComputeParticleHashesKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, cl_event* finishedEvent);
		void EnqueueComputeParticleMapKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent);
		void EnqueueUpdateParticlesPressureKernel();
		void EnqueueUpdateParticlesDynamicsKernel(float deltaTime);
		void EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize);

#ifdef DEBUG_ARRAYS
		void DebugParticles(ParticleBufferSet& bufferSet);
		void DebugPrePrefixSumHashes(ParticleBufferSet& bufferSet);
		void DebugHashes(ParticleBufferSet& bufferSet);
#endif

		friend struct ParticleBufferSetInterop;
		friend struct ParticleBufferSetNoInterop;
	};
}