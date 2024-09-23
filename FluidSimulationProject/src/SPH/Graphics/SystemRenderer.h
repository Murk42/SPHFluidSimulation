#pragma once
#include "SPH/System/System.h"

namespace SPH
{
	class SystemRenderCache
	{
	public:
		virtual ~SystemRenderCache() { }

		virtual void LinkSPHSystem(System* system) = 0;
		virtual void SetModelMatrix(Mat4f modelMatrix) = 0;

		virtual System* GetSystem() const = 0;
		virtual const Mat4f& GetModelMatrix() const = 0;
	};

	class SystemRenderer
	{
	public:
		virtual ~SystemRenderer() { }

		virtual void Render(SystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix) = 0;
	private:
	};
}