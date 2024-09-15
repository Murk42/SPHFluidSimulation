#version 430

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec4 i_color;

layout(location = 0) uniform mat4 u_modelView;
layout(location = 1) uniform mat4 u_proj;

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

void main()
{
	vec3 center = (u_modelView * vec4(i_pos, 1)).xyz;	
	float dist = length(center);
	vec3 toCamera = -center / dist;
	vec3 up = vec3(0, 1, 0);	
	vec3 right = cross(toCamera, up);

	up *= 0.05f;
	right *= 0.05f;

	vec3 pos = center;

	if (gl_VertexID == 0)	
		pos += +up -right;
	if (gl_VertexID == 1)	
		pos += +up +right;
	if (gl_VertexID == 2)	
		pos += -up -right;
	if (gl_VertexID == 3)	
		pos += -up +right;			
	
	f_color = i_color;
	f_center = center;
	f_radiusSqr = sqrDistance(center, center + right);
	f_pos = pos;
	f_worldPos = i_pos;

	gl_Position = u_proj * vec4(pos, 1);
}