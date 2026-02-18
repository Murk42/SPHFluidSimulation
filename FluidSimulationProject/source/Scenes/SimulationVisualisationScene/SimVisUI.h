#pragma once
#include "CameraControls.h"
#include "SPH/Core/SimulationEngine.h"

class SimulationVisualizationScene;

class SimVisUI :
	public UI::Screen
{
public:
	CameraMouseFocusNode cameraMouseFocusNode;

	UI::Nodes::Label titleText;
	UI::Nodes::Label controlsInfoText;
	UI::Nodes::Label simulationInfoText;
	UI::Nodes::Label runtimeInfoText;

	SimVisUI(ResourceManager& resourceManager, SimulationVisualizationScene& scene);
	~SimVisUI();

	void UpdateSimulationExecutionInfo(uintMem stepsPerUpdate, float deltaTime, float executionTime, uint FPS);
	void UpdateSimulationEngineInfo(SPH::SimulationEngine* simulationEngine);
private:
	SimulationVisualizationScene& scene;
};