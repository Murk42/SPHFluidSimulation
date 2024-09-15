#include "pch.h"
#include "UI.h"

UIScreen::UIScreen()
{
	Graphics::FontAtlasesData::AddToFont(font, { 12, 20}, CharacterSet::ASCIICharacterSet());

	infoText.SetParent(this);
	infoText.SetTransform({
		.pos = Vec2f(2, 2),
		.parentPivot = Vec2f(0, 0),
		.pivot = Vec2f(0, 0),
		});
	infoText.SetFont(font);
	infoText.SetFontHeight(12);
	infoText.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,
		});
	ReconstructInfoText();	

	viscosityText.SetParent(this);
	viscosityText.SetTransform({
		.pos = Vec2f(5, -5),
		.parentPivot = Vec2f(0, 1),
		.pivot = Vec2f(0, 1)
		});
	viscosityText.textRenderUnit.SetFont(font);
	viscosityText.textRenderUnit.SetFontHeight(20);
	viscosityText.textRenderUnit.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,		
		});
	viscosityText.SetMultilineInput(false);
	viscosityText.SetEmptyText("Enter viscosity value");	
}

void UIScreen::SetFPS(uintMem FPS)
{
	this->FPS = FPS;
	ReconstructInfoText();
}

void UIScreen::SetParticleCount(uintMem particleCount)
{
	this->particleCount = particleCount;
}

void UIScreen::SetInfo(StringUTF8 text)
{
	this->info = text;
	ReconstructInfoText();
}

void UIScreen::ReconstructInfoText()
{
	StringUTF8 text;
	if (!info.Empty())
		text += info + "\n\n";

	text += "Number of particles: " + StringParsing::Convert(particleCount) + "\n\n";

	if (implementationName.Empty())
		text += "T to rotate implementations. No implementation is selected currently.\n";
	else
		text += "T to rotate implementations. Current implementation is \"" + implementationName + "\"\n";

	text +=
		"X to start the simulation\n"
		"Shift+X to stop the simulation\n"
		"RightArrow to step the simulation\n"
		"FPS: " + StringParsing::Convert(FPS);

	infoText.SetText(text);
}
