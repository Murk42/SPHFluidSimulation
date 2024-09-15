#include "pch.h"
#include "RenderingSystem.h"
#include "UI.h"

#include "OpenCLContext.h"
#include "SPH/SPHSystemGPU.h"
#include "SPH/SPHSystemGPURenderer.h"

#include "VSConsoleWriteStream.h"

using namespace Blaze;

SPH::SystemInitParameters systemInitParams{
	.dynamicParticleGenerationParameters = {
		.spawnVolumeOffset = Vec3f(0.0f),
		.spawnVolumeSize = Vec3f(4.0f),
		.particlesPerUnit = 40,
		.randomOffsetIntensity = 0.0f },
	.staticParticleGenerationParameters {
		.boxSize = Vec3f(2.0f, 2.0f, 2.0f),
		.boxOffset = Vec3f(0.0f, 0.0f, 0.0f),
		.particleDistance = 0.0f
	},

	.particleMass = 1.0f,
	.gasConstant = 100.0f,
	.elasticity = 0.5f,
	.viscosity = 1.0f,

	.boundingBoxSize = Vec3f(4.0f, 4.0f, 4.0f),
	.restDensity = 100.0f,
	.maxInteractionDistance = 1.0f,

	.bufferCount = 2,
};
const uintMem simulationThreadCount = 0;

bool exitApp = false;
bool runSimulation = false;
LambdaEventHandler<Input::Events::KeyPressed> keyPresedEventHandler;
LambdaEventHandler<Input::Events::WindowCloseEvent> windowCloseEventHandler;
LambdaEventHandler<Input::Events::WindowResizedEvent> windowResizedEventHandler;

Mat4f SPHSystemModelMatrix = Mat4f::TranslationMatrix(Vec3f(-0.5f, -0.5f, 1.0f) * systemInitParams.boundingBoxSize.x);

Vec2f cameraAngles;
Quatf cameraRot;
Vec3f cameraPos = Vec3f(0, 0, 0);

uintMem highlightedParticle = 0;
uintMem selectedParticle = 0;

