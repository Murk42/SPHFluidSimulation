#define Vec3i int3
#define Vec3f float3
#define Vec3u uint3
#define HASH_TYPE uint
#define uint32 uint
#define uint64 ulong
#define uintMem size_t


#define modf fract

#define NEW_VEC3I(x, y, z) (int3)(x, y, z)
#define NEW_VEC3U(x, y, z) (uint3)(x, y, z)
#define NEW_VEC3F(x, y, z) (float3)(x, y, z)
#define CONVERT_VEC3I(x) convert_int3(x)
#define CONVERT_VEC3F(x) convert_float3(x)

#define GLOBAL global
#define CONSTANT constant
#define STRUCT struct
#define xyz() xyz
#define KERNEL kernel
#define PACKED __attribute__((packed))
#define Vec4f float4
#define CONSTEXPR inline

#define INITIALIZE_THREAD_ID() threadID = get_global_id(0)
