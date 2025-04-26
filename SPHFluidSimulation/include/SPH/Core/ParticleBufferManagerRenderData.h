#pragma once
#include "BlazeEngineGraphics/Core/OpenGL/OpenGLWrapper/OpenGLGraphicsBuffer.h"

#include "SPH/Core/ParticleBufferManager.h"

namespace SPH
{	
	class ParticleBufferManagerRenderData : public ParticleBufferManager
	{
	public:
		/*
			\param index - the index of the buffer for which to get the buffer, stride, and bufferOffset
			\param stride - size of the data of one particle in the buffer with index 'index' in bytes
			\param bufferOffset - byte offset for the particle buffer with the index 'index' is set into this parameter
			\return The graphics buffer which holds the particle data
		*/
		virtual Graphics::OpenGLWrapper::GraphicsBuffer* GetDynamicParticlesGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset) = 0;
		/*
			\param index - the index of the buffer for which to get the buffer, stride, and bufferOffset
			\param stride - size of the data of one particle in the buffer with index 'index' in bytes
			\param bufferOffset - byte offset for the particle buffer with the index 'index' is set into this parameter
			\return The graphics buffer which holds the particle data
		*/
		virtual Graphics::OpenGLWrapper::GraphicsBuffer* GetStaticParticlesGraphicsBuffer(uintMem& stride, uintMem& bufferOffset) = 0;
		
		/*
			This function must be called from the thread on which the OpenGL context was created on.

			\param signalEvent - pointer to the event object
			\return A ResourceLockGuard object that stores a index of the buffer for rendering. To get the index cast the pointer from GetResource() to uintMem.
			The lock guard should be unlocked when rendering is finished
		*/
		virtual ResourceLockGuard LockDynamicParticlesForRendering(void* signalEvent) = 0;
		/*
			This function must be called from the thread on which the OpenGL context was created on.
		
			\param signalEvent - pointer to the event object
			\return A ResourceLockGuard object that doesn't store anything. The lock guard should be unlocked when rendering is finished
		*/
		virtual ResourceLockGuard LockStaticParticlesForRendering(void* signalEvent) = 0;

		virtual void PrepareDynamicParticlesForRendering() = 0;
		virtual void PrepareStaticParticlesForRendering() = 0;		
	};
	//class ParticleBufferManagerRenderData : public ParticleBufferManager
	//{
	//public:
	//	/*
	//		\param index - the index of the buffer for which to get the buffer, stride, and bufferOffset
	//		\param stride - size of the data of one particle in the buffer with index 'index' in bytes
	//		\param bufferOffset - byte offset for the particle buffer with the index 'index' is set into this parameter
	//		\return The graphics buffer which holds the particle data
	//	*/
	//	virtual Graphics::OpenGLWrapper::GraphicsBuffer* GetGraphicsBuffer(uintMem index, uintMem& stride, uintMem& bufferOffset) = 0;		
	//
	//	/*
	//		This function must be called from the thread on which the OpenGL context was created on.
	//
	//		\param signalEvent - pointer to the event object
	//		\return A ResourceLockGuard object that stores a index of the buffer for rendering. To get the index cast the pointer from GetResource() to uintMem.
	//		The lock guard should be unlocked when rendering is finished
	//	*/
	//	virtual ResourceLockGuard LockForRendering(void* signalEvent) = 0;		
	//
	//	virtual void PrepareForRendering() = 0;
	//};
}