#pragma once
#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimVisUI.h"
#include "CameraControls.h"
#include "SPH/OpenCL/OpenCLContext.h"
#include "SPH/Graphics/ParticleRenderer.h"
#include "SPH/Core/SceneBlueprint.h"

class Simulation
{
public:
	virtual ~Simulation() {}

	virtual SPH::SimulationEngine& GetSimulationEngine() = 0;
	virtual SPH::ParticleBufferManager& GetDynamicParticlesBufferManager() = 0;
	virtual SPH::ParticleBufferManager& GetStaticParticlesBufferManager() = 0;

	virtual void Update(float dt, uintMem simulationSteps) = 0;
	virtual void Render(const Graphics::RendererRegistry& registry, const Graphics::RenderContext& renderContext, const Mat4f& modelMatrix) = 0;
};

class SimulationVisualisationScene :
	public SceneBlueprint
{
public:
	struct SimulationData
	{
		SPH::SimulationEngine& system;
		SPH::ParticleBufferManagerGL& dynamicParticlesBufferManager;
		SPH::ParticleBufferManagerGL& staticParticlesBufferManager;
	};

	OpenCLContext& clContext;
	cl_command_queue clCommandQueue;
	Graphics::OpenGL::RenderWindow_OpenGL& window;

	ResourceManager resourceManager;

	Array<std::function<Handle<Simulation>()>> simulations;
	uintMem currentSimulationIndex;
	Handle<Simulation> currentSimulation;

	SPH::SceneBlueprint simulationSceneBlueprint;
	Mat4f simulationModelMatrix = Mat4f::TranslationMatrix(Vec3f(0, 0, 20));

	Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;

	SPH::ParticleRenderer particleRenderer{ graphicsContext };
	Graphics::OpenGL::TexturedRectRenderer_OpenGL texturedRectRenderer{ graphicsContext };
	Graphics::OpenGL::AntialiasedTextRenderer_OpenGL antialiasedTextRenderer{ graphicsContext };
	Graphics::OpenGL::PanelRenderer_OpenGL panelRenderer{ graphicsContext };
	Graphics::OpenGL::BasicIndexedMeshRenderer_OpenGL basicMeshRenderer{ graphicsContext };
	Graphics::RendererRegistry rendererRegistry{ particleRenderer, texturedRectRenderer, antialiasedTextRenderer, panelRenderer, basicMeshRenderer };

	UI::System UISystem{ graphicsContext, rendererRegistry, window };
	ResourceRef<UI::FontManager> fontManager = resourceManager.LoadResource<UI::FontManager>("fontManager",  resourceManager);

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

	SimulationVisualisationScene(OpenCLContext& clContext, cl_command_queue clCommandQueue, Graphics::OpenGL::RenderWindow_OpenGL& window);
	~SimulationVisualisationScene();

	void LoadScene();

	void Update() override;
	void Render(const Graphics::RenderContext& renderContext) override;
	void OnEvent(const Input::GenericInputEvent& event) override;

	void WindowResized(const Window::ResizedEvent& event);
	EVENT_MEMBER_FUNCTION(SimulationVisualisationScene, WindowResized, window.resizedEventDispatcher);

	void InitializeSystemAndSetAsCurrent(uintMem index);
};