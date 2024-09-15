#pragma once
#include "SPH/SPHSystemRenderer.h"

class RenderingSystem
{
public:
	RenderingSystem();
	
	void SetProjection(const Mat4f&);
	void SetViewMatrix(const Mat4f&);	
	void SetSPHSystemRenderingCache(SPHSystemRenderCache* renderCache);
	void SetSPHSystemRenderer(SPHSystemRenderer* renderer);
	
	void SetScreen(UI::Screen* screen);

	void Render();

	inline Window& GetWindow() { return renderWindow.GetWindowSDL(); }
	inline Graphics::OpenGL::GraphicsContext_OpenGL& GetGraphicsContext() { return graphicsContext; }
	inline const Mat4f& GetViewMatrix() const { return viewMatrix; }
	inline const Mat4f& GetProjectionMatrix() const { return projectionMatrix; }
private:
	Graphics::OpenGL::GraphicsContext_OpenGL graphicsContext;
	Graphics::OpenGL::RenderWindow_OpenGL renderWindow;
	Graphics::OpenGL::TexturedRectRenderer_OpenGL texturedRectRenderer;
	Graphics::OpenGL::Line2DRenderer_OpenGL line2DRenderer;

	SPHSystemRenderer* sphSystemRenderer;
	Graphics::OpenGL::PanelRenderer_OpenGL panelRenderer;
	Graphics::OpenGL::UIRenderPipeline_OpenGL UIRenderPipeline;

	LambdaEventHandler<Input::Events::WindowResizedEvent> windowResizedEvent;	

	Mat4f projectionMatrix;
	Mat4f viewMatrix;	
	
	SPHSystemRenderCache* sphSystemRenderCache;
};