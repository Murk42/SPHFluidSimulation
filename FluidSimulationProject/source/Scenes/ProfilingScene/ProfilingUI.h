#pragma once

class TextButton : 
	public UI::InputNode,
	public UI::UIMouseEventHandler,
	public Graphics::RenderObject
{
public:
	struct PressedEvent
	{
		TextButton* button;
	};

	ColorRGBAf normalColor = 0x222222ff;
	ColorRGBAf highlightedColor = 0x303030ff;
	ColorRGBAf pressedColor = 0x161616ff;
	ColorRGBAf disabledColor = 0x181818ff;
	ColorRGBAf textColor = 0xffffffff;
	ColorRGBAf disabledTextColor = 0xbbbbbbff;
	bool disabled = false;
	bool highlighted = false;
	bool pressed = false;

	UI::Node textNode;
	UIGraphics::PanelNodeRenderUnit panelRenderUnit;
	UIGraphics::TextRenderUnit textRenderUnit;

	EventDispatcher<PressedEvent> pressedEventDispatcher;

	TextButton();

	Graphics::RenderUnit* GetRenderUnit(uint index) override;

	void Disable();
	void Enable();
private:
	void OnEvent(MouseEnterEvent event) override;
	void OnEvent(MouseExitEvent event) override;
	void OnEvent(MouseReleasedEvent event) override;
	void OnEvent(MousePressedEvent event) override;
};

class ProfilingUI : public UI::Screen
{
public:
	Font font = Font::LoadDefault();
	UI::Text titleText;
	UI::Text profilingStateText;
	UI::Text profilingLog;
	UI::Text profilingPercent;
	TextButton starProfilingButton;
	LambdaEventHandler<TextButton::PressedEvent> startStopProfilingButtonPressedEventHandler;	

	ProfilingUI();
	~ProfilingUI();

	void ProfilingStopped();

	void SetProfilingPercent(float percent);
	void LogProfiling(String log);
};