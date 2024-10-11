#include "pch.h"
#include "RenderingSystem.h"
#include "OpenCLContext.h"

#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"

CLIENT_API void Setup()
{
	Debug::Logger::AddOutputFile("log.txt");

	bool exitApp = false;

	//Setup rendering and window
	RenderingSystem renderingSystem;
	Window& window = renderingSystem.GetWindow();
	window.Maximize();
	window.ShowWindow(true);
	window.Raise();

	renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(120 * Math::PI / 180, (float)window.GetSize().x / window.GetSize().y, 0.1, 1000));

	//Setup processing pools	
	OpenCLContext CLContext{ renderingSystem.GetGraphicsContext() };

	LambdaEventHandler<Input::Events::WindowCloseEvent> windowCloseEventHandler{
		[&](auto event) { exitApp = true; }
	};
	window.closeEventDispatcher.AddHandler(windowCloseEventHandler);
	LambdaEventHandler<Input::Events::WindowResizedEvent> windowResizedEventHandler{
		[&](auto event) {
			renderingSystem.SetProjection(Mat4f::PerspectiveMatrix(Math::PI / 2, (float)event.size.x / event.size.y, 0.1, 1000));
			}
	};
	window.resizedEventDispatcher.AddHandler(windowResizedEventHandler);

	std::function<Scene* ()> sceneCreators[]{
		[&]() { return new ProfilingScene(CLContext, renderingSystem); },
		[&]() { return new SimulationVisualisationScene(CLContext, renderingSystem); },
	};
	uintMem currentSceneIndex = 0;
	Scene* currentScene = nullptr;

	currentScene = sceneCreators[currentSceneIndex]();
	
	while (!exitApp)
	{
		Input::Update();				

		if (window.GetLastKeyState(Key::Tab).pressed)
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