#pragma once
#include "Scenes/Scene.h"
#include "OpenCLContext.h"
#include "ProfilingUI.h"

#include "SPH/ParticleBufferManager/OfflineCPUParticleBufferManager.h"
#include "SPH/ParticleBufferManager/OfflineGPUParticleBufferManager.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

class ProfilingScene : public SceneBlueprint
{
public:
	ResourceManager resourceManager;

	ProfilingScene(OpenCLContext& CLContext, cl_command_queue clQueue, Graphics::OpenGL::RenderWindow_OpenGL& window);
	~ProfilingScene();

	void Update() override;
	void Render(const Graphics::RenderContext& renderContext) override;
	void OnEvent(const Input::GenericInputEvent& event) override;

	void StartProfiling();
private:
	struct Profile
	{
		SPH::ParticleSimulationParameters systemInitParameters;
		String name;
		float simulationDuration;
		float simulationStepTime;
		uint stepsPerUpdate;
		Path outputFilePath;
		File outputFile;
	};
	struct SimulationData
	{
		SPH::SimulationEngine& system;
		SPH::ParticleBufferManager& dynamicParticleBufferSet;
		SPH::ParticleBufferManager& staticParticleBufferSet;

		//Array<SPH::SystemProfilingData> profilingData;
	};

	OpenCLContext& clContext;
	Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext;
	Graphics::OpenGL::RenderWindow_OpenGL& window;

	SPH::OfflineGPUParticleBufferManager GPUDynamicParticlesBufferManager;
	SPH::OfflineGPUParticleBufferManager GPUStaticParticlesBufferManager;
	SPH::GPUSimulationEngine SPHSystemGPU;
	SPH::OfflineCPUParticleBufferManager CPUDynamicParticlesBufferManager;
	SPH::OfflineCPUParticleBufferManager CPUStaticParticlesBufferManager;
	SPH::CPUSimulationEngine SPHSystemCPU;

	Array<SimulationData> SPHSystems;

	Graphics::OpenGL::TexturedRectRenderer_OpenGL texturedRectRenderer{ graphicsContext };
	Graphics::OpenGL::AntialiasedTextRenderer_OpenGL antialiasedTextRenderer{ graphicsContext };
	Graphics::OpenGL::PanelRenderer_OpenGL panelRenderer{ graphicsContext };
	Graphics::RendererRegistry rendererRegistry{ texturedRectRenderer, antialiasedTextRenderer, panelRenderer };

	UI::System UISystem{ graphicsContext, rendererRegistry, window };
	ResourceRef<UI::FontManager> fontManager = resourceManager.LoadResource<UI::FontManager>("fontManager", resourceManager);

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