#include "pch.h"
#include "SPH/ParticleBufferManager/ParticleBufferManagerRenderData.h"
#include "SPH/System/System.h"

namespace SPH
{
	//ParticleBufferManagerRenderData::ParticleBufferManagerRenderData(ParticleBufferManager& particleBufferManager) :
	//	particleBufferManager(particleBufferManager)
	//{
	//	particleBufferManager.waitReadFinishAction.AddCallback(WaitOnRead, this);
	//}
	//void ParticleBufferManagerRenderData::InitializeVertexArray()
	//{
	//	dynamicParticleVertexArray.EnableVertexAttribute(0);
	//	dynamicParticleVertexArray.ManagerVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
	//	dynamicParticleVertexArray.ManagerVertexAttributeBuffer(0, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.ManagerVertexAttributeDivisor(0, 1);
	//
	//	dynamicParticleVertexArray.EnableVertexAttribute(1);
	//	dynamicParticleVertexArray.ManagerVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
	//	dynamicParticleVertexArray.ManagerVertexAttributeBuffer(1, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.ManagerVertexAttributeDivisor(1, 1);
	//
	//	dynamicParticleVertexArray.EnableVertexAttribute(2);
	//	dynamicParticleVertexArray.ManagerVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
	//	dynamicParticleVertexArray.ManagerVertexAttributeBuffer(2, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.ManagerVertexAttributeDivisor(2, 1);
	//}
	//void ParticleBufferManagerRenderData::StartRender()
	//{
	//	particleBufferManager.waitWriteFinishAction.ExecuteAction();
	//
	//	renderStartAction.ExecuteAction();
	//}
	//void ParticleBufferManagerRenderData::EndRender()
	//{
	//	renderEndAction.ExecuteAction();
	//
	//	readFinishedFence.ManagerFence();
	//}
	//void ParticleBufferManagerRenderData::WaitOnRead(void* userData)
	//{
	//	auto& renderData = *(ParticleBufferManagerRenderData*)userData;
	//
	//	auto fenceState = renderData.readFinishedFence.BlockClient(2);		
	//
	//	switch (fenceState)
	//	{
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
	//		break;
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:			
	//		renderData.readFinishedFence = Graphics::OpenGLWrapper::Fence();
	//		break;
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::TimeoutExpired:
	//		Debug::Logger::LogWarning("Client", "System simulation fence timeout");
	//		break;
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::Error:
	//		Debug::Logger::LogWarning("Client", "System simulation fence error");
	//		break;
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::FenceNotManager:
	//		break;
	//	default:
	//		//Debug::Logger::LogWarning("Client", "Invalid FenceReturnState enum value");
	//		break;
	//	}		
	//}	
}
