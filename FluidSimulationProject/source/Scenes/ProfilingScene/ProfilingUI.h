#pragma once

class ProfilingScene;

class ProfilingUI : public UI::Screen
{
public:
	ProfilingScene& scene;

	UI::Nodes::Label titleText;
	UI::Nodes::Label profilingStateText;
	UI::Nodes::Label profilingLog;
	UI::Nodes::Label profilingPercent;
	UI::Nodes::PanelButton startProfilingButton;

	String log;

	ProfilingUI(ResourceManager& resourceManager, ProfilingScene& scene);
	~ProfilingUI();

	void ProfilingStopped();

	void SetProfilingPercent(float percent);
	void LogProfiling(String log);
};