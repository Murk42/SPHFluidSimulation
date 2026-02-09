#version 430

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_velocity;
layout(location = 2) in float i_pressure;
layout(location = 3) in uint i_hash;
layout(location = 4) in uint i_color;

layout(location = 0) uniform mat4 u_projMatrix;
layout(location = 1) uniform mat4 u_viewMatrix;
layout(location = 2) uniform mat4 u_modelMatrix;
layout(location = 3) uniform float u_size;
layout(location = 4) uniform vec4 u_color;

out vec4 f_color;
out vec3 f_center;
out vec3 f_pos;
out float f_radiusSqr;
out vec3 f_worldPos;

float sqrDistance(vec3 a, vec3 b)
{
	a = a - b;
	return dot(a, a);
}

uint ExpandBits(uint v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}
uint Morton3D(uvec3 value)
{
	//center so that 0, 0, 0 is in the center of the morton cube
	//value = value + ivec3(512, 512, 512);

	uint xx = ExpandBits(uint(value.x) & 0x000003FFu);
	uint yy = ExpandBits(uint(value.y) & 0x000003FFu);
	uint zz = ExpandBits(uint(value.z) & 0x000003FFu);
	return xx + yy * 2 + zz * 4;
}
uvec3 GetCell(vec3 position, float maxInteractionDistance)
{
	return uvec3(ivec3(floor(position / maxInteractionDistance)));
}
uint GetHash(uvec3 cell)
{
	return Morton3D(cell);
}

 float Noise(float x)
{
	float ptr = 0.0f;
	return modf(sin(x * 112.9898f) * 43758.5453f, ptr);
}
vec3 RandomColor(uint x)
{
	return vec3(Noise((x % 1094)), Noise((x % 4194) + 143), Noise((x % 125) + 829));
}

#define FADE_START 5
#define FADE_END 4.0

void main()
{
	vec3 center = (u_viewMatrix * u_modelMatrix * vec4(i_pos, 1)).xyz;
	float dist = length(center);
	vec3 toCamera = -center / dist;
	vec3 up = vec3(0, 1, 0);
	vec3 right = cross(toCamera, up);

	up *= u_size;
	right *= u_size;

	vec3 pos = center;

	if (gl_VertexID == 0)
		pos += +up -right;
	if (gl_VertexID == 1)
		pos += +up +right;
	if (gl_VertexID == 2)
		pos += -up -right;
	if (gl_VertexID == 3)
		pos += -up +right;


	vec4 color;
	if (i_color == 0)
	{
		//uint hash = GetHash(GetCell(i_pos, 1.0f));
		//uint hashMod = hash;
		//color = vec4(RandomColor(GetHash(GetCell(i_pos, 1.0f))), 1.0f);
		//color = vec4(vec3(float(hash % 8) / 8.0f), 1.0f);

		float colorCoef = clamp(1 - exp(-i_pressure / 2000), 0, 1);

		color = mix(u_color, vec4(1.0f, 0.0f, 0.0f, 1.0f), colorCoef);
	}
	else if (i_color == 1)
		color = vec4(0, 0, 1, 1);
	else if (i_color == 2)
		color = vec4(0, 1, 0, 1);
	else
		color = vec4(1, 0, 0, 1);

	color.a *= (1 - smoothstep(float(FADE_START), FADE_END, dist));

	f_color = color;
	f_center = center;
	f_radiusSqr = sqrDistance(center, center + right);
	f_pos = pos;
	f_worldPos = i_pos;

	gl_Position = u_projMatrix * vec4(pos, 1);
}