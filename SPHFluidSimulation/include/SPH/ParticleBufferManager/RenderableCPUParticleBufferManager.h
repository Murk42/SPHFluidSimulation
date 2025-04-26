#pragma once
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/Core/ParticleBufferManagerRenderData.h"
#include "SPH/ParticleBufferManager/CPUResourceLock.h"

namespace SPH
{
	class RenderableCPUParticleBufferManager : 
		public ParticleBufferManagerRenderData
	{
	public:
		RenderableCPUParticleBufferManager();
		//This buffer manager can be destroyed only by the OpenGL thread
		~RenderableCPUParticleBufferManager();

		//This function can only be called by the OpenGL thread. Calling this function will release old 
		//particle buffers. Any operations acting on the old buffers will result in undefined behaviour 
		//after calling this function. Therefore those operations must be waited on. Before waiting for the 
		//operations function 'FlushAllOperations' must be called, otherwise a deadlock might occur
		void Clear() override;
		//Its safe to call this function from any thread but not at the same time
		void Advance() override;

		//Calling this function might allocate new particle buffers and therefore invalidate the old ones. Any
		//operations acting on the old buffers will result in undefined behaviour after calling this function.
		//Therefore those operations must be waited on. Before waiting for the operations function 
		//'FlushAllOperations' must be called otherwise a deadlock might occur
		void AllocateDynamicParticles(uintMem count, DynamicParticle* particles) override;
		//Calling this function might allocate new particle buffers and therefore invalidate the old ones. Any
		//operations acting on the old buffers will result in undefined behaviour after calling this function.
		//Therefore those operations must be waited on. Before waiting for the operations function 
		//'FlushAllOperations' must be called, otherwise a deadlock might occur
		void AllocateStaticParticles(uintMem count, StaticParticle* particles) override;

		//It is safe to call this function from multiple threads at the same time
		uintMem GetDynamicParticleBufferCount() const override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		uintMem GetDynamicParticleCount() override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		uintMem GetStaticParticleCount() override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		Graphics::OpenGLWrapper::GraphicsBuffer* GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset) override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		Graphics::OpenGLWrapper::GraphicsBuffer* GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset) override;

		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		ResourceLockGuard LockDynamicParticlesForRead(void* signalEvent) override;		
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		ResourceLockGuard LockDynamicParticlesForWrite(void* signalEvent) override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		ResourceLockGuard LockStaticParticlesForRead(void* signalEvent) override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles 
		//aren't being allocated at the same time
		ResourceLockGuard LockStaticParticlesForWrite(void* signalEvent) override;		
		//This function can only be called by the OpenGL thread
		ResourceLockGuard LockDynamicParticlesForRendering(void* signalEvent) override;
		//This function can only be called by the OpenGL thread
		ResourceLockGuard LockStaticParticlesForRendering(void* signalEvent) override;

		//This function can only be called by the OpenGL thread
		void PrepareDynamicParticlesForRendering() override;
		//This function can only be called by the OpenGL thread
		void PrepareStaticParticlesForRendering() override;

		//This function can only be called by the OpenGL thread. It must be called before waiting for any
		//locking/unlocking operations to finish, otherwise a deadlock might occur
		void FlushAllOperations() override;
	private:		
		class ParticlesBuffer
		{
		public:
			ParticlesBuffer();			
			
			void SetPointer(void* ptr, bool preparedForRendering);
			
			ResourceLockGuard LockRead();
			ResourceLockGuard LockWrite(bool isOpenGLThread);
			//This function can only be called by the OpenGL thread
			ResourceLockGuard LockForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize);
			
			//This function can only be called by the OpenGL thread
			void CheckRenderingFence();

			//This function can only be called by the OpenGL thread
			void WaitRenderingFence();
			
			//This function can only be called by the OpenGL thread
			void PrepareForRendering(Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize);
		private:
			void* ptr;
			CPULock lock;
			bool renderingFenceFlag;
			bool preparedForRendering;
			Graphics::OpenGLWrapper::Fence renderingFence;
		};

		std::thread::id openGLThreadID;
		uintMem currentBuffer;

		Array<ParticlesBuffer> dynamicParticlesBuffers;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer dynamicParticlesMemory;
		uintMem dynamicParticlesCount;

		ParticlesBuffer staticParticlesBuffer;
		Graphics::OpenGLWrapper::ImmutableMappedGraphicsBuffer staticParticlesMemory;
		uintMem staticParticlesCount;

		//This function can only be called by the OpenGL thread
		void CheckAllRenderingFences(); 
		
		//This function can only be called by the OpenGL thread
		void ClearDynamicParticlesBuffers();
		//This function can only be called by the OpenGL thread
		void ClearStaticParticlesBuffer();
	};
}