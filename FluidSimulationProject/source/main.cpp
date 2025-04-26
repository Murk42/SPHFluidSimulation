#include "pch.h"
#include "RenderingSystem.h"
#include "OpenCLContext.h"

#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"

CLIENT_API void Setup()
{
	Debug::Logger::AddOutputFile("outputs/log.txt");

	bool exitApp = false;

	//Setup rendering and window
	RenderingSystem renderingSystem;
	Window& window = renderingSystem.GetWindow();
	window.SetHiddenFlag(false);
	window.Raise();	

	renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));

	//Setup OpenCL
	OpenCLContext clContext{ renderingSystem.GetGraphicsContext() };	
	cl_command_queue clQueue = clContext.GetCommandQueue(true, true);		

	LambdaEventHandler<Window::WindowCloseEvent> windowCloseEventHandler{
		[&](auto event) { exitApp = true; }
	};
	window.windowCloseEventDispatcher.AddHandler(windowCloseEventHandler);
	LambdaEventHandler<Window::WindowResizedEvent> windowResizedEventHandler{
		[&](auto event) {
			renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(Math::PI / 2 * 1.4f, (float)event.size.x / event.size.y, 0.1, 1000));
			}
	};
	window.windowResizedEventDispatcher.AddHandler(windowResizedEventHandler);

	std::function<Scene* ()> sceneCreators[]{
		[&]() { return new SimulationVisualisationScene(clContext, clQueue, renderingSystem); },
		[&]() { return new ProfilingScene(clContext, clQueue, renderingSystem); },
	};
	uintMem currentSceneIndex = 0;
	Scene* currentScene = nullptr;

	currentScene = sceneCreators[currentSceneIndex]();
	
	while (!exitApp)
	{
		Input::Update();				

		if (Keyboard::GetFrameKeyState(Keyboard::Key::TAB).pressed)
		{
			delete currentScene;
			currentSceneIndex = (currentSceneIndex + 1) % _countof(sceneCreators);
			currentScene = sceneCreators[currentSceneIndex]();			
		}

		if (currentScene)
			currentScene->Update();		

	}

	delete currentScene;
}