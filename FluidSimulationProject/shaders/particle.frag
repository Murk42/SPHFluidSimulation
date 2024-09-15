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

void main()
{
	if (sqrDistance(f_center, f_pos) > f_radiusSqr)
		discard;

	gl_FragColor = vec4(f_color);
}