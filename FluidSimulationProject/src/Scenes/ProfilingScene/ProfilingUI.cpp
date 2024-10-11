#include "pch.h"
#include "ProfilingUI.h"

ProfilingUI::ProfilingUI()
{
	Graphics::FontAtlasesData::AddToFont(font, { 12, 20, 28 }, CharacterSet::ASCIICharacterSet());

	titleText.SetParent(this);
	titleText.SetTransform({
		.pos = Vec2f(0, -24),
		.parentPivot = Vec2f(0.5f, 1.0f),
		.pivot = Vec2f(0.5f, 1.0f),
		});
	titleText.SetFont(font);
	titleText.SetFontHeight(28);
	titleText.SetText("Simulation profiling");

	profilingStateText.SetParent(this);
	profilingStateText.SetTransform({
		.pos = Vec2f(10, -50),
		.parentPivot = Vec2f(0.0f, 1.0f),
		.pivot = Vec2f(0.0f, 1.0f),
		});
	profilingStateText.SetFont(font);
	profilingStateText.SetFontHeight(20);
	profilingStateText.SetTextColor(0xffffffff);
	profilingStateText.SetText("Not profiling");
	profilingStateText.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.lineVerticalAlign = UI::TextLineVerticalAlign::Center,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,
		});

	starProfilingButton.SetParent(&profilingStateText);
	starProfilingButton.SetTransform({
		.pos = Vec2f(0, -5),
		.parentPivot = Vec2f(0.0f, 0.0f),
		.pivot = Vec2f(0.0f, 1.0f),
		.size = Vec2f(200, 40)
		});		
	starProfilingButton.textRenderUnit.SetFont(font);
	starProfilingButton.textRenderUnit.SetFontHeight(20);
	starProfilingButton.textRenderUnit.SetText("Start profiling");		
	starProfilingButton.pressedEventDispatcher.AddHandler(startStopProfilingButtonPressedEventHandler);

	startStopProfilingButtonPressedEventHandler.SetFunction([&](auto event) {		
		profilingStateText.SetText("Profiling");
		profilingStateText.SetTextColor(0xff0000ff);			
		profilingLog.SetText("");
		starProfilingButton.Disable();
		});

	profilingPercent.SetParent(&starProfilingButton);
	profilingPercent.SetTransform({
		.pos = Vec2f(5, 0),
		.parentPivot = Vec2f(1.0f, 0.5f),
		.pivot = Vec2f(0.0f, 0.5f),
		});
	profilingPercent.SetFont(font);
	profilingPercent.SetFontHeight(20);
	profilingPercent.SetTextColor(0xffffffff);
	profilingPercent.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.lineVerticalAlign = UI::TextLineVerticalAlign::Center,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,
		});

	profilingLog.SetParent(&starProfilingButton);
	profilingLog.SetTransform({
		.pos = Vec2f(0, -5),
		.parentPivot = Vec2f(0.0f, 0.0f),
		.pivot = Vec2f(0.0f, 1.0f),		
		});
	profilingLog.SetFont(font);
	profilingLog.SetFontHeight(12);
	profilingLog.SetTextColor(0xffffffff);	
	profilingLog.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.lineVerticalAlign = UI::TextLineVerticalAlign::Top,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,
		});
}

ProfilingUI::~ProfilingUI()
{	
}

void ProfilingUI::ProfilingStopped()
{	
	profilingStateText.SetTextColor(0xffffffff);
	profilingStateText.SetText("Not profiling");
	profilingPercent.SetText("");
	starProfilingButton.Enable();
}

void ProfilingUI::SetProfilingPercent(float percent)
{
	percent = std::clamp(percent, 0.0f, 1.0f);
	profilingPercent.SetText(StringParsing::Convert((uint)(percent * 100)) + "%");
}

void ProfilingUI::LogProfiling(String log)
{
	auto text = profilingLog.GetText();
	profilingLog.SetText(text + log);
}

TextButton::TextButton() :
	textRenderUnit(&textNode), panelRenderUnit(this)
{
	textNode.SetParent(this);
	textNode.SetTransform({
		.parentPivot = Vec2f(0.5f),
		.pivot = Vec2f(0.5f, 0.4f),
		});
	
	textRenderUnit.SetTextColor(textColor);
	textRenderUnit.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Center,
		.lineVerticalAlign = UI::TextLineVerticalAlign::Center,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::Nothing,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::Nothing,
		});

	panelRenderUnit.SetFillColor(normalColor);
	panelRenderUnit.SetCornerRadius(5);
	panelRenderUnit.SetBorderWidth(0);
}

Graphics::RenderUnit* TextButton::GetRenderUnit(uint index)
{
	switch (index)
	{
	case 0:
		return &panelRenderUnit;
	case 1:
		return &textRenderUnit;
	default:
		return nullptr;
	}
}

void TextButton::Disable()
{
	panelRenderUnit.SetFillColor(disabledColor);
	textRenderUnit.SetTextColor(disabledTextColor);
	disabled = true;
}

void TextButton::Enable()
{
	disabled = false;
	textRenderUnit.SetTextColor(textColor);
	if (highlighted)
		panelRenderUnit.SetFillColor(highlightedColor);
}

void TextButton::OnEvent(MouseEnterEvent event)
{
	if (!disabled)	
		panelRenderUnit.SetFillColor(highlightedColor);
	
	highlighted = true;
}

void TextButton::OnEvent(MouseExitEvent event)
{
	if (!disabled)
		panelRenderUnit.SetFillColor(normalColor);

	highlighted = false;	
}

void TextButton::OnEvent(MouseReleasedEvent event)
{
	if (!disabled && pressed)
	{
		panelRenderUnit.SetFillColor(highlightedColor);
		pressed = false;
	}
}

void TextButton::OnEvent(MousePressedEvent event)
{
	if (!disabled)
	{
		pressed = true;
		panelRenderUnit.SetFillColor(pressedColor);
		pressedEventDispatcher.Call({ this });
	}
}
