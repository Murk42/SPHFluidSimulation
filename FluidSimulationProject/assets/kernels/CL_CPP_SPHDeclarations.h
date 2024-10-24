#pragma once

#ifdef CL_COMPILER
#define Vec3i int3
#define Vec3f float3
#define HASH_TYPE uint

#define modf fract

#define CONVERT_VEC3I(x) convert_int3(x)
#define CONVERT_VEC3F(x) convert_float3(x)
#define NEW_VEC3I(x, y, z) (int3)(x, y, z)
#define NEW_VEC3F(x, y, z) (float3)(x, y, z)

struct __attribute__((packed)) DynamicParticle
{
	float4 positionAndPressure;
	float4 velocityAndHash;
};
struct __attribute__((packed)) StaticParticle
{
	float4 positionAndPressure;
};
#define GLOBAL global
#define STRUCT struct
#define xyz() xyz
#else
static Vec3f floor(const Vec3f x)
{
	return {
		std::floor(x.x),
		std::floor(x.y),
		std::floor(x.z)
	};
}

#define CONVERT_VEC3I(x) Vec3i(x)
#define CONVERT_VEC3F(x) Vec3f(x)
#define NEW_VEC3I(x, y, z) Vec3i(x, y, z)
#define NEW_VEC3F(x, y, z) Vec3f(x, y, z)
#define HASH_TYPE std::atomic_uint32_t

struct DynamicParticle
{
	union {
		struct {
			Vec3f position;
			float pressure;
		};
		struct {
			Vec4f positionAndPressure;
		};
	};
	union {
		struct {
			Vec3f velocity;
			uint32 hash;
		};
		struct {
			Vec4f velocityAndHash;
		};
	};

	DynamicParticle() :
		position(), pressure(), velocity(), hash()
	{
	}
	DynamicParticle(const DynamicParticle& o) :
		position(o.position), pressure(o.pressure), velocity(o.velocity), hash(o.hash)
	{
	}
	DynamicParticle& operator=(const DynamicParticle& o)
	{
		position = o.position;
		pressure = o.pressure;
		velocity = o.velocity;
		hash = o.hash;
		return *this;
	}
};
struct StaticParticle
{
	union {
		struct {
			Vec3f position;
			float pressure;
		};
		struct {
			Vec4f positionAndPressure;
		};

	};

	StaticParticle() :
		position(), pressure()
	{
	}
	StaticParticle(const StaticParticle& o) :
		position(o.position), pressure(o.pressure)
	{

	}
	StaticParticle& operator=(const StaticParticle& o)
	{
		position = o.position;
		pressure = o.pressure;	
		return *this;
	}
};

#define GLOBAL
#define STRUCT

#define dot(x, y) x.DotProduct(y)
#define atomic_inc(x) ((*(x))++)
#define atomic_dec(x) ((*(x))--)
#endif

struct ParticleBehaviourParameters
{
	//Particle dynamics constants
	float particleMass;
	float gasConstant;
	float elasticity;
	float viscosity;
	float gravityX;
	float gravityY;
	float gravityZ;	

	//Particle simulation parameters		
	float restDensity;
	float maxInteractionDistance;

#ifndef CL_COMPILER
	Vec3f dummy1;
#endif
};
struct ParticleBoundParameters
{	
	Vec3f boxOffset;
#ifndef CL_COMPILER
	float dummy1;
#endif
	Vec3f boxSize;
#ifndef CL_COMPILER
	float dummy2;
#endif
	float wallElasticity;
	float floorAndRoofElasticity;
	bool bounded;
	bool boundedByRoof;
	bool boundedByWalls;
	bool dummy4;
#ifndef CL_COMPILER
	float dummy3;
#endif
};
struct ParticleSimulationParameters
{
	STRUCT ParticleBehaviourParameters behaviour;
	STRUCT ParticleBoundParameters bounds;
	uint dynamicParticleCount;
	uint dynamicParticleHashMapSize;
	uint staticParticleCount;
	uint staticParticleHashMapSize;

	float selfDensity;
	float smoothingKernelConstant;
};