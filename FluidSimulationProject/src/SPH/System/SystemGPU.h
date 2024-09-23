#pragma once
#include "OpenCLContext.h"
#include "SPH/System/System.h"

#define USE_OPENCL_OPENGL_INTEROP

namespace SPH
{	
	class SystemGPU : public System
	{
	public:
		SystemGPU(OpenCLContext& clContext);

		void Initialize(const SystemInitParameters& initParams) override;
		void LoadKernels(const ParticleBehaviourParameters& behaviourParameters, const ParticleBoundParameters& boundingParameters);

		void Update(float dt) override;

		uintMem GetDynamicParticleCount() const override { return dynamicParticleCount; }
		uintMem GetStaticParticleCount() const override { return staticParticleCount; }
		StringView SystemImplementationName() override { return "GPU"; };				
	
		void StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray();
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray();
		void EndRender();
	private:							
		struct ParticleBufferSet
		{
			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;
			Graphics::OpenGLWrapper::VertexArray staticParticleVertexArray;
#ifdef USE_OPENCL_OPENGL_INTEROP
			Graphics::OpenGLWrapper::ImmutableDynamicGraphicsBuffer dynamicParticleBufferGL;
			Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticleBufferGL;
			cl::BufferGL particleBufferCL;
#else
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleBufferGL;
			Graphics::OpenGLWrapper::ImmutableStaticGraphicsBuffer staticParticleBufferGL;
			cl::Buffer particleBufferCL;
			void* particleBufferMap;
#endif

			Graphics::OpenGLWrapper::Fence readFinishedFence;
			cl::Event writeFinishedEvent;
		};

		OpenCLContext& clContext;

		cl::CommandQueue queue;

		cl::Program partialSumProgram ;
		cl::Program SPHProgram;
		
		cl::Kernel computeParticleHashesKernel;		
		cl::Kernel scanOnComputeGroupsKernel;
		cl::Kernel addToComputeGroupArraysKernel;
		cl::Kernel computeParticleMapKernel;
		cl::Kernel updateParticlesPressureKernel;		
		cl::Kernel updateParticlesDynamicsKernel;

		uint bufferSetIndex;
		uint nextBufferSetIndex;		
		Array<ParticleBufferSet> bufferSets;
		cl::Buffer hashMapBuffer;		
		cl::Buffer newHashMapBuffer;
		cl::Buffer particleMapBuffer;
		cl::Buffer staticParticleBuffer;
		cl::Buffer staticHashMapBuffer;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;				
		uintMem hashMapSize;
		uintMem staticHashMapSize;

		uintMem scanKernelElementCountPerGroup;
		uintMem hashesPerParticle;
		uintMem hashesPerStaticParticle;		
		bool swapHashMaps;

		float particleMoveElapsedTime;

		void GenerateHashes(Array<StaticParticle>& staticParticles, Array<uint32>& dynamicHashMap, Array<uint32>& staticHashMap, float maxInteractionDistance);
		void CreateBuffers(uintMem bufferCount, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, const Array<uint32>& dynamicHashMap, const Array<uint32>& staticHashMap);
		void BuildPartialSumProgram();
		void BuildSPHProgram(const ParticleBehaviourParameters& behaviourParameters, const ParticleBoundParameters& boundParameters);
		
		void EnqueueComputeParticleHashesKernel(cl_event* event);
		void EnqueueComputeParticleMapKernel(cl::Buffer& particleBuffer);
		void EnqueueUpdateParticlesPressureKernel();
		void EnqueueUpdateParticlesDynamicsKernel(float deltaTime);
		void EnqueuePartialSumKernels();
	};
}