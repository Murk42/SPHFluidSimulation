#include "pch.h"
#include "RenderingSystem.h"
#include "UI.h"

#include "OpenCLContext.h"

#include "SPH/System/SystemGPU.h"
#include "SPH/Graphics/SystemGPURenderer.h"
#include "SPH/System/SystemCPU.h"
#include "SPH/Graphics/SystemCPURenderer.h"

#include "SPH/ParticleGenerator/FilledBoxParticleGenerator.h"
#include "SPH/ParticleGenerator/BoxShellParticleGenerator.h"

#include "ThreadPool.h"

SPH::SystemInitParameters systemInitParams{
	.dynamicParticleGenerationParameters = {
		.generator = std::make_shared<SPH::FilledBoxParticleGenerator<SPH::DynamicParticle>>(SPH::FilledBoxParticleParameters {
			.spawnVolumeSize = Vec3f(10.0f),
			.spawnVolumeOffset = Vec3f(-5.0f),
			.particlesPerUnit = 10,
			.randomOffsetIntensity = 0.4f
		})
	},
	.staticParticleGenerationParameters {	
		.generator = std::make_shared<SPH::BoxShellParticleGenerator<SPH::StaticParticle>>(SPH::BoxShellParticleParameters {
			.spawnVolumeSize = Vec3f(20.0f), 
			.spawnVolumeOffset = Vec3f(-10.0f),
			.particleDistance = 0.2f,
			.randomOffsetIntensity = 0.1f
		})
	},
	.particleBehaviourParameters {
		.particleMass = 1.0f,
		.gasConstant = 200.0f,
		.elasticity = 0.5f,
		.viscosity = 0.5f,
		.gravity = Vec3f(0, -9.81, 0),
		
		.restDensity = 25.0f,
		.maxInteractionDistance = 1.0f,
	},
	.particleBoundParameters {
		.bounded = false,
	},
	.bufferCount = 2,
	.hashesPerParticle = 1,
	.hashesPerStaticParticle = 1,
};

const uintMem simulationThreadCount = 0;

bool exitApp = false;
bool runSimulation = false;
LambdaEventHandler<Input::Events::KeyPressed> keyPresedEventHandler;
LambdaEventHandler<Input::Events::MouseScroll> mouseScrollEventHandler;
LambdaEventHandler<Input::Events::WindowCloseEvent> windowCloseEventHandler;
LambdaEventHandler<Input::Events::WindowResizedEvent> windowResizedEventHandler;
LambdaEventHandler<UI::EditableText::TextEnteredEvent> viscosityValueChangedEventHandler;

SPH::System* SPHSystem = nullptr;
SPH::SystemRenderer* SPHSystemRenderer = nullptr;
SPH::SystemRenderCache* SPHSystemRenderCache = nullptr;
Mat4f SPHSystemModelMatrix = Mat4f::TranslationMatrix(Vec3f(0, 0, 20));

bool cameraHasMouseFocus = false;
float cameraSpeed = 1.0f;
Vec2f cameraAngles;
Quatf cameraRot;
Vec3f cameraPos = Vec3f(0, 0, 0);

static void NewSystem(SPH::System* newSystem, SPH::SystemRenderer* newRenderer, SPH::SystemRenderCache* newRenderCache, UIScreen* screen, RenderingSystem* renderingSystem)
{	
	delete SPHSystem;
	delete SPHSystemRenderer;
	delete SPHSystemRenderCache;
	SPHSystem = nullptr;
	SPHSystemRenderer = nullptr;
	SPHSystemRenderCache = nullptr;

	SPHSystem = newSystem;
	if (system != nullptr)
	{
		SPHSystemRenderer = newRenderer;
		SPHSystemRenderCache = newRenderCache;
		SPHSystem->Initialize(systemInitParams);

		screen->SetParticleCount(SPHSystem->GetDynamicParticleCount());
		screen->SetImplenetationName(SPHSystem->SystemImplementationName());

		SPHSystemRenderCache->LinkSPHSystem(SPHSystem);
		SPHSystemRenderCache->SetModelMatrix(SPHSystemModelMatrix);

		renderingSystem->SetSPHSystemRenderer(SPHSystemRenderer);
		renderingSystem->SetSPHSystemRenderingCache(SPHSystemRenderCache);
	}
	else
	{
		screen->SetParticleCount(0);
		screen->SetImplenetationName(StringUTF8());
	}
}

static void NewGPUSystem(OpenCLContext& openCLContext, UIScreen* uiScreen, RenderingSystem* renderingSystem)
{
	NewSystem(
		new SPH::SystemGPU(openCLContext),
		new SPH::SystemGPURenderer(renderingSystem->GetGraphicsContext()),
		new SPH::SystemGPURenderCache(),
		uiScreen,
		renderingSystem
	);
}
static void NewCPUSystem(ThreadPool& threadPool, UIScreen* uiScreen, RenderingSystem* renderingSystem)
{
	NewSystem(
		new SPH::SystemCPU(threadPool),
		new SPH::SystemCPURenderer(renderingSystem->GetGraphicsContext()),
		new SPH::SystemCPURenderCache(),
		uiScreen,
		renderingSystem
	);
}

