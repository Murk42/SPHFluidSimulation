#pragma once
#include "Scenes/Scene.h"
#include "OpenCLContext.h"
#include "RenderingSystem.h"
#include "ProfilingUI.h"

#include "SPH/ParticleBufferManager/OfflineCPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/OfflineGPUParticleBufferManager.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

class ProfilingScene : public Scene
{
public:
	ProfilingScene(OpenCLContext& CLContext, cl_command_queue clQueue, RenderingSystem& renderingSystem);
	~ProfilingScene();

	void Update() override;

	void StartProfiling();
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
		SPH::ParticleBufferManager& particleBufferSet;		

		//Array<SPH::SystemProfilingData> profilingData;
	};

	OpenCLContext& clContext;
	RenderingSystem& renderingSystem;
	Window& window;	

	SPH::OfflineGPUParticleBufferManager GPUParticleBufferManager;
	SPH::SystemGPU SPHSystemGPU;
	SPH::OfflineCPUParticleBufferManager CPUParticleBufferManager;
	SPH::SystemCPU SPHSystemCPU;

	Array<SPHSystemData> SPHSystems;	

	UI::InputManager UIInputManager;
	ProfilingUI uiScreen;		

	Array<Profile> profiles;

	bool profiling = false;
	uintMem profileIndex = 0;
	uintMem systemIndex = 0;
	uintMem currentUpdate = 0;		

	void LoadProfiles();

	void UpdateProfilingState();
	void StopProfiling();	

	void NewProfileStarted();
	void NewSystemStarted();
	void ProfileFinished();
	void SystemFinished();
};