static void SetupEvents(Window& window, RenderingSystem& renderingSystem, UIScreen& uiScreen)
{
	keyPresedEventHandler.SetFunction({
		[&](auto event) { if (event.key == Key::Escape)	exitApp = true;	}
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
}

static void UpdateCamera(Window& window, float dt)
{		
	dt = std::min(dt, 0.1f);

	if (window.GetLastKeyState(Key::W).down)
		cameraPos += cameraRot * Vec3f(0, 0, dt);
	if (window.GetLastKeyState(Key::S).down)
		cameraPos += cameraRot * Vec3f(0, 0,-dt);
	if (window.GetLastKeyState(Key::D).down)
		cameraPos += cameraRot * Vec3f(dt, 0, 0);
	if (window.GetLastKeyState(Key::A).down)
		cameraPos += cameraRot * Vec3f(-dt, 0, 0);
	if (window.GetLastKeyState(Key::Space).down)
		if (window.GetLastKeyState(Key::LShift).down)
			cameraPos += cameraRot * Vec3f(0,-dt, 0);
		else
			cameraPos += cameraRot * Vec3f(0, dt, 0);

	if (window.GetLastKeyState(Key::MouseRight).down)
	{		
		cameraAngles.x += -(float)Input::GetDesktopMouseMovement().y / 1000;
		cameraAngles.y += (float)Input::GetDesktopMouseMovement().x / 1000;
	}
	cameraRot = Quatf(Vec3f(0, 1, 0), cameraAngles.y) * Quatf(Vec3f(1, 0, 0), cameraAngles.x );
}

CLIENT_API void Setup()
{
	VSConsoleWriteStream writeStream;
	Debug::Logger::AddOutputStream(writeStream);

	RenderingSystem renderingSystem;
	Window& window = renderingSystem.GetWindow();	
	window.Maximize();
	window.ShowWindow(true);		

	renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(Math::PI / 2, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));			

	//Setup UI
	UIScreen uiScreen;
	uiScreen.SetWindow(&window);
	renderingSystem.SetScreen(&uiScreen);		
	UI::UIInputManager uiInputManager;
	uiInputManager.AddScreen(&uiScreen);	
	LambdaEventHandler<UI::EditableText::TextEnteredEvent> viscosityValueChangedEventHandler{ [&](UI::EditableText::TextEnteredEvent event) {
		float value;
		Result res = StringParsing::Convert(event.string, value);
		if (!res.IsEmpty())
		{
			res.ClearSilent();
			return;
		}

		systemInitParams.viscosity = value;
	} };
	uiScreen.viscosityText.textEnteredEventDispatcher.AddHandler(viscosityValueChangedEventHandler);

	//Setup processing pools
	OpenCLContext openCLContext{ renderingSystem.GetGraphicsContext() };	

	//ThreadPool threadPool;
	//threadPool.AllocateThreads(simulationThreadCount);
	
	SPH::System* system = nullptr;	
	SPHSystemRenderer* systemRenderer = nullptr;
	SPHSystemRenderCache* systemRenderCache = nullptr;	

	auto NewSystem = [&](SPH::System* newSystem, SPHSystemRenderer* newRenderer, SPHSystemRenderCache* newRenderCache)
		{
			highlightedParticle = SIZE_MAX;
			selectedParticle = SIZE_MAX;

			delete system;
			delete systemRenderer;
			delete systemRenderCache;
			system = nullptr;
			systemRenderer = nullptr;
			systemRenderCache = nullptr;

			system = newSystem;
			if (system != nullptr)
			{
				systemRenderer = newRenderer;
				systemRenderCache = newRenderCache;
				system->Initialize(systemInitParams);

				uiScreen.SetParticleCount(system->GetParticleCount());				
				uiScreen.SetImplmenetationName(system->SystemImplementationName());

				systemRenderCache->LinkSPHSystem(system);
				systemRenderCache->SetModelMatrix(SPHSystemModelMatrix);

				renderingSystem.SetSPHSystemRenderer(systemRenderer);
				renderingSystem.SetSPHSystemRenderingCache(systemRenderCache);
			}
			else
			{
				uiScreen.SetParticleCount(0);				
				uiScreen.SetImplmenetationName(StringUTF8());
			}			
		};
	NewSystem(
		new SPH::SystemGPU(openCLContext),
		new SPHSystemGPURenderer(renderingSystem.GetGraphicsContext()),
		new SPHSystemGPURenderCache()
	);
	
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

		if (uiInputManager.GetSelectedNode() == nullptr)
		{
			if (window.GetLastKeyState(Key::X).pressed)
				if (window.GetLastKeyState(Key::LShift).down)
					runSimulation = false;
				else
					runSimulation = true;

			//if (window.GetLastKeyState(Key::T).pressed)
			//	if (system == nullptr || system->SystemImplementationName() == "CPU")
			//		NewSystem(new SPH::SystemGPU(openCLContext));
			//	else
			//		NewSystem(new SPH::SystemCPU(threadPool));
			//
			if (window.GetLastKeyState(Key::R).pressed)
				system->Initialize(systemInitParams);
		}

		if (system != nullptr && (runSimulation || window.GetLastKeyState(Key::Right).pressed && uiInputManager.GetSelectedNode() == nullptr))
			system->Update(std::min(dt, 0.01f));		
							

		if (uiInputManager.GetSelectedNode() == nullptr)
			UpdateCamera(window, dt);		

		//auto particles = system->GetParticles();

		if (system != nullptr)
		{
			//for (auto& particle : particles)
			//{
			//
			//	if (particle.pressure != FLT_MAX)
			//	{
			//		float t = exp(-particle.velocity.Lenght() / 10.0f);
			//		t = std::clamp(t, 0.0f, 1.0f);
			//		Vec3f color = Vec3f(1, 1, 1) * t + Vec3f(1, 0, 0) * (1 - t);
			//		particle.color = Vec4f(color, 1);
			//	}
			//	else
			//		particle.color = Vec4f(1, 1, 1, 0.1f);
			//}

			//if (window.GetLastKeyState(Key::LCtrl).down && uiInputManager.GetSelectedNode() == nullptr)
			//{
			//	uintMem index = systemRenderCache.GetClosestParticleToScreenPos((Vec2f)window.GetMousePos(), renderingSystem);				
			//	
			//	highlightedParticle = index;						
			//
			//	if (window.GetLastKeyState(Key::MouseLeft).pressed)
			//		selectedParticle = highlightedParticle;
			//}

			//if (highlightedParticle != SIZE_MAX)
			//	particles[highlightedParticle].color = Vec4f(1, 0, 0, 1);
			//
			//if (selectedParticle != SIZE_MAX)
			//	particles[selectedParticle].color = Vec4f(0, 1, 0, 1);
			//
			//if (selectedParticle != SIZE_MAX)
			//{			
			//	SPH::Particle particle = particles[selectedParticle];
			//	auto neighborTable = system->FindNeighbors(particles, particle.position);												
			//
			//	for (auto& index : neighborTable)
			//		if (index != selectedParticle)
			//			particles[index].color = Vec4f(0, 0, 1, 1);
			//
			//	//if (window.GetLastKeyState(Key::MouseLeft).down)
			//	//{
			//	//	Vec3f dir = cameraRot * Vec3f((Vec2f)Input::GetDesktopMouseMovement(), 0);
			//	//
			//	//	dir *= 10;
			//	//	system.ApplyForceToArea(selectedParticle->position, dir);
			//	//}
			//
			//	uiScreen.SetInfo(
			//		"Neighbor count: " + StringParsing::Convert(neighborTable.Count()) + "\n"
			//		"Position: " + StringParsing::Convert(particle.position.x).Resize(7, '0') + ", " + StringParsing::Convert(particle.position.y).Resize(7, '0') + ", " + StringParsing::Convert(particle.position.z).Resize(7, '0') + "\n"
			//		"Velocity: " + StringParsing::Convert(particle.velocity.x).Resize(7, '0') + ", " + StringParsing::Convert(particle.velocity.y).Resize(7, '0') + ", " + StringParsing::Convert(particle.velocity.z).Resize(7, '0') + "\n"
			//		"Pressure: " + StringParsing::Convert(particle.pressure) + "\n"					
			//	);										  
			//}
		}		
				
		renderingSystem.Render();

		++FPSCount;
		if (FPSStopwatch.GetTime() > 1.0f)
		{
			uiScreen.SetFPS(FPSCount);
			FPSStopwatch.Reset();
			FPSCount = 0;
		}		
	}

	delete system;
	Debug::Logger::RemoveOutputStream(writeStream);
}