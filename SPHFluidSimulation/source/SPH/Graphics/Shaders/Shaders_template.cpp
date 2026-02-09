#include "Shaders.h"

namespace SPH::ShaderSource
{
	static const char particle_frag_str[] = { $bytes "particle.frag"$, '\0' };
	const Blaze::StringView particle_frag = Blaze::StringView(particle_frag_str);
	static const char particle_vert_str[] = { $bytes "particle.vert"$, '\0' };
	const Blaze::StringView particle_vert = Blaze::StringView(particle_vert_str);
}