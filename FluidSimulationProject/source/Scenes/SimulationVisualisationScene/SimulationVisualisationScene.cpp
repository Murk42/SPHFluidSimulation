#include "pch.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"

SimulationVisualisationScene::SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clCommandQueue, RenderingSystem& renderingSystem) :
	clContext(clContext), clCommandQueue(clCommandQueue), renderingSystem(renderingSystem), window(renderingSystem.GetWindow()), SPHSystemRenderer(renderingSystem.GetGraphicsContext()), currentSPHSystemIndex(0),
	SPHSystemGPU(clContext.context, clContext.device, clCommandQueue, renderingSystem.GetGraphicsContext()),
	SPHSystemCPU(std::thread::hardware_concurrency()), 
	GPUParticleBufferManager(clContext.context, clContext.device, clCommandQueue),
	uiScreen(*this)
{		
	//Setup UI		
	UIInputManager.SetScreen(&uiScreen);

	renderingSystem.SetScreen(&uiScreen);
	renderingSystem.SetSPHSystemRenderer(&SPHSystemRenderer);
	renderingSystem.SetSPHSystemRenderingCache(&SPHSystemRenderCache);
	renderingSystem.GetWindow().keyDownEventDispatcher.AddHandler(*this);

	SPHSystemRenderCache.SetModelMatrix(SPHSystemModelMatrix);		

	simulationScene.LoadScene("assets/simulationScenes/scene.json");
			
	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUParticleBufferManager });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUParticleBufferManager });

	InitializeSystemAndSetAsCurrent(1);	
}

SimulationVisualisationScene::~SimulationVisualisationScene()
{
	renderingSystem.GetWindow().keyDownEventDispatcher.RemoveHandler(*this);
	renderingSystem.SetScreen(nullptr);
	renderingSystem.SetSPHSystemRenderer(nullptr);
	renderingSystem.SetSPHSystemRenderingCache(nullptr);	
}

void SimulationVisualisationScene::Update()
{
	float dt = frameStopwatch.Reset();	
		
	renderingSystem.SetViewMatrix(cameraControls.GetViewMatrix());		

	if (runSimulation || stepSimulation)
	{
		SPHSystems[currentSPHSystemIndex].system.Update(0.01f, 3);		
		SPHSystems[currentSPHSystemIndex].particleBufferManager.PrepareDynamicParticlesForRendering();
		SPHSystems[currentSPHSystemIndex].particleBufferManager.PrepareStaticParticlesForRendering();
		stepSimulation = false;
	}
	
	uiScreen.SetInfo("Simulation elapsed time: " + StringParsing::Convert(SPHSystems[currentSPHSystemIndex].system.GetSimulationTime()) + "s\n\n");

	renderingSystem.Render();

	++FPSCount;
	if (FPSStopwatch.GetTime() > 1.0f)
	{
		uiScreen.SetFPS(FPSCount);
		FPSStopwatch.Reset();
		FPSCount = 0;
	}
}

void SimulationVisualisationScene::OnEvent(const Keyboard::KeyDownEvent& event)
{
	if (UIInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode || UIInputManager.GetSelectedNode() == nullptr)
	{		
		using namespace Keyboard;
		using enum Key;

		switch (event.key)
		{
		case X: {
			if (bool(event.modifier & Keyboard::KeyModifier::SHIFT))
				runSimulation = false;
			else
				runSimulation = true;
			break;
		}
		case RIGHT: {
			stepSimulation = true;
			break;
		}
		case T: {
			InitializeSystemAndSetAsCurrent((currentSPHSystemIndex + 1) % SPHSystems.Count());
			runSimulation = false;
			break;
		}
		case R: {
			simulationScene.LoadScene("assets/simulationScenes/scene.json");
			InitializeSystemAndSetAsCurrent(currentSPHSystemIndex);
			break;
		}
		case I: {
			if (bool(event.modifier & Keyboard::KeyModifier::SHIFT))
			{
				imagingCameraPos = cameraControls.GetCameraPos();
				imagingCameraAngles = cameraControls.GetCameraAngles();
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
					cameraControls.SetCameraPos(imagingCameraPos);
					cameraControls.SetCameraAngles(imagingCameraAngles);
				}
				else
				{
					renderingSystem.DisableCustomClearColor();
					SPHSystemRenderer.SetDynamicParticleColor(0xffffffff);
					SPHSystemRenderer.SetStaticParticleColor(0xff0000ff);
					renderingSystem.SetScreen(&uiScreen);
				}
			}
			break;
		}
		}									
	}
}

void SimulationVisualisationScene::InitializeSystemAndSetAsCurrent(uintMem index)
{
	//Clear the old system
	SPHSystems[currentSPHSystemIndex].system.Clear();
	SPHSystems[currentSPHSystemIndex].particleBufferManager.Clear();

	currentSPHSystemIndex = index;

	simulationScene.InitializeSystem(SPHSystems[currentSPHSystemIndex].system, SPHSystems[currentSPHSystemIndex].particleBufferManager);

	SPHSystemRenderCache.SetParticleBufferManagerRenderData(SPHSystems[currentSPHSystemIndex].particleBufferManager);	

	uiScreen.SetImplenetationName(SPHSystems[currentSPHSystemIndex].system.SystemImplementationName());
	uiScreen.SetParticleCount(SPHSystems[currentSPHSystemIndex].particleBufferManager.GetDynamicParticleCount());	
}
