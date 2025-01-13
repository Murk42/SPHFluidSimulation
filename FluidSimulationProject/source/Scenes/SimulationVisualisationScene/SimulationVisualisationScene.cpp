#include "pch.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"

SimulationVisualisationScene::SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clQueue, ThreadPool& threadPool, RenderingSystem& renderingSystem) :
	clContext(clContext), clQueue(clQueue), threadPool(threadPool), renderingSystem(renderingSystem), window(renderingSystem.GetWindow()), SPHSystemRenderer(renderingSystem.GetGraphicsContext()), currentSPHSystemIndex(0),
	SPHSystemGPU(clContext.context, clContext.device, clQueue, renderingSystem.GetGraphicsContext()), SPHSystemCPU(threadPool), GPUParticleBufferSet(clContext.context, clContext.device, clQueue),
	uiScreen(&window)
{		
	//Setup UI	
	uiScreen.SetWindow(&window);	

	UIInputManager.SetScreen(&uiScreen);

	SetupEvents();
			
	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUParticleBufferSet });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUParticleBufferSet });

	for (auto& SPHSystem : SPHSystems)
		SPHSystem.system.EnableProfiling(true);

	SetSystemIndex(0);
	LoadSystemInitParameters();
	simulationScene.InitializeSystem(SPHSystems[currentSPHSystemIndex].system, SPHSystems[currentSPHSystemIndex].particleBufferSet);	

	uiScreen.SetParticleCount(SPHSystems[currentSPHSystemIndex].system.GetDynamicParticleCount());

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
			simulationScene.InitializeSystem(SPHSystems[currentSPHSystemIndex].system, SPHSystems[currentSPHSystemIndex].particleBufferSet);
			uiScreen.SetParticleCount(SPHSystems[currentSPHSystemIndex].system.GetDynamicParticleCount());
			runSimulation = false;
		}			

		if (window.GetLastKeyState(Key::R).pressed)
		{
			LoadSystemInitParameters();
			simulationScene.InitializeSystem(SPHSystems[currentSPHSystemIndex].system, SPHSystems[currentSPHSystemIndex].particleBufferSet);
			uiScreen.SetParticleCount(SPHSystems[currentSPHSystemIndex].system.GetDynamicParticleCount());
		}

		if (window.GetLastKeyState(Key::I).pressed)
		{
			if (window.GetLastKeyState(Key::LShift).down)
			{
				imagingCameraPos = cameraPos;
				imagingCameraAngles = cameraAngles;
			}
			else
			{
				imagingMode = !imagingMode;
				if (imagingMode)
				{
					renderingSystem.SetCustomClearColor(0xffffffff);
					SPHSystemRenderer.SetDynamicParticleColor(0x000000ff);
					SPHSystemRenderer.SetStaticParticleColor(0xff0000ff);
					renderingSystem.SetScreen(nullptr);
					cameraPos = imagingCameraPos;
					cameraAngles = imagingCameraAngles;					
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
	}

	bool updated = false;

	if ((runSimulation || window.GetLastKeyState(Key::Right).pressed && (UIInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode || UIInputManager.GetSelectedNode() == nullptr)))
	{
		SPHSystems[currentSPHSystemIndex].system.Update(std::min(dt, 0.01666f), simulationStepsPerUpdate);		
	}		

	auto& profilingData = SPHSystems[currentSPHSystemIndex].system.GetProfilingData();

	String info;
	info +=
		"Simulation elapsed time: " + StringParsing::Convert(SPHSystems[currentSPHSystemIndex].system.GetSimulationTime()) + "s\n"
		"Steps per update: " + StringParsing::Convert(simulationStepsPerUpdate) + "\n"
		"\n"
		"Time per update: " + StringParsing::Convert(profilingData.timePerStep_s * 1000) + "ms\n";

	for (auto it = profilingData.implementationSpecific.FirstIterator(); it != profilingData.implementationSpecific.BehindIterator(); ++it)
	{
		if (const float* value = it.GetValue<float>())
			info += *it.GetKey() + ": " + StringParsing::Convert(*value * 1000) + "ms\n";
	}

	info += "\n";
	

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
	simulationScene.LoadScene("assets/simulationScenes/scene.json");	
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
