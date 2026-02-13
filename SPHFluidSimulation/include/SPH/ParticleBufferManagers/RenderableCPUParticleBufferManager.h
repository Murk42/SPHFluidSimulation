#pragma once
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/ParticleBufferManagers/ParticleBufferManagerGL.h"
#include "SPH/ParticleBufferManagers/CPUResourceLock.h"

namespace SPH
{
	class RenderableCPUParticleBufferManager :
		public ParticleBufferManagerGL
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
		void Allocate(uintMem newParticleSize, uintMem newParticleCount, void* particles, uintMem newBufferCount) override;


		//It is safe to call this function from multiple threads at the same time
		uintMem GetBufferCount() const override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles
		//aren't being allocated at the same time
		uintMem GetParticleCount() const override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles
		//aren't being allocated at the same time
		uintMem GetParticleSize() const override;

		//It is safe to call this function from multiple threads at the same time, as long as new particles
		//aren't being allocated at the same time
		Graphics::OpenGL::GraphicsBuffer* GetGraphicsBuffer(uintMem index, uintMem& bufferOffset) override;

		//It is safe to call this function from multiple threads at the same time, as long as new particles
		//aren't being allocated at the same time
		ResourceLockGuard LockRead(void* signalEvent) override;
		//It is safe to call this function from multiple threads at the same time, as long as new particles
		//aren't being allocated at the same time
		ResourceLockGuard LockWrite(void* signalEvent) override;
		//This function can only be called by the OpenGL thread
		ResourceLockGuard LockForRendering(void* signalEvent) override;

		//This function can only be called by the OpenGL thread
		void PrepareForRendering() override;

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
			ResourceLockGuard LockForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize);

			//This function can only be called by the OpenGL thread
			void CheckRenderingFence();

			//This function can only be called by the OpenGL thread
			void WaitRenderingFence();

			//This function can only be called by the OpenGL thread
			void PrepareForRendering(Graphics::OpenGL::ImmutableMappedGraphicsBuffer& buffer, uintMem index, uintMem bufferSize);
		private:
			void* ptr;
			CPULock lock;
			bool renderingFenceFlag;
			bool preparedForRendering;
			Graphics::OpenGL::Fence renderingFence;
		};

		std::thread::id openGLThreadID;
		uintMem currentBuffer;

		Array<ParticlesBuffer> buffers;
		Graphics::OpenGL::ImmutableMappedGraphicsBuffer bufferGL;
		uintMem particleSize;
		uintMem particleCount;

		//This function can only be called by the OpenGL thread
		void CheckAllRenderingFences();
	};
}