#include "pch.h"
#include "RenderingSystem.h"

Graphics::OpenGL::GraphicsContextProperties_OpenGL graphicsContextProperties{	
	.majorVersion = 4,
	.minorVersion = 5,
};
Graphics::OpenGL::WindowSDLCreateOptions_OpenGL windowCreateOptions{	
	.title = "SPH fluid simulation",
	.size = Vec2u(1920, 1080) / 10 * 7,
	.openMode = WindowSDLOpenMode::Normal,	
	.styleFlags = WindowSDLStyleFlags::Resizable,
};

RenderingSystem::RenderingSystem() :
	graphicsContext(graphicsContextProperties),
	renderWindow(graphicsContext, windowCreateOptions),
	sphSystemRenderer(nullptr),
	texturedRectRenderer(graphicsContext),
	line2DRenderer(graphicsContext),
	panelRenderer(graphicsContext),
	UIRenderPipeline(texturedRectRenderer, panelRenderer),
	sphSystemRenderCache(nullptr)
{
	graphicsContext.SetClearColor(clearColor);	

	windowResizedEvent.SetFunction([&](auto event) {
		graphicsContext.Flush();
		graphicsContext.SetRenderArea(Vec2i(), event.size);
		});
	renderWindow.GetWindowSDL().resizedEventDispatcher.AddHandler(windowResizedEvent);
	graphicsContext.SetRenderArea(Vec2i(), renderWindow.GetSize());		
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

	renderWindow.GetWindowSDL().SwapBuffers();
}

void RenderingSystem::SetCustomClearColor(ColorRGBAf color)
{
	graphicsContext.SetClearColor(color);
}

void RenderingSystem::DisableCustomClearColor()
{
	graphicsContext.SetClearColor(clearColor);
}
