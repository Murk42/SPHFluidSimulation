#version 430

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_velocity;
layout(location = 2) in float i_pressure;
layout(location = 3) in uint i_color;

layout(location = 0) uniform mat4 u_modelView;
layout(location = 1) uniform mat4 u_proj;
layout(location = 2) uniform float u_size;
layout(location = 3) uniform vec4 u_color;

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

#define FADE_START 5
#define FADE_END 4

void main()
{
	vec3 center = (u_modelView * vec4(i_pos, 1)).xyz;	
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

		float colorCoef = clamp(1 - exp(-i_pressure / 2000), 0, 1);
		
		color = mix(u_color, vec4(1.0f, 0.0f, 0.0f, 1.0f), colorCoef);	
	}
	else if (i_color == 1)
		color = vec4(0, 0, 1, 1);
	else if (i_color == 2)
		color = vec4(0, 1, 0, 1);
	else
		color = vec4(1, 0, 0, 1);
	
	color.a *= (1 - smoothstep(FADE_START, FADE_END, dist));

	f_color = color;
	f_center = center;
	f_radiusSqr = sqrDistance(center, center + right);
	f_pos = pos;
	f_worldPos = i_pos;

	gl_Position = u_proj * vec4(pos, 1);
}