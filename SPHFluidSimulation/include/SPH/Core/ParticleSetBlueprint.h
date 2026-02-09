#pragma once

namespace SPH
{	
	class ParticleSetBlueprint
	{
	public:
		template<typename T>
		using WriteFunction = void(*)(uintMem index, const T& value, void* userData);

		virtual ~ParticleSetBlueprint() { }
		virtual void Load(StringView string) = 0;

		virtual uintMem GetParticleCount() const = 0;
		virtual void WriteParticlesPositions(const WriteFunction<Vec3f> writeFunction, void* userData) const = 0;
	private:
	};
}