#version 430

in vec4 f_color;
in vec3 f_center;
in vec3 f_pos;
in float f_radiusSqr;
in vec3 f_worldPos;

float sqrDistance(vec3 a, vec3 b)
{
	a = a - b;
	return dot(a, a);
}

out vec4 o_color;

void main()
{
	if (sqrDistance(f_center, f_pos) > f_radiusSqr || f_color.a == 0)
		discard;

	o_color = vec4(f_color);
}