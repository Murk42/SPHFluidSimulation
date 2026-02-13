#include "pch.h"
#include "SPH/OpenCL/OpenCLContext.h"

#include "Scenes/Scene.h"
#include "Scenes/SimulationVisualisationScene/SimulationVisualisationScene.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"

CLIENT_API void Setup()
{
	Debug::Logger::AddOutputFile("outputs/log.txt");

	bool exitApp = false;

	//Setup rendering and window
	Graphics::OpenGL::GraphicsContext_OpenGL graphicsContext{ Graphics::OpenGL::GraphicsContextProperties_OpenGL{
		.contextFlags = Graphics::OpenGL::ContextFlags::Debug
	} };
	Graphics::OpenGL::RenderWindow_OpenGL window{ graphicsContext, { } };
	window.SetHiddenFlag(false);
	window.Raise();

	//Setup OpenCL
	OpenCLContext clContext{ graphicsContext };
	cl_command_queue clQueue = clContext.GetCommandQueue(true, true);

	LambdaEventHandler<Window::CloseEvent> windowCloseEventHandler{
		[&](auto event) { exitApp = true; }
	};
	window.closeEventDispatcher.AddHandler(windowCloseEventHandler);

	std::function<SceneBlueprint* ()> sceneCreators[]{
		[&]() { return new SimulationVisualisationScene(clContext, clQueue, window); },
		[&]() { return new ProfilingScene(clContext, clQueue, window); },
	};
	uintMem currentSceneIndex = 0;
	SceneBlueprint* currentScene = nullptr;

	currentScene = sceneCreators[currentSceneIndex]();

	while (!exitApp)
	{
		Input::Update();		

		if (currentScene)
		{
			Input::GenericInputEvent event;
			while (window.ProcessInputEvent(event))
			{
				if (event.TryProcess([&](const Input::KeyDownEvent& event)
					{
						if (event.key == Input::Key::TAB)
						{
							delete currentScene;
							currentSceneIndex = (currentSceneIndex + 1) % _countof(sceneCreators);
							currentScene = sceneCreators[currentSceneIndex]();
							return true;
						}
						return false;
					}))
					continue;

				currentScene->OnEvent(event);
			}

			currentScene->Update();

			currentScene->Render({ window.GetSize() });
		}

	}

	delete currentScene;
}