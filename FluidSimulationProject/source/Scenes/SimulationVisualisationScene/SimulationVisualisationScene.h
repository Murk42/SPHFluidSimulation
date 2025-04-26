#pragma once
#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimVisUI.h"
#include "RenderingSystem.h"
#include "CameraControls.h"
#include "OpenCLContext.h"
#include "SPH/ParticleBufferManager/RenderableCPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/RenderableGPUParticleBufferManager.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"
#include "SPH/Scene/Scene.h"

class SimulationVisualisationScene : 
	public Scene,
	EventHandler<Keyboard::KeyDownEvent>
{
public:	
	struct SPHSystemData
	{
		SPH::System& system;
		SPH::ParticleBufferManagerRenderData& particleBufferManager;
	};

	OpenCLContext& clContext;	
	cl_command_queue clCommandQueue;
	RenderingSystem& renderingSystem;
	Window& window;	

	SPH::Scene simulationScene;	

	SPH::RenderableGPUParticleBufferManagerWithoutCLGLInterop GPUParticleBufferManager;
	SPH::SystemGPU SPHSystemGPU;
	SPH::RenderableCPUParticleBufferManager CPUParticleBufferManager;
	SPH::SystemCPU SPHSystemCPU;
	Array<SPHSystemData> SPHSystems;

	uintMem currentSPHSystemIndex;

	SPH::SystemRenderer SPHSystemRenderer;
	SPH::SystemRenderCache SPHSystemRenderCache;
	Mat4f SPHSystemModelMatrix = Mat4f::TranslationMatrix(Vec3f(0, 0, 20));

	UI::InputManager UIInputManager;
	SimVisUI uiScreen;
	
	CameraControls cameraControls;

	bool runSimulation = false;
	bool stepSimulation = false;	

	Vec3f imagingCameraPos;
	Vec2f imagingCameraAngles;	

	Stopwatch frameStopwatch;
	Stopwatch FPSStopwatch;
	Stopwatch simulationStopwatch;
	uintMem FPSCount = 0;
	bool imagingMode = false;

	SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clCommandQueue, RenderingSystem& renderingSystem);
	~SimulationVisualisationScene();

	void Update() override;

	void OnEvent(const Keyboard::KeyDownEvent& event) override;

	void InitializeSystemAndSetAsCurrent(uintMem index);
};