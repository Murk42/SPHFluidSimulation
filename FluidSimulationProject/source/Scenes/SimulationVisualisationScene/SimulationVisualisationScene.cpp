#include "pch.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"

#include "SPH/ParticleGenerator/FilledBoxParticleGenerator.h"
#include "SPH/ParticleGenerator/BoxShellParticleGenerator.h"

SimulationVisualisationScene::SimulationVisualisationScene(OpenCLContext& clContext, cl::CommandQueue& clQueue, ThreadPool& threadPool, RenderingSystem& renderingSystem) :
	clContext(clContext), clQueue(clQueue), threadPool(threadPool), renderingSystem(renderingSystem), window(renderingSystem.GetWindow()), SPHSystemRenderer(renderingSystem.GetGraphicsContext()), currentSPHSystemIndex(0),
	SPHSystemGPU(clContext, clQueue, renderingSystem.GetGraphicsContext()), SPHSystemCPU(threadPool), GPUParticleBufferSet(clContext, clQueue)
{		
	//Setup UI	
	uiScreen.SetWindow(&window);	

	UIInputManager.SetScreen(&uiScreen);

	SetupEvents();
			
	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUParticleBufferSet });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUParticleBufferSet });

	SetSystemIndex(0);
	LoadSystemInitParameters();
	SPHSystems[currentSPHSystemIndex].system.Initialize(systemInitParameters, SPHSystems[currentSPHSystemIndex].particleBufferSet);

	renderingSystem.SetScreen(&uiScreen);	
	renderingSystem.SetSPHSystemRenderer(&SPHSystemRenderer);
	renderingSystem.SetSPHSystemRenderingCache(&SPHSystemRenderCache);	
}


SimulationVisualisationScene::~SimulationVisualisationScene()
{
	renderingSystem.SetScreen(nullptr);
	renderingSystem.SetSPHSystemRenderer(nullptr);
	renderingSystem.SetSPHSystemRenderingCache(nullptr);	
}

void SimulationVisualisationScene::Update()
{
	float dt = frameStopwatch.Reset();	
	
	UpdateCamera(dt);
	renderingSystem.SetViewMatrix(Mat4f::RotationMatrix(cameraRot.Conjugated()) * Mat4f::TranslationMatrix(-cameraPos));

	if (UIInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode || UIInputManager.GetSelectedNode() == nullptr)
	{
		if (window.GetLastKeyState(Key::X).pressed)
			if (window.GetLastKeyState(Key::LShift).down)
				runSimulation = false;
			else
				runSimulation = true;

		if (window.GetLastKeyState(Key::T).pressed)
		{
			SetSystemIndex((currentSPHSystemIndex + 1) % SPHSystems.Count());									
			SPHSystems[currentSPHSystemIndex].system.Initialize(systemInitParameters, SPHSystems[currentSPHSystemIndex].particleBufferSet);
			runSimulation = false;
		}			

		if (window.GetLastKeyState(Key::R).pressed)
		{
			LoadSystemInitParameters();
			SPHSystems[currentSPHSystemIndex].system.Initialize(systemInitParameters, SPHSystems[currentSPHSystemIndex].particleBufferSet);
		}

		if (window.GetLastKeyState(Key::I).pressed)
		{
			imagingMode = !imagingMode;
			if (imagingMode)
			{
				renderingSystem.SetCustomClearColor(0xffffffff);
				SPHSystemRenderer.SetDynamicParticleColor(0x000000ff);
				SPHSystemRenderer.SetStaticParticleColor(0xff0000ff);
				renderingSystem.SetScreen(nullptr);
				cameraPos = Vec3f(0, 3, 10);
				cameraAngles = Vec2f(Math::PI / 7, 0);
			}
			else
			{
				renderingSystem.DisableCustomClearColor();
				SPHSystemRenderer.SetDynamicParticleColor(0xffffffff);
				SPHSystemRenderer.SetStaticParticleColor(0xff0000ff);
				renderingSystem.SetScreen(&uiScreen);
			}
		}
	}

	bool updated = false;

	if ((runSimulation || window.GetLastKeyState(Key::Right).pressed && (UIInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode || UIInputManager.GetSelectedNode() == nullptr)))
	{
		SPHSystems[currentSPHSystemIndex].system.Update(0.01f, simulationStepsPerUpdate);		
	}	

	String info;
	info += "Simulation elapsed time: " + StringParsing::Convert(SPHSystems[currentSPHSystemIndex].system.GetSimulationTime()) + "s\n";
	info += "Steps per update: " + StringParsing::Convert(simulationStepsPerUpdate) + "\n";

	uiScreen.SetInfo(info);

	if (UIInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode)
		UpdateCamera(dt);

	renderingSystem.Render();

	++FPSCount;
	if (FPSStopwatch.GetTime() > 1.0f)
	{
		uiScreen.SetFPS(FPSCount);
		FPSStopwatch.Reset();
		FPSCount = 0;
	}
}

