#pragma once
#include "OpenCLContext.h"
#include "SPHSystem.h"

namespace SPH
{	
	class SystemGPU : public System
	{
	public:
		SystemGPU(OpenCLContext& clContext);

		void Initialize(const SystemInitParameters& initParams) override;

		void Update(float dt) override;


		uintMem GetParticleCount() const override { return particleCount; }
		StringView SystemImplementationName() override { return "GPU"; };				

		Array<Particle> GetParticles() override;
		//Array<uintMem> FindNeighbors(Array<Particle>& particles, Vec3f position) override;

		void StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
		void EndRender();
	private:							
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

		Array<uintMem> scanKernelGroupCounts;		

		struct ParticleBufferSet
		{
			Graphics::OpenGLWrapper::VertexArray vertexArray;
			Graphics::OpenGLWrapper::MutableDrawGraphicsBuffer particleBufferGL;
			cl::Buffer particleBufferCL;
			Array<Particle> particles;

			//Graphics::OpenGLWrapper::Fence writeFinishedFence;
			//Graphics::OpenGLWrapper::Fence readFinishedFence;
			//cl::Event readFinishedEvent;
			//cl::Event writeFinishedEvent;
		};		

		uint bufferSetIndex;
		uint nextBufferSetIndex;		
		Array<ParticleBufferSet> bufferSets;
		cl::Buffer hashMapBuffer;		
		cl::Buffer newHashMapBuffer;
		cl::Buffer particleMapBuffer;

		uintMem particleCount;
		uintMem hashMapSize;
		uintMem dynamicParticleCount;				
		uintMem scanKernelElementCountPerGroup;
		uintMem hashesPerParticle = 2;
		float maxInteractionDistance;
		bool swapHashMaps;

		void EnqueueComputeParticleHashesKernel();
		void EnqueueComputeParticleMapKernel();
		void EnqueueUpdateParticlesPressureKernel();
		void EnqueueUpdateParticlesDynamicsKernel(float deltaTime);
		void EnqueuePartialSumKernels();
	};
}