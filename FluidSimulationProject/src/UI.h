#pragma once

class CameraMouseFocusNode : 
	public UI::InputNode,	
	public UI::UISelectEventHandler,
	public UI::UIMouseEventHandler,
	public UI::UIKeyboardEventHandler
{
public:
	
private:
	void OnEvent(UI::UISelectEventHandler::SelectedEvent event);
	void OnEvent(UI::UISelectEventHandler::DeselectedEvent event);
	void OnEvent(UI::UIKeyboardEventHandler::KeyPressedEvent event);
	void OnEvent(UI::UIMouseEventHandler::MousePressedEvent event);
	void OnEvent(UI::UIMouseEventHandler::MouseEnterEvent event);

	bool HitTest(Vec2f) override;
};

class EditableText : public UI::Node, Graphics::RenderObject
{
public:
	UIGraphics::PanelNodeRenderUnit panelRenderUnit;
	UI::EditableText editableText;

	LambdaEventHandler<UI::Node::TransformUpdatedEvent> transformChangedEventHandler;

	EditableText();
private:
	Graphics::RenderUnit* GetRenderUnit(uint index) override;
};

class UIScreen :
	public UI::Screen
{
	uint FPS;
	uint particleCount;
	StringUTF8 info;
	StringUTF8 implementationName;
public:
	Font font = Font::LoadDefault();	
	CameraMouseFocusNode cameraMouseFocusNode;
	UI::Text infoText;
	//EditableText viscosityText;

	UIScreen();

	void SetFPS(uintMem FPS);
	void SetParticleCount(uintMem particleCount);
	void SetInfo(StringUTF8 text);
	void SetImplenetationName(StringUTF8 implementationName) { this->implementationName = implementationName; ReconstructInfoText(); }
private:
	void ReconstructInfoText();
};