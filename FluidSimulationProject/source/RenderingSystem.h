#pragma once
#include "SPH/Graphics/SystemRenderer.h"

class RenderingSystem
{
public:
	RenderingSystem();
	~RenderingSystem();
	
	void SetProjection(const Mat4f&);
	void SetViewMatrix(const Mat4f&);		
	void SetSPHSystemRenderer(SPH::SystemRenderer* renderer);
	
	void SetScreen(UI::Screen* screen);

	void Render(ArrayView<SPH::SystemRenderCache&> renderCaches);

	void SetCustomClearColor(ColorRGBAf color);
	void DisableCustomClearColor();

	inline Window& GetWindow() { return renderWindow.GetWindow(); }
	inline Graphics::OpenGL::GraphicsContext_OpenGL& GetGraphicsContext() { return graphicsContext; }
	inline const Mat4f& GetViewMatrix() const { return viewMatrix; }
	inline const Mat4f& GetProjectionMatrix() const { return projectionMatrix; }
private:
	ColorRGBAf clearColor = 0x101510ff;

	Graphics::OpenGL::GraphicsContext_OpenGL graphicsContext;
	Graphics::OpenGL::RenderWindow_OpenGL renderWindow;
	Graphics::OpenGL::TexturedRectRenderer_OpenGL texturedRectRenderer;
	Graphics::OpenGL::ColoredCharacterRenderer_OpenGL coloredCharacterRenderer;
	Graphics::OpenGL::Line2DRenderer_OpenGL line2DRenderer;

	SPH::SystemRenderer* sphSystemRenderer;
	Graphics::OpenGL::PanelRenderer_OpenGL panelRenderer;
	Graphics::OpenGL::UIRenderPipeline_OpenGL UIRenderPipeline;

	LambdaEventHandler<Window::WindowResizedEvent> windowResizedEvent;

	Mat4f projectionMatrix;
	Mat4f viewMatrix;	
	
	SPH::SystemRenderCache* sphSystemRenderCache;
};