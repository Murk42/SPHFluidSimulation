
#ifdef CL_COMPILER
#define Vec3i int3
#define Vec3f float3

#define modf fract

#define CONVERT_VEC3I(x) convert_int3(x)
#define CONVERT_VEC3F(x) convert_float3(x)
#define NEW_VEC3I(x, y, z) (int3)(x, y, z)
#define NEW_VEC3F(x, y, z) (float3)(x, y, z)
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
#endif


Vec3i GetCell(Vec3f position, float maxInteractionDistance)
{
    position = floor(position / maxInteractionDistance);
    return CONVERT_VEC3I(position);
}

uint GetHash(Vec3i cell)
{
    return (
        (((uint)cell.x) * 73856093) ^
        (((uint)cell.y) * 19349663) ^
        (((uint)cell.z) * 83492791)
        );
}

float SmoothingKernelConstant(float h)
{
    return 30.0f / (3.1415 * pow(h, 5));
}

float SmoothingKernelD0(float r, float maxInteractionDistance)
{
    if (r >= maxInteractionDistance)
        return 0;

    float distance = maxInteractionDistance - r;
    return distance * distance;
}

float SmoothingKernelD1(float r, float maxInteractionDistance)
{
    if (r >= maxInteractionDistance)
        return 0;

    return 2 * (r - maxInteractionDistance);
}

float SmoothingKernelD2(float r, float maxInteractionDistance)
{
    if (r >= maxInteractionDistance)
        return 0;

    return 2;
}

float Noise(float x)
{    
    float ptr = 0.0f;
    return modf(sin(x * 112.9898f) * 43758.5453f, &ptr);
}

Vec3f RandomDirection(float x)
{
    //https://math.stackexchange.com/questions/44689/how-to-find-a-random-axis-or-unit-vector-in-3d
    float theta = Noise(x) * 2 * 3.1415;
    float z = Noise(x) * 2 - 1;

    float s = sin(theta);
    float c = cos(theta);
    float z2 = sqrt(1 - z * z);
    return NEW_VEC3F(z2 * c, z2 * s, z);
}

#ifndef CL_COMPILER
#undef Vec3f
#undef Vec3i
#undef  modf
#endif
