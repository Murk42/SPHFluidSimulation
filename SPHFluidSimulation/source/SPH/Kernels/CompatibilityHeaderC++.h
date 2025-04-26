#pragma once
#include "BlazeEngineCore/BlazeEngineCoreDefines.h"
#include "BlazeEngineCore/Math/Vector.h"
using namespace Blaze;

#include <cmath>
#include <atomic>

#define NEW_VEC3I(x, y, z) Vec3i(x, y, z)
#define NEW_VEC3U(x, y, z) Vec3u(x, y, z)
#define NEW_VEC3F(x, y, z) Vec3f(x, y, z)
#define CONVERT_VEC3I(x) Vec3i(x)
#define CONVERT_VEC3I_RTN(x )
#define CONVERT_VEC3F(x) Vec3f(x)
#define HASH_TYPE std::atomic_uint32_t

#define GLOBAL
#define CONSTANT const
#define STRUCT
#define KERNEL
#define PACKED
#define CONSTEXPR constexpr

#define dot(x, y) x.DotProduct(y)
#define atomic_inc(x) ((*(x))++)
#define atomic_dec(x) ((*(x))--)