#pragma once

class ProfilingScene;

class ProfilingUI : public UI::Screen
{
public:
	ProfilingScene& scene;

	Font font = Font::LoadDefault();
	UI::Nodes::Text titleText;
	UI::Nodes::Text profilingStateText;
	UI::Nodes::Text profilingLog;
	UI::Nodes::Text profilingPercent;
	UI::Nodes::TextButton startProfilingButton;	

	ProfilingUI(ProfilingScene& scene, Window* window);
	~ProfilingUI();

	void ProfilingStopped();

	void SetProfilingPercent(float percent);
	void LogProfiling(String log);
};