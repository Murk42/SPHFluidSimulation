#pragma once

class Scene
{
public:
	virtual ~Scene() { }

	virtual void Update() = 0;
};