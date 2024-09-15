#pragma once

using namespace Blaze;

class UIScreen :
	public UI::Screen
{
	uint FPS;
	uint particleCount;
	StringUTF8 info;
	StringUTF8 implementationName;
public:
	Font font = Font::LoadDefault();
	UI::Text infoText;
	UI::EditableText viscosityText;

	UIScreen();

	void SetFPS(uintMem FPS);
	void SetParticleCount(uintMem particleCount);
	void SetInfo(StringUTF8 text);
	void SetImplmenetationName(StringUTF8 implementationName) { this->implementationName = implementationName; ReconstructInfoText(); }
private:
	void ReconstructInfoText();
};