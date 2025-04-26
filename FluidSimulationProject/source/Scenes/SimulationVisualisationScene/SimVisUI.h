#pragma once
#include "CameraControls.h"

class EditableText : public UI::Node, Graphics::RenderObject
{
public:
	UI::PanelRenderUnit panelRenderUnit;
	UI::Nodes::EditableText editableText;

	LambdaEventHandler<UI::Node::TransformUpdatedEvent> transformChangedEventHandler;

	EditableText();
private:
	Graphics::RenderUnit* GetRenderUnit(uint index) override;
};

class SimulationVisualisationScene;

class SimVisUI :
	public UI::Screen
{	
public:
	Font font = Font::LoadDefault();	
	CameraMouseFocusNode cameraMouseFocusNode;
	UI::Nodes::Text titleText;
	UI::Nodes::Text infoText;		

	uintMem hashCounter;
	UI::Nodes::Text hashCounterText;

	SimVisUI(SimulationVisualisationScene& scene);
	~SimVisUI();

	void SetFPS(uintMem FPS);
	void SetParticleCount(uintMem particleCount);
	void SetInfo(StringUTF8 text);
	void SetImplenetationName(StringUTF8 implementationName) { this->implementationName = implementationName; ReconstructInfoText(); }
private:
	SimulationVisualisationScene& scene;

	uint FPS;
	uint particleCount;
	StringUTF8 info;
	StringUTF8 implementationName;

	void ReconstructInfoText();
	void Event_PostInputUpdate(const Input::InputPostUpdateEvent&);
};