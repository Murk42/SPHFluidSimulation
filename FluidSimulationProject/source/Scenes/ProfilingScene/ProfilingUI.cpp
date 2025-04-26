#include "pch.h"
#include "ProfilingUI.h"
#include "ProfilingScene.h"

ProfilingUI::ProfilingUI(ProfilingScene& scene, Window* window)
	: scene(scene), Screen(window)
{
	Graphics::FontAtlasesData::AddToFont(font, ArrayView<uint>{ 12, 20, 28 }, CharacterSet::ASCIICharacterSet());

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

	startProfilingButton.SetParent(&profilingStateText);
	startProfilingButton.SetTransform({
		.pos = Vec2f(0, -5),
		.parentPivot = Vec2f(0.0f, 0.0f),
		.pivot = Vec2f(0.0f, 1.0f),
		.size = Vec2f(200, 40)
		});		
	startProfilingButton.textRenderUnit.SetFont(font);
	startProfilingButton.textRenderUnit.SetFontHeight(20);
	startProfilingButton.textRenderUnit.SetText("Start profiling");		
	startProfilingButton.pressedEventCallback = [&](auto event) {
		profilingStateText.SetText("Profiling");
		profilingStateText.SetTextColor(0xff0000ff);
		profilingLog.SetText("");
		startProfilingButton.Disable();
		scene.StartProfiling();
		};	

	profilingPercent.SetParent(&startProfilingButton);
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

	profilingLog.SetParent(this);
	profilingLog.SetTransform({
		.pos = Vec2f(5, 5),
		.parentPivot = Vec2f(0.0f, 0.0f),
		.pivot = Vec2f(0.0f, 0.0f),		
		.size = Vec2f(1000, 950),
		});
	profilingLog.SetCullingNode(&profilingLog);
	profilingLog.SetFont(font);
	profilingLog.SetFontHeight(12);
	profilingLog.SetTextColor(0xffffffff);	
	profilingLog.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.lineVerticalAlign = UI::TextLineVerticalAlign::Bottom,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::Nothing,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::Nothing,		
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
	startProfilingButton.Enable();
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