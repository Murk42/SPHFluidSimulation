#include "pch.h"
#include "ParticleBufferSetRenderData.h"
#include "SPH/System/System.h"

namespace SPH
{
	//ParticleBufferSetRenderData::ParticleBufferSetRenderData(ParticleBufferSet& particleBufferSet) :
	//	particleBufferSet(particleBufferSet)
	//{
	//	particleBufferSet.waitReadFinishAction.AddCallback(WaitOnRead, this);
	//}
	//void ParticleBufferSetRenderData::InitializeVertexArray()
	//{
	//	dynamicParticleVertexArray.EnableVertexAttribute(0);
	//	dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
	//	dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);
	//
	//	dynamicParticleVertexArray.EnableVertexAttribute(1);
	//	dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
	//	dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);
	//
	//	dynamicParticleVertexArray.EnableVertexAttribute(2);
	//	dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
	//	dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &(GetDynamicParticleBufferGL()), sizeof(DynamicParticle), 0);
	//	dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);
	//}
	//void ParticleBufferSetRenderData::StartRender()
	//{
	//	particleBufferSet.waitWriteFinishAction.ExecuteAction();
	//
	//	renderStartAction.ExecuteAction();
	//}
	//void ParticleBufferSetRenderData::EndRender()
	//{
	//	renderEndAction.ExecuteAction();
	//
	//	readFinishedFence.SetFence();
	//}
	//void ParticleBufferSetRenderData::WaitOnRead(void* userData)
	//{
	//	auto& renderData = *(ParticleBufferSetRenderData*)userData;
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
	//	case Blaze::Graphics::OpenGLWrapper::FenceReturnState::FenceNotSet:
	//		break;
	//	default:
	//		//Debug::Logger::LogWarning("Client", "Invalid FenceReturnState enum value");
	//		break;
	//	}		
	//}	
}
