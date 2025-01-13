#pragma once
#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimVisUI.h"
#include "RenderingSystem.h"
#include "OpenCLContext.h"
#include "ThreadPool.h"
#include "SPH/ParticleBufferSet/RenderableCPUParticleBufferSet.h"
#include "SPH/ParticleBufferSet/RenderableGPUParticleBufferSet.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"
#include "SPH/Scene/Scene.h"

class SimulationVisualisationScene : public Scene
{
public:
	SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clQueue, ThreadPool& threadPoo, RenderingSystem& renderingSystem);
	~SimulationVisualisationScene();

	void Update() override;
private:	
	struct SPHSystemData
	{
		SPH::System& system;
		SPH::ParticleBufferSet& particleBufferSet;
	};

	OpenCLContext& clContext;	
	cl_command_queue clQueue;
	RenderingSystem& renderingSystem;
	Window& window;

	ThreadPool& threadPool;

	SPH::Scene simulationScene;	

	SPH::RenderableGPUParticleBufferSet GPUParticleBufferSet;
	SPH::SystemGPU SPHSystemGPU;
	SPH::RenderableCPUParticleBufferSet CPUParticleBufferSet;
	SPH::SystemCPU SPHSystemCPU;
	Array<SPHSystemData> SPHSystems;

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

	Vec3f imagingCameraPos;
	Vec2f imagingCameraAngles;

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