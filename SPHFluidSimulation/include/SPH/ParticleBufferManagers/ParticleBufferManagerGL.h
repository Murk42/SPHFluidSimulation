#pragma once
#include "BlazeEngine/Graphics/Core/OpenGL/OpenGLWrapper/OpenGLGraphicsBuffer.h"

#include "SPH/Core/ParticleBufferManager.h"

namespace SPH
{
	class ParticleBufferManagerGL : public ParticleBufferManager
	{
	public:
		/*
			\param index - the index of the buffer for which to get the buffer, stride, and bufferOffset
			\param stride - size of the data of one particle in the buffer with index 'index' in bytes
			\param bufferOffset - byte offset for the particle buffer with the index 'index' is set into this parameter
			\return The graphics buffer which holds the particle data
		*/
		virtual Graphics::OpenGL::GraphicsBuffer* GetGraphicsBuffer(uintMem index, uintMem& bufferOffset) = 0;

		/*
			This function must be called from the thread on which the OpenGL context was created on.

			\param signalEvent - pointer to the event object
			\return A ResourceLockGuard object that stores a index of the buffer for rendering. To get the index cast the pointer from GetResource() to uintMem.
			The lock guard should be unlocked when rendering is finished
		*/
		virtual ResourceLockGuard LockForRendering(void* signalEvent) = 0;

		virtual void PrepareForRendering() = 0;
	};
}