#pragma once
#include "BlazeEngine/Core/BlazeEngineCoreDefines.h"
#include "BlazeEngine/Core/Math/Vector.h"

using namespace Blaze;

namespace SPH
{
	struct DynamicParticle
	{
		Vec3f position;
		float pressure;
		Vec3f velocity;
		uint32 hash;
	};

	struct StaticParticle
	{
		Vec3f position;
		float pressure;
	};

	struct ParticleBehaviourParameters
	{
		//Particle dynamics constants
		float particleMass = 0.0f;
		float gasConstant = 0.0f;
		float elasticity = 0.0f;
		float viscosity = 0.0f;
		float gravityX = 0.0f;
		float gravityY = 0.0f;
		float gravityZ = 0.0f;

		//Particle simulation parameters
		float restDensity = 0.0f;
		float maxInteractionDistance = 0.0f;

		//Constants
		float selfDensity = 0.0f;
		float smoothingKernelConstant = 0.0f;
	};
}