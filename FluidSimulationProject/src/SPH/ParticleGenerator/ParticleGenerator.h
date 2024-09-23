#pragma once


namespace SPH
{
	template<typename T>
	concept PositionedParticle = requires { true; };// requires { std::same_as<decltype((T::position)), Vec3f&>; };

	template<PositionedParticle T>
	class ParticleGenerator
	{
	public:
		virtual void Generate(Array<T>& particles) = 0;
	};
}