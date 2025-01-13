#pragma once
#include "SPH/ParticleBufferManager/CPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/ResourceLock.h"
#include "SPH/ParticleBufferManager/ParticleBufferManagerRenderData.h"

namespace SPH
{	
	/*
	class RenderableCPUParticleBufferManager :		
		public CPUParticleBufferManager,
		public ParticleBufferManagerRenderData
	{
	public:					
		RenderableCPUParticleBufferManager();
		
		void Clear();
		void Advance() override;		

		void ManagerDynamicParticles(ArrayView<DynamicParticle> dynamicParticles) override;
		void ManagerStaticParticles(ArrayView<StaticParticle> staticParticles) override;
		
		CPUParticleReadBufferHandle& GetReadBufferHandle() override;
		CPUParticleWriteBufferHandle& GetWriteBufferHandle() override;
		CPUParticleWriteBufferHandle& GetReadWriteBufferHandle() override;
		const StaticParticle* GetStaticParticles() override;
		ParticleRenderBufferHandle& GetRenderBufferHandle() override;
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticleVertexArray() override;

		uintMem GetDynamicParticleCount() override;		
		uintMem GetStaticParticleCount() override;
	private:
		class Buffer : 
			public CPUParticleReadBufferHandle,
			public CPUParticleWriteBufferHandle,
			public ParticleRenderBufferHandle
		{
			const RenderableCPUParticleBufferManager& bufferManager;

			Graphics::OpenGLWrapper::VertexArray dynamicParticleVertexArray;
			Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticleBuffer;
			DynamicParticle* dynamicParticlesMap;						

			CPUSync readSync;
			CPUSync writeSync;	

			std::mutex stateMutex;
			std::condition_variable stateCV;
						
			Graphics::OpenGLWrapper::Fence renderingFinishedFence;						
		public:
			Buffer(const RenderableCPUParticleBufferManager& bufferManager);
			~Buffer();

			void Clear();
			
			void ManagerDynamicParticles(const DynamicParticle* particles);

			CPUSync& GetReadSync() override;
			CPUSync& GetWriteSync() override;
			void StartRender() override;
			void FinishRender() override;
			void WaitRender() override;

			const DynamicParticle* GetReadBuffer() override { return dynamicParticlesMap; }
			DynamicParticle* GetWriteBuffer() override { return dynamicParticlesMap; }

			Graphics::OpenGLWrapper::VertexArray& GetVertexArray() override { return dynamicParticleVertexArray; }			
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		Graphics::OpenGLWrapper::VertexArray staticParticleVertexArray;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer staticParticleBuffer;
		StaticParticle* staticParticlesMap;

		uintMem dynamicParticleCount;
		uintMem staticParticleCount;
	};
	*/

	class RenderableCPUParticleBufferManager : 
		public CPUParticleBufferManager,
		public ParticleBufferManagerRenderData
	{
	public:
		RenderableCPUParticleBufferManager();
		~RenderableCPUParticleBufferManager();

		void Clear() override;
		void Advance() override;

		void AllocateDynamicParticles(uintMem count) override;
		void AllocateStaticParticles(uintMem count) override;

		uintMem GetDynamicParticleCount() override;
		uintMem GetStaticParticleCount() override;

		CPUParticleBufferLockGuard LockDynamicParticlesActiveRead(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;
		CPUParticleBufferLockGuard LockDynamicParticlesAvailableRead(const TimeInterval& timeInterval = TimeInterval::Infinity(), uintMem* index) override;
		CPUParticleBufferLockGuard LockDynamicParticlesReadWrite(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;

		CPUParticleBufferLockGuard LockStaticParticlesRead(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;
		CPUParticleBufferLockGuard LockStaticParticlesReadWrite(const TimeInterval& timeInterval = TimeInterval::Infinity()) override;

		Graphics::OpenGLWrapper::VertexArray& GetDynamicParticlesVertexArray(uintMem index) override;
		Graphics::OpenGLWrapper::VertexArray& GetStaticParticlesVertexArray() override;

	private:
		struct Buffer
		{
			Graphics::OpenGLWrapper::VertexArray dynamicParticlesVA;

			std::mutex dynamicParticlesFencesMutex;
			Array<Graphics::OpenGLWrapper::Fence> dynamicParticleFences;
			Lock dynamicParticlesLock;			
			DynamicParticle* dynamicParticlesMap;
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		std::mutex staticParticlesFencesMutex;
		Array<Graphics::OpenGLWrapper::Fence> staticParticleFences;
		Lock staticParticlesLock;		
		StaticParticle* staticParticlesMap;
		Graphics::OpenGLWrapper::VertexArray staticParticlesVA;

		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer staticParticlesBuffer;
		uintMem staticParticlesCount;
		
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesBuffer;
		uintMem dynamicParticlesCount;

		void CleanDynamicParticlesBuffers();
		void CleanStaticParticlesBuffer();
	};
}