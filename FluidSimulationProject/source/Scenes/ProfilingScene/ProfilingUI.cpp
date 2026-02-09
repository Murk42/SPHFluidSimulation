#include "pch.h"
#include "ProfilingUI.h"
#include "ProfilingScene.h"

ProfilingUI::ProfilingUI(ResourceManager& resourceManager, ProfilingScene& scene)
	: Screen(resourceManager), scene(scene)
{

	titleText.SetParent(this);
	titleText.SetTransform({ .pos = Vec2f(0, -24), .parentPivot = Vec2f(0.5f, 1.0f), .pivot = Vec2f(0.5f, 1.0f), });
	titleText.SetText("Simulation profiling");
	titleText.SetTextStyle({ .fontName = "default", .fontHeight = 28 });

	profilingStateText.SetParent(this);
	profilingStateText.SetTransform({ .pos = Vec2f(10, -50), .parentPivot = Vec2f(0.0f, 1.0f), .pivot = Vec2f(0.0f, 1.0f) });
	profilingStateText.SetText("Not profiling");
	profilingStateText.SetTextStyle({ .fontName = "default", .fontHeight = 20 });

	startProfilingButton.SetParent(&profilingStateText);
	startProfilingButton.SetTransform({ .pos = Vec2f(0, -5), .parentPivot = Vec2f(0.0f, 0.0f), .pivot = Vec2f(0.0f, 1.0f), .size = Vec2f(200, 40) });
	//startProfilingButton.textRenderUnit.SetFont(font);
	//startProfilingButton.textRenderUnit.SetFontHeight(20);
	//startProfilingButton.textRenderUnit.SetText("Start profiling");
	startProfilingButton.SetPressedEventCallback([&](auto event) {
		profilingStateText.SetText("Profiling");
		profilingLog.SetText("");
		startProfilingButton.Disable();
		scene.StartProfiling();
		});

	profilingPercent.SetParent(&startProfilingButton);
	profilingPercent.SetTransform({ .pos = Vec2f(5, 0), .parentPivot = Vec2f(1.0f, 0.5f), .pivot = Vec2f(0.0f, 0.5f), });
	profilingPercent.SetText("");
	profilingPercent.SetTextStyle({ .fontName = "default", .fontHeight = 20 });

	profilingLog.SetParent(this);
	profilingLog.SetTransform({ .pos = Vec2f(5, 5), .parentPivot = Vec2f(0.0f, 0.0f), .pivot = Vec2f(0.0f, 0.0f), .size = Vec2f(1000, 950), });
}

ProfilingUI::~ProfilingUI()
{
}

void ProfilingUI::ProfilingStopped()
{
	profilingStateText.SetText("Not profiling");
	profilingPercent.SetText("");
	startProfilingButton.Enable();
}

void ProfilingUI::SetProfilingPercent(float percent)
{
	percent = std::clamp(percent, 0.0f, 1.0f);
	profilingPercent.SetText(String::Parse((uint)(percent * 100)) + "%");
}

void ProfilingUI::LogProfiling(String log)
{
	this->log += log;
	profilingLog.SetText(log);
}