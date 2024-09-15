#pragma once
#include "SPHSystem.h"

class SPHSystemRenderCache
{
public:
	virtual ~SPHSystemRenderCache() { }

	virtual void LinkSPHSystem(SPH::System* system) = 0;
	virtual void SetModelMatrix(Mat4f modelMatrix) = 0;

	virtual SPH::System* GetSystem() const = 0;
	virtual const Mat4f& GetModelMatrix() const = 0;
};

class SPHSystemRenderer
{
public:		
	virtual ~SPHSystemRenderer() { }

	virtual void Render(SPHSystemRenderCache& renderCache, const Mat4f& viewMatrix, const Mat4f& projMatrix) = 0;
private:	
};