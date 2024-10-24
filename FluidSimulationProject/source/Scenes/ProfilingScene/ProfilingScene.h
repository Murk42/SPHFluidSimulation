#pragma once
#include "Scenes/Scene.h"
#include "OpenCLContext.h"
#include "RenderingSystem.h"

#include "ThreadPool.h"

#include "ProfilingUI.h"

#include "SPH/ParticleBufferSet/RenderableCPUParticleBufferSet.h"
#include "SPH/ParticleBufferSet/RenderableGPUParticleBufferSet.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

class ProfilingScene : public Scene
{
public:
	ProfilingScene(OpenCLContext& CLContext, cl::CommandQueue& clQueue, ThreadPool& threadPool, RenderingSystem& renderingSystem);
	~ProfilingScene();

	void Update() override;
private:
	struct Profile 
	{
		SPH::SystemInitParameters systemInitParameters;
		String name;
		float simulationDuration;
		float simulationStepTime;
		uint stepsPerUpdate;
		Path outputFilePath;
	};
	struct SPHSystemData
	{
		SPH::System& system;
		SPH::ParticleBufferSet& particleBufferSet;
	};

	OpenCLContext& clContext;
	RenderingSystem& renderingSystem;
	Window& window;

	ThreadPool& threadPool;

	SPH::RenderableGPUParticleBufferSet GPUParticleBufferSet;
	SPH::SystemGPU SPHSystemGPU;
	SPH::RenderableCPUParticleBufferSet CPUParticleBufferSet;
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
	File outputFile;	

	void LoadProfiles();

	void SetupEvents();

	void WriteToOutputFile(StringView s);

	void StartProfiling();
	void UpdateProfilingState();
	void StopProfiling();	
};