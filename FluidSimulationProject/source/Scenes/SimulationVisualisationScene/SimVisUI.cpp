#include "pch.h"
#include "SimVisUI.h"
#include "SimulationVisualisationScene.h"

SimVisUI::SimVisUI(ResourceManager& resourceManager, SimulationVisualizationScene& scene)
	: Screen(resourceManager), scene(scene)
{
	titleText.SetParent(this);
	titleText.SetTransform({ .pos = Vec2f(0, -5), .parentPivot = Vec2f(0.5f, 1.0f), .pivot = Vec2f(0.5f, 1.0f), });
	titleText.SetText("Simulation visualization");
	titleText.SetTextStyle({ .fontName = "default", .fontHeight = 32 });	

	controlsInfoText.SetParent(this);
	controlsInfoText.SetTransform({ .pos = Vec2f(2, 2), .parentPivot = Vec2f(0, 0), .pivot = Vec2f(0, 0), });
	controlsInfoText.SetTextStyle({ .fontName = "default", .fontHeight = 12, .color = 0xf5f5f580 });
	controlsInfoText.SetText(
		"T - change simulation engines\n"
		"I - toggle imaging mode\n"
		"X - start/stop simulation\n"
		"Shift+X - step simulation\n"
		"R - reset simulation\n"
		"\n"
		"Click on the screen to rotate camera.\n"
		"Move with W, A, S, D.\n"
		"Adjust speed with the scroll - wheel.\n"
		"Press TAB to switch scenes.\n"
	);

	simulationInfoText.SetParent(this);
	simulationInfoText.SetTransform({ .pos = Vec2f(2, 0), .parentPivot = Vec2f(0, 0.8), .pivot = Vec2f(0, 0), });
	simulationInfoText.SetTextStyle({ .fontName = "default", .fontHeight = 16 });

	runtimeInfoText.SetParent(&simulationInfoText);
	runtimeInfoText.SetTransform({ .pos = Vec2f(2, -30), .parentPivot = Vec2f(0, 0.0), .pivot = Vec2f(0, 1), });
	runtimeInfoText.SetTextStyle({ .fontName = "default", .fontHeight = 16 });

	cameraMouseFocusNode.SetParent(this);
	scene.cameraControls.SetAsTargetNode(&cameraMouseFocusNode);
}

SimVisUI::~SimVisUI()
{
}

void SimVisUI::UpdateSimulationExecutionInfo(uintMem stepsPerUpdate, float deltaTime, float executionTime, uint FPS)
{
	u8String text;

	text += Format(
		"Steps per update: {}\n"
		"Delta time: {: 4.1}ms\n"
		"Execution time: {: 4.1}ms\n",
		stepsPerUpdate, deltaTime * 1000.0f, executionTime * 1000.0f, scene.FPS
	);
	text += Format("\nFPS: {: 4}", FPS);

	runtimeInfoText.SetText(text);
}

void SimVisUI::UpdateSimulationEngineInfo(SPH::SimulationEngine* simulationEngine)
{
	u8String text;

	if (simulationEngine != nullptr)
	{
		text += Format("Simulation engine: {}", simulationEngine->SystemImplementationName());
		if (auto manager = simulationEngine->GetDynamicParticlesBufferManager())
			text += Format("\nDynamic particles: {}", manager->GetParticleCount());
		else
			text += Format("\nDynamic particles: No dynamic particle buffer manager");

		if (auto manager = simulationEngine->GetStaticParticlesBufferManager())
			text += Format("\nStatic particles: {}", manager->GetParticleCount());
		else
			text += Format("\nStatic particles: No static particle buffer manager");
	}
	else
	{
		text += "No simulation engine set";
	}

	simulationInfoText.SetText(text);
}