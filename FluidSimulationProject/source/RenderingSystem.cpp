#include "pch.h"
#include "RenderingSystem.h"

Graphics::OpenGL::GraphicsContextProperties_OpenGL graphicsContextProperties{	
	.majorVersion = 4,
	.minorVersion = 5,	
};
WindowCreateOptions windowCreateOptions{		
	.hidden = true,
	.title = "SPH fluid simulation",
	.size = Vec2u(1920, 1080) / 10 * 7,
	.presentMode = WindowPresentMode::Maximized,	
};

RenderingSystem::RenderingSystem() :
	graphicsContext(graphicsContextProperties),
	renderWindow(graphicsContext, windowCreateOptions),
	sphSystemRenderer(nullptr),
	texturedRectRenderer(graphicsContext),
	coloredCharacterRenderer(graphicsContext),
	line2DRenderer(graphicsContext),
	panelRenderer(graphicsContext),
	UIRenderPipeline(texturedRectRenderer, coloredCharacterRenderer, panelRenderer),
	sphSystemRenderCache(nullptr)
{
	graphicsContext.SetActiveRenderWindow(renderWindow);
	graphicsContext.SetClearColor(clearColor);	
	graphicsContext.SetRenderArea(Vec2i(), renderWindow.GetSize());		

	//if (!graphicsContext.SetSwapInterval(Graphics::OpenGL::WindowSwapInterval::AdaptiveVSync))
	//	Debug::Logger::LogInfo("Client", "Asked for adaptive VSync but its not supported");
	graphicsContext.SetSwapInterval(Graphics::OpenGL::WindowSwapInterval::None);

	windowResizedEvent.SetFunction([&](auto event) {
		graphicsContext.Flush();
		graphicsContext.SetRenderArea(Vec2i(), event.size);
		});
	renderWindow.GetWindow().windowResizedEventDispatcher.AddHandler(windowResizedEvent);
}

RenderingSystem::~RenderingSystem()
{
	renderWindow.GetWindow().windowResizedEventDispatcher.RemoveHandler(windowResizedEvent);
}

void RenderingSystem::SetProjection(const Mat4f& matrix)
{
	this->projectionMatrix = matrix;	
}

void RenderingSystem::SetViewMatrix(const Mat4f& matrix)
{
	this->viewMatrix = matrix;	
}

void RenderingSystem::SetSPHSystemRenderingCache(SPH::SystemRenderCache* renderCache)
{
	sphSystemRenderCache = renderCache;
}

void RenderingSystem::SetSPHSystemRenderer(SPH::SystemRenderer* renderer)
{
	sphSystemRenderer = renderer;
}

void RenderingSystem::SetScreen(UI::Screen* screen)
{
	UIRenderPipeline.SetScreen(screen);			
}

void RenderingSystem::Render()
{
	graphicsContext.ClearTarget();

	graphicsContext.EnableDepthTest(true);
	
	if (sphSystemRenderCache != nullptr)	
		sphSystemRenderer->Render(*sphSystemRenderCache, viewMatrix, projectionMatrix);		
	
	graphicsContext.EnableDepthTest(false);

	UIRenderPipeline.Render(renderWindow.GetSize());

	renderWindow.GetWindow().SwapBuffers();
}

void RenderingSystem::SetCustomClearColor(ColorRGBAf color)
{
	graphicsContext.SetClearColor(color);
}

void RenderingSystem::DisableCustomClearColor()
{
	graphicsContext.SetClearColor(clearColor);
}
