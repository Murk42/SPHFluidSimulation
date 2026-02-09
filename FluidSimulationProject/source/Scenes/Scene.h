#pragma once

class SceneBlueprint
{
public:
	virtual ~SceneBlueprint() { }

	virtual void Update() = 0;
	virtual void Render(const Graphics::RenderContext& renderContext) = 0;
	virtual void OnEvent(const Input::GenericInputEvent& event) = 0;
};