void SimulationVisualisationScene::SetSystemIndex(uintMem index)
{
	SPHSystems[currentSPHSystemIndex].system.Clear();
	currentSPHSystemIndex = std::min(index, SPHSystems.Count() - 1);
	SPHSystemRenderCache.LinkSPHSystem(&SPHSystems[currentSPHSystemIndex].system, dynamic_cast<SPH::ParticleBufferSetRenderData&>(SPHSystems[currentSPHSystemIndex].particleBufferSet));
	SPHSystemRenderCache.SetModelMatrix(Mat4f::TranslationMatrix(Vec3f(0, 0, 20)));
	uiScreen.SetImplenetationName(SPHSystems[currentSPHSystemIndex].system.SystemImplementationName());
}

void SimulationVisualisationScene::LoadSystemInitParameters()
{
	File jsonFile{ "assets/simulationProfiles/systemVisualisationProfile.json", FileAccessPermission::Read };
	std::string jsonFileString;
	jsonFileString.resize(jsonFile.GetSize());
	jsonFile.Read(jsonFileString.data(), jsonFileString.size());
	systemInitParameters.ParseJSON(JSON::parse(jsonFileString));
}

void SimulationVisualisationScene::SetupEvents()
{	
	mouseScrollEventHandler.SetFunction([&](auto event) {
		cameraSpeed *= pow(0.9f, -event.value);
		cameraSpeed = std::clamp(cameraSpeed, 0.01f, 20.0f);
		});
	window.mouseScrollDispatcher.AddHandler(mouseScrollEventHandler);
}

void SimulationVisualisationScene::UpdateCamera(float dt)
{
	dt = std::min(dt, 0.1f);

	if (window.GetLastKeyState(Key::W).down)
		cameraPos += cameraRot * Vec3f(0, 0, dt) * cameraSpeed;
	if (window.GetLastKeyState(Key::S).down)
		cameraPos += cameraRot * Vec3f(0, 0, -dt) * cameraSpeed;
	if (window.GetLastKeyState(Key::D).down)
		cameraPos += cameraRot * Vec3f(dt, 0, 0) * cameraSpeed;
	if (window.GetLastKeyState(Key::A).down)
		cameraPos += cameraRot * Vec3f(-dt, 0, 0) * cameraSpeed;
	if (window.GetLastKeyState(Key::Space).down)
		if (window.GetLastKeyState(Key::LShift).down)
			cameraPos += cameraRot * Vec3f(0, -dt, 0) * cameraSpeed;
		else
			cameraPos += cameraRot * Vec3f(0, dt, 0) * cameraSpeed;

	if (uiScreen.cameraHasMouseFocus)
	{
		cameraAngles.x += -(float)Input::GetDesktopMouseMovement().y / 1000;
		cameraAngles.y += (float)Input::GetDesktopMouseMovement().x / 1000;
	}
	cameraRot = Quatf(Vec3f(0, 1, 0), cameraAngles.y) * Quatf(Vec3f(1, 0, 0), cameraAngles.x);
}