static void SetupEvents(Window& window, RenderingSystem& renderingSystem, UIScreen& uiScreen)
{
	keyPresedEventHandler.SetFunction({
		
	});
	window.keyPressedDispatcher.AddHandler(keyPresedEventHandler);
	windowCloseEventHandler.SetFunction({
		[&](auto event) { exitApp = true; }
	});
	window.closeEventDispatcher.AddHandler(windowCloseEventHandler);
	windowResizedEventHandler.SetFunction({
		[&](auto event) {
			renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(Math::PI / 2, (float)event.size.x / event.size.y, 0.1, 1000));

			auto transform = uiScreen.GetTransform();
			transform.size = (Vec2f)event.size;
			uiScreen.SetTransform(transform);
			}
	});
	window.resizedEventDispatcher.AddHandler(windowResizedEventHandler);
	mouseScrollEventHandler.SetFunction([&](auto event) {
		cameraSpeed *= pow(0.9f, -event.value);
		cameraSpeed = std::clamp(cameraSpeed, 0.01f, 20.0f);
		});	
	window.mouseScrollDispatcher.AddHandler(mouseScrollEventHandler);

	viscosityValueChangedEventHandler.SetFunction([&](UI::EditableText::TextEnteredEvent event) {
		float value;
		Result res = StringParsing::Convert(event.string, value);
		if (!res.IsEmpty())
		{
			res.ClearSilent();
			return;
		}

		systemInitParams.particleBehaviourParameters.viscosity = value;
	});
	uiScreen.viscosityText.editableText.textEnteredEventDispatcher.AddHandler(viscosityValueChangedEventHandler);
}

static void UpdateCamera(Window& window, float dt)
{				
	dt = std::min(dt, 0.1f);

	if (window.GetLastKeyState(Key::W).down)
		cameraPos += cameraRot * Vec3f(0, 0, dt) * cameraSpeed;
	if (window.GetLastKeyState(Key::S).down)
		cameraPos += cameraRot * Vec3f(0, 0,-dt) * cameraSpeed;
	if (window.GetLastKeyState(Key::D).down)
		cameraPos += cameraRot * Vec3f(dt, 0, 0) * cameraSpeed;
	if (window.GetLastKeyState(Key::A).down)
		cameraPos += cameraRot * Vec3f(-dt, 0, 0) * cameraSpeed;
	if (window.GetLastKeyState(Key::Space).down)
		if (window.GetLastKeyState(Key::LShift).down)
			cameraPos += cameraRot * Vec3f(0,-dt, 0) * cameraSpeed;
		else
			cameraPos += cameraRot * Vec3f(0, dt, 0) * cameraSpeed;

	if (cameraHasMouseFocus)
	{		
		cameraAngles.x += -(float)Input::GetDesktopMouseMovement().y / 1000;
		cameraAngles.y += (float)Input::GetDesktopMouseMovement().x / 1000;
	}
	cameraRot = Quatf(Vec3f(0, 1, 0), cameraAngles.y) * Quatf(Vec3f(1, 0, 0), cameraAngles.x );
}

CLIENT_API void Setup()
{		
	//Setup rendering and window
	RenderingSystem renderingSystem;
	Window& window = renderingSystem.GetWindow();	
	window.Maximize();
	window.ShowWindow(true);		
	window.Raise();

	renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(Math::PI / 2, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));			
	
	//Setup processing pools	
	OpenCLContext openCLContext{ renderingSystem.GetGraphicsContext() };
	ThreadPool threadPool;
	threadPool.AllocateThreads(simulationThreadCount);

	//Setup UI
	UIScreen uiScreen;
	uiScreen.SetWindow(&window);
	renderingSystem.SetScreen(&uiScreen);		
	UI::InputManager uiInputManager;
	uiInputManager.SetScreen(&uiScreen);		
		
	
	NewGPUSystem(openCLContext, &uiScreen, &renderingSystem);
	//NewCPUSystem(threadPool, &uiScreen, &renderingSystem);
	
	SetupEvents(window, renderingSystem, uiScreen);
	
	Stopwatch frameStopwatch;
	Stopwatch FPSStopwatch;
	Stopwatch simulationStopwatch;
	uintMem FPSCount = 0;	
	while (!exitApp)
	{
		float dt = frameStopwatch.Reset();

		Input::Update();		

		renderingSystem.SetViewMatrix(Mat4f::RotationMatrix(cameraRot.Conjugated()) * Mat4f::TranslationMatrix(-cameraPos));

		
		if (!Set<UI::Node*>({ nullptr, &uiScreen.cameraMouseFocusNode }).Find(uiInputManager.GetSelectedNode()).IsNull())
		{
			if (window.GetLastKeyState(Key::X).pressed)
				if (window.GetLastKeyState(Key::LShift).down)
					runSimulation = false;
				else
					runSimulation = true;

			if (window.GetLastKeyState(Key::T).pressed)
				if (SPHSystem == nullptr || SPHSystem->SystemImplementationName() == "CPU")
					NewGPUSystem(openCLContext, &uiScreen, &renderingSystem);
				else
					NewCPUSystem(threadPool, &uiScreen, &renderingSystem);
			
			if (window.GetLastKeyState(Key::R).pressed)
				SPHSystem->Initialize(systemInitParams);
		}

		if (SPHSystem != nullptr && (runSimulation || window.GetLastKeyState(Key::Right).pressed && !Set<UI::Node*>({ nullptr, &uiScreen.cameraMouseFocusNode }).Find(uiInputManager.GetSelectedNode()).IsNull()))
			SPHSystem->Update(0.01f);
							
		if (uiInputManager.GetSelectedNode() == &uiScreen.cameraMouseFocusNode)
			UpdateCamera(window, dt);				
				
		renderingSystem.Render();

		++FPSCount;
		if (FPSStopwatch.GetTime() > 1.0f)
		{
			uiScreen.SetFPS(FPSCount);
			FPSStopwatch.Reset();
			FPSCount = 0;
		}		
	}

	delete SPHSystem;
	delete SPHSystemRenderer;
	delete SPHSystemRenderCache;
}