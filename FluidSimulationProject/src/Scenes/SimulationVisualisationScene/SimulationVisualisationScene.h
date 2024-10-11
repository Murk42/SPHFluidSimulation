#pragma once
#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimVisUI.h"
#include "RenderingSystem.h"
#include "OpenCLContext.h"
#include "ThreadPool.h"

class SimulationVisualisationScene : public Scene
{
public:
	SimulationVisualisationScene(OpenCLContext& CLContext, RenderingSystem& renderingSystem);
	~SimulationVisualisationScene();

	void Update() override;
private:
	OpenCLContext& CLContext;
	RenderingSystem& renderingSystem;
	Window& window;

	ThreadPool threadPool;

	SPH::SystemInitParameters systemInitParameters;
	Array<std::unique_ptr<SPH::System>> SPHSystems;
	uintMem currentSPHSystemIndex;	
	SPH::SystemRenderer SPHSystemRenderer;
	SPH::SystemRenderCache SPHSystemRenderCache;
	Mat4f SPHSystemModelMatrix = Mat4f::TranslationMatrix(Vec3f(0, 0, 20));

	UI::InputManager UIInputManager;
	SimVisUI uiScreen;
	
	float cameraSpeed = 1.0f;
	Vec2f cameraAngles;
	Quatf cameraRot;
	Vec3f cameraPos = Vec3f(0, 0, 0);

	bool runSimulation = false;
	uint simulationStepsPerUpdate = 1;

	LambdaEventHandler<Input::Events::MouseScroll> mouseScrollEventHandler;	

	Stopwatch frameStopwatch;
	Stopwatch FPSStopwatch;
	Stopwatch simulationStopwatch;
	uintMem FPSCount = 0;
	bool imagingMode = false;

	void SetSystemIndex(uintMem index);
	void LoadSystemInitParameters();

	void SetupEvents();	

	void UpdateCamera(float dt);
};