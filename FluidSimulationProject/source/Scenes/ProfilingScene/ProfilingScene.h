#pragma once
#include "Scenes/Scene.h"
#include "OpenCLContext.h"
#include "RenderingSystem.h"

#include "ThreadPool.h"

#include "ProfilingUI.h"

#include "SPH/ParticleBufferSet/OfflineCPUParticleBufferSet.h"
#include "SPH/ParticleBufferSet/OfflineGPUParticleBufferSet.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

class ProfilingScene : public Scene
{
public:
	ProfilingScene(OpenCLContext& CLContext, cl_command_queue clQueue, ThreadPool& threadPool, RenderingSystem& renderingSystem);
	~ProfilingScene();

	void Update() override;
private:
	struct Profile 
	{
		SPH::SystemParameters systemInitParameters;
		String name;
		float simulationDuration;
		float simulationStepTime;
		uint stepsPerUpdate;
		Path outputFilePath;
		File outputFile;
	};
	struct SPHSystemData
	{
		SPH::System& system;
		SPH::ParticleBufferSet& particleBufferSet;		

		Array<SPH::SystemProfilingData> profilingData;
	};

	OpenCLContext& clContext;
	RenderingSystem& renderingSystem;
	Window& window;

	ThreadPool& threadPool;

	SPH::OfflineGPUParticleBufferSet GPUParticleBufferSet;
	SPH::SystemGPU SPHSystemGPU;
	SPH::OfflineCPUParticleBufferSet CPUParticleBufferSet;
	SPH::SystemCPU SPHSystemCPU;

	Array<SPHSystemData> SPHSystems;	

	UI::InputManager UIInputManager;
	ProfilingUI uiScreen;	

	LambdaEventHandler<TextButton::PressedEvent> startProfilingButtonPressedEventHandler;

	Array<Profile> profiles;

	bool profiling = false;
	uintMem profileIndex = 0;
	uintMem systemIndex = 0;
	uintMem currentUpdate = 0;		

	void LoadProfiles();

	void SetupEvents();

	void StartProfiling();
	void UpdateProfilingState();
	void StopProfiling();	

	void NewProfileStarted();
	void NewSystemStarted();
	void ProfileFinished();
	void SystemFinished();
};