#pragma once
#include "Scenes/Scene.h"
#include "OpenCLContext.h"
#include "RenderingSystem.h"

#include "ThreadPool.h"

#include "ProfilingUI.h"

class ProfilingScene : public Scene
{
public:
	ProfilingScene(OpenCLContext& CLContext, RenderingSystem& renderingSystem);
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

	OpenCLContext& CLContext;
	RenderingSystem& renderingSystem;
	Window& window;

	ThreadPool threadPool;

	Array<std::shared_ptr<SPH::System>> SPHSystems;	

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