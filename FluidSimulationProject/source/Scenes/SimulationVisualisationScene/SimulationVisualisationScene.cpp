#include "pch.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"
#include "SPH/ParticleBufferManager/RenderableCPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/RenderableGPUParticleBufferManager.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

class CPUSimulation : public Simulation
{
public:
	SPH::RenderableCPUParticleBufferManager dynamicParticleBufferManager;
	SPH::RenderableCPUParticleBufferManager staticParticleBufferManager;
	SPH::CPUSimulationEngine engine;

	SPH::ParticleBufferManagerRenderCache dynamicParticlesRenderCache;
	SPH::ParticleBufferManagerRenderCache staticParticlesRenderCache;

	CPUSimulation(SPH::SceneBlueprint& simulationSceneBlueprint)
		: engine(std::thread::hardware_concurrency())
	{
		engine.Initialize(simulationSceneBlueprint, dynamicParticleBufferManager, staticParticleBufferManager);
		dynamicParticlesRenderCache.SetParticleBufferManagerRenderData(dynamicParticleBufferManager);
		staticParticlesRenderCache.SetParticleBufferManagerRenderData(staticParticleBufferManager);
	}
	~CPUSimulation()
	{

	}

	SPH::SimulationEngine& GetSimulationEngine() override
	{
		return engine;
	}
	SPH::ParticleBufferManager& GetDynamicParticlesBufferManager() override
	{
		return dynamicParticleBufferManager;
	}
	SPH::ParticleBufferManager& GetStaticParticlesBufferManager() override
	{
		return staticParticleBufferManager;
	}
	void Simulation::Update(float dt, uintMem simulationSteps) override
	{
		engine.Update(dt, simulationSteps);
	}
	void Render(const Graphics::RendererRegistry& registry, const Graphics::RenderContext& renderContext, const Mat4f& modelMatrix) override
	{
		dynamicParticleBufferManager.PrepareForRendering();
		staticParticleBufferManager.PrepareForRendering();

		auto particleRenderer = registry.GetRenderer<SPH::ParticleRenderer>();

		if (particleRenderer == nullptr)
		{
			BLAZE_LOG_WARNING("Couldn't find particle renderer");
			return;
		}

		particleRenderer->StartRender(renderContext);
		particleRenderer->Render(dynamicParticlesRenderCache, modelMatrix, 0xffffffff, 0.1f);
		particleRenderer->Render(staticParticlesRenderCache, modelMatrix, 0xff0000ff, 0.03f);
		particleRenderer->EndRender(renderContext);
	}
};

class GPUSimulation : public Simulation
{
public:
	SPH::RenderableGPUParticleBufferManagerWithoutCLGLInterop dynamicParticleBufferManager;
	SPH::RenderableGPUParticleBufferManagerWithoutCLGLInterop staticParticleBufferManager;
	SPH::GPUSimulationEngine engine;

	SPH::ParticleBufferManagerRenderCache dynamicParticlesRenderCache;
	SPH::ParticleBufferManagerRenderCache staticParticlesRenderCache;

	GPUSimulation(cl_context clContext, cl_device_id clDevice, cl_command_queue clQueue, SPH::SceneBlueprint& simulationSceneBlueprint) :
		dynamicParticleBufferManager(clContext, clDevice, clQueue), 
		staticParticleBufferManager(clContext, clDevice, clQueue),
		engine(clContext, clDevice, clQueue)
	{
		engine.Initialize(simulationSceneBlueprint, dynamicParticleBufferManager, staticParticleBufferManager);
		dynamicParticlesRenderCache.SetParticleBufferManagerRenderData(dynamicParticleBufferManager);
		staticParticlesRenderCache.SetParticleBufferManagerRenderData(staticParticleBufferManager);
	}
	~GPUSimulation()
	{

	}

	SPH::SimulationEngine& GetSimulationEngine() override
	{
		return engine;
	}
	SPH::ParticleBufferManager& GetDynamicParticlesBufferManager() override
	{
		return dynamicParticleBufferManager;
	}
	SPH::ParticleBufferManager& GetStaticParticlesBufferManager() override
	{
		return staticParticleBufferManager;
	}
	void Simulation::Update(float dt, uintMem simulationSteps) override
	{
		engine.Update(dt, simulationSteps);
	}
	void Render(const Graphics::RendererRegistry& registry, const Graphics::RenderContext& renderContext, const Mat4f& modelMatrix) override
	{
		dynamicParticleBufferManager.PrepareForRendering();
		staticParticleBufferManager.PrepareForRendering();

		auto particleRenderer = registry.GetRenderer<SPH::ParticleRenderer>();

		if (particleRenderer == nullptr)
		{
			BLAZE_LOG_WARNING("Couldn't find particle renderer");
			return;
		}

		particleRenderer->StartRender(renderContext);
		particleRenderer->Render(dynamicParticlesRenderCache, modelMatrix, 0xffffffff, 0.1f);
		particleRenderer->Render(staticParticlesRenderCache, modelMatrix, 0xff0000ff, 0.03f);
		particleRenderer->EndRender(renderContext);
	}
};

SimulationVisualisationScene::SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clCommandQueue, Graphics::OpenGL::RenderWindow_OpenGL& window) :
	clContext(clContext), clCommandQueue(clCommandQueue), graphicsContext(window.GetGraphicsContext()), window(window), currentSimulationIndex(0)
{
	fontManager->AddFontFace("default", resourceManager.LoadResource<UI::FontFace>("default", "assets/fonts/Hack-Regular.ttf", 0));
	fontManager->CreateFontAtlas("default", { 16, 32, 64 }, antialiasedTextRenderer);
	UISystem.SetScreen<SimVisUI>(resourceManager, *this);	

	particleRenderer.SetProjectionMatrix(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));
	basicMeshRenderer.SetProjectionMatrix(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));
	basicMeshRenderer.SetShadingOptions(Vec3f(0, -1, 0), 0xffffff);

	LoadScene();

	simulations.AddBack([&]() -> Handle<Simulation> { return Handle<Simulation>::CreateDerived<CPUSimulation>(simulationSceneBlueprint); });
	simulations.AddBack([&]() -> Handle<Simulation> { return Handle<Simulation>::CreateDerived<GPUSimulation>(clContext.context, clContext.device, this->clCommandQueue, simulationSceneBlueprint); });

	InitializeSystemAndSetAsCurrent(1);
}

void SimulationVisualisationScene::LoadScene()
{
	simulationSceneBlueprint.LoadScene("assets/simulationScenes/triangleTestScene.json");
}

SimulationVisualisationScene::~SimulationVisualisationScene()
{
}

void SimulationVisualisationScene::Update()
{
	float dt = frameStopwatch.Reset();

	if (runSimulation || stepSimulation)
	{
		if (currentSimulation != nullptr)
			currentSimulation->Update(0.01f, 10);

		stepSimulation = false;
	}

	++FPSCount;
	if (FPSStopwatch.GetTime() > 1.0f)
	{
		static_cast<SimVisUI*>(UISystem.GetScreen())->SetFPS(FPSCount);
		FPSStopwatch.Reset();
		FPSCount = 0;
	}

	cameraControls.Update();

	(UISystem.GetScreen())->Update();
	static_cast<SimVisUI*>(UISystem.GetScreen())->Update();
}

void SimulationVisualisationScene::Render(const Graphics::RenderContext& renderContext)
{
	window.ClearRenderBuffers();

	particleRenderer.SetViewMatrix(cameraControls.GetViewMatrix());

	if (currentSimulation)
		currentSimulation->Render(rendererRegistry, renderContext, simulationModelMatrix);
			
	basicMeshRenderer.SetViewMatrix(cameraControls.GetViewMatrix());
	basicMeshRenderer.StartRender(renderContext);
	basicMeshRenderer.Render(simulationSceneBlueprint.GetMesh().GetVertices(), simulationSceneBlueprint.GetMesh().GetIndices(), simulationModelMatrix, 0x80000080, renderContext);
	basicMeshRenderer.EndRender(renderContext);

	if (!imagingMode)
	{
		graphicsContext.EnableDepthTest(false);
		UISystem.Render();
		graphicsContext.EnableDepthTest(true);
	}

	window.Present();
}

void SimulationVisualisationScene::OnEvent(const Input::GenericInputEvent& event)
{
	if (event.TryProcess([&](const Input::KeyDownEvent& event)
		{
			using namespace Input;
			using enum Key;

			switch (event.key)
			{
			case X:
			{
				if (bool(event.modifier & Input::KeyModifier::SHIFT))
					runSimulation = false;
				else
					runSimulation = true;
				break;
			}
			case RIGHT:
			{
				stepSimulation = true;
				break;
			}
			case T:
			{
				InitializeSystemAndSetAsCurrent((currentSimulationIndex + 1) % simulations.Count());
				runSimulation = false;
				break;
			}
			case R:
			{
				LoadScene();
				InitializeSystemAndSetAsCurrent(currentSimulationIndex);
				break;
			}
			case I:
			{
				if (bool(event.modifier & Input::KeyModifier::SHIFT))
				{
					imagingCameraPos = cameraControls.GetCameraPos();
					imagingCameraAngles = cameraControls.GetCameraAngles();
				}
				else
				{
					imagingMode = !imagingMode;
					if (imagingMode)
					{
						graphicsContext.SetClearColor(0xffffffff);
						cameraControls.SetCameraPos(imagingCameraPos);
						cameraControls.SetCameraAngles(imagingCameraAngles);
					}
					else
					{
						graphicsContext.SetClearColor(0x000000ff);
					}
				}
				break;
			}
			default:
				return false;
			}
			return true;
		}))
		return;

	if (UISystem.ProcessInputEvent(event, false))
		return;

	
}

void SimulationVisualisationScene::WindowResized(const Window::ResizedEvent& event)
{
	particleRenderer.SetProjectionMatrix(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));
	basicMeshRenderer.SetProjectionMatrix(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));
}

void SimulationVisualisationScene::InitializeSystemAndSetAsCurrent(uintMem index)
{
	currentSimulationIndex = index;

	currentSimulation = simulations[index]();

	static_cast<SimVisUI*>(UISystem.GetScreen())->SetImplenetationName(currentSimulation->GetSimulationEngine().SystemImplementationName());
	static_cast<SimVisUI*>(UISystem.GetScreen())->SetParticleCount(currentSimulation->GetDynamicParticlesBufferManager().GetParticleCount());
}
