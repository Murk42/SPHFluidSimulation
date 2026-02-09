#pragma once
#include "CameraControls.h"

class SimulationVisualisationScene;

class SimVisUI :
	public UI::Screen
{
public:
	CameraMouseFocusNode cameraMouseFocusNode;

	UI::Nodes::Label titleText;
	UI::Nodes::Label infoText;

	//uintMem hashCounter;
	UI::Nodes::Label hashCounterText;

	SimVisUI(ResourceManager& resourceManager, SimulationVisualisationScene& scene);
	~SimVisUI();

	void Update();

	void SetFPS(uintMem FPS);
	void SetParticleCount(uintMem particleCount);
	void SetInfo(u8String text);
	void SetImplenetationName(u8String implementationName) { this->implementationName = implementationName; ReconstructInfoText(); }
private:
	SimulationVisualisationScene& scene;

	uint FPS;
	uint particleCount;
	u8String info;
	u8String implementationName;

	void ReconstructInfoText();
	void Event_PostInputUpdate(const Input::InputPostUpdateEvent&);
};