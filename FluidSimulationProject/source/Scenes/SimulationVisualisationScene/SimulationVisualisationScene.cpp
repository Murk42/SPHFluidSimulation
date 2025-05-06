#include "pch.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"

SimulationVisualisationScene::SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clCommandQueue, RenderingSystem& renderingSystem) :
	clContext(clContext), clCommandQueue(clCommandQueue), renderingSystem(renderingSystem), window(renderingSystem.GetWindow()), SPHSystemRenderer(renderingSystem.GetGraphicsContext()), currentSPHSystemIndex(0),
	SPHSystemGPU(clContext.context, clContext.device, clCommandQueue, renderingSystem.GetGraphicsContext()),
	SPHSystemCPU(std::thread::hardware_concurrency()),
	GPUdynamicParticleBufferManager(clContext.context, clContext.device, clCommandQueue),
	GPUstaticParticleBufferManager(clContext.context, clContext.device, clCommandQueue),
	uiScreen(*this)
{		
	//Setup UI		
	UIInputManager.SetScreen(&uiScreen);

	renderingSystem.SetScreen(&uiScreen);
	renderingSystem.SetSPHSystemRenderer(&SPHSystemRenderer);	
	renderingSystem.GetWindow().keyDownEventDispatcher.AddHandler(*this);

	SPHSystemDynamicParticlesRenderCache.SetModelMatrix(SPHSystemModelMatrix);		
	SPHSystemDynamicParticlesRenderCache.SetParticleColor(0xffffffff);
	SPHSystemDynamicParticlesRenderCache.SetParticleSize(0.1f);
	SPHSystemStaticParticlesRenderCache.SetModelMatrix(SPHSystemModelMatrix);
	SPHSystemStaticParticlesRenderCache.SetParticleColor(0xff0000ff);
	SPHSystemStaticParticlesRenderCache.SetParticleSize(0.03f);

	simulationScene.LoadScene("assets/simulationScenes/scene.json");
			
	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUdynamicParticleBufferManager, GPUstaticParticleBufferManager });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUdynamicParticleBufferManager, CPUstaticParticleBufferManager });

	InitializeSystemAndSetAsCurrent(1);	
}

SimulationVisualisationScene::~SimulationVisualisationScene()
{
	renderingSystem.GetWindow().keyDownEventDispatcher.RemoveHandler(*this);
	renderingSystem.SetScreen(nullptr);
	renderingSystem.SetSPHSystemRenderer(nullptr);	
}

void SimulationVisualisationScene::Update()
{
	float dt = frameStopwatch.Reset();	
		
	renderingSystem.SetViewMatrix(cameraControls.GetViewMatrix());		

	if (runSimulation || stepSimulation)
	{
		SPHSystems[currentSPHSystemIndex].system.Update(0.01f, 3);		
		SPHSystems[currentSPHSystemIndex].dynamicParticlesBufferManager.PrepareForRendering();
		SPHSystems[currentSPHSystemIndex].staticParticlesBufferManager.PrepareForRendering();		
		stepSimulation = false;
	}
	
	uiScreen.SetInfo("Simulation elapsed time: " + StringParsing::Convert(SPHSystems[currentSPHSystemIndex].system.GetSimulationTime()) + "s\n\n");
	
	renderingSystem.Render({
		&SPHSystemDynamicParticlesRenderCache,
		&SPHSystemStaticParticlesRenderCache
		});

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
					SPHSystemDynamicParticlesRenderCache.SetParticleColor(0x000000ff);
					SPHSystemStaticParticlesRenderCache.SetParticleColor(0xff0000ff);					
					renderingSystem.SetScreen(nullptr);
					cameraControls.SetCameraPos(imagingCameraPos);
					cameraControls.SetCameraAngles(imagingCameraAngles);
				}
				else
				{
					renderingSystem.DisableCustomClearColor();
					SPHSystemDynamicParticlesRenderCache.SetParticleColor(0xffffffff);
					SPHSystemStaticParticlesRenderCache.SetParticleColor(0xff0000ff);
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
	SPHSystems[currentSPHSystemIndex].dynamicParticlesBufferManager.Clear();
	SPHSystems[currentSPHSystemIndex].staticParticlesBufferManager.Clear();

	currentSPHSystemIndex = index;

	SPHSystems[currentSPHSystemIndex].system.Initialize(simulationScene, SPHSystems[currentSPHSystemIndex].dynamicParticlesBufferManager, SPHSystems[currentSPHSystemIndex].staticParticlesBufferManager);

	SPHSystemDynamicParticlesRenderCache.SetParticleBufferManagerRenderData(SPHSystems[currentSPHSystemIndex].dynamicParticlesBufferManager, sizeof(SPH::DynamicParticle));
	SPHSystemStaticParticlesRenderCache.SetParticleBufferManagerRenderData(SPHSystems[currentSPHSystemIndex].staticParticlesBufferManager, sizeof(SPH::StaticParticle));

	uiScreen.SetImplenetationName(SPHSystems[currentSPHSystemIndex].system.SystemImplementationName());
	uiScreen.SetParticleCount(SPHSystems[currentSPHSystemIndex].dynamicParticlesBufferManager.GetBufferSize() / sizeof(SPH::DynamicParticle));
}
