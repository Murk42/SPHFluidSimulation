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
		.lineVerticalAlign = UI::TextLineVerticalAlign::Top,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,		
		});
	ReconstructInfoText();	
	infoText.CleanTransform();
	infoText.CleanFinalTransform();

	viscosityText.SetParent(this);
	viscosityText.SetTransform({
		.pos = Vec2f(10, -10),
		.parentPivot = Vec2f(0, 1),
		.pivot = Vec2f(0, 1),
		.size = Vec2f(100, 24)
		});	
	viscosityText.editableText.textRenderUnit.SetFont(font);
	viscosityText.editableText.textRenderUnit.SetFontHeight(20);
	viscosityText.editableText.textRenderUnit.SetLayoutOptions({
		.lineHorizontalAlign = UI::TextLineHorizontalAlign::Left,
		.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::ResizeToFit,
		.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit,		
		});
	viscosityText.editableText.SetMultilineInput(false);
	viscosityText.editableText.SetEmptyText("Enter viscosity value");		

	cameraMouseFocusNode.SetParent(this);
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
		text += "T to rotate implementations. No implementation is selected currently.\n\n";
	else
		text += "T to rotate implementations. Current implementation is \"" + implementationName + "\"\n\n";

	text +=		
		"Input controls:\n"
		"\tX to start the simulation\n"
		"\tShift+X to stop the simulation\n"
		"\tRightArrow to step the simulation\n"
		"\tR to reset the simulation\n"
		"\tClick the screen to enter camera move mode\n"
		"\tESC to exit camera move mode\n"
		"\tIn camera move mode use W, A, S, D and the mouse to move the camera\n"		
		"FPS: " + StringParsing::Convert(FPS);

	infoText.SetText(text);
}

extern bool cameraHasMouseFocus;

void CameraMouseFocusNode::OnEvent(UI::UISelectEventHandler::SelectedEvent event)
{	
	cameraHasMouseFocus = true;		
	GetScreen()->GetWindow()->SetWindowLockMouseFlag(true);	
	Input::SetCursorType(Input::CursorType::Crosshair);
}		

void CameraMouseFocusNode::OnEvent(UI::UISelectEventHandler::DeselectedEvent event)
{	
	cameraHasMouseFocus = false;	
	GetScreen()->GetWindow()->SetWindowLockMouseFlag(false);	
	Input::SetCursorType(Input::CursorType::Arrow);
}	

void CameraMouseFocusNode::OnEvent(UI::UIKeyboardEventHandler::KeyPressedEvent event)
{
	if (event.key == Key::Escape)	
		event.inputManager->SelectNode(nullptr);	
}

void CameraMouseFocusNode::OnEvent(UI::UIMouseEventHandler::MousePressedEvent event)
{		
	event.inputManager->SelectNode(this);
}
void CameraMouseFocusNode::OnEvent(UI::UIMouseEventHandler::MouseEnterEvent event)
{
	if (event.inputManager->GetSelectedNode() == this)
		Input::SetCursorType(Input::CursorType::Crosshair);
}
bool CameraMouseFocusNode::HitTest(Vec2f)
{
	return true;
}

EditableText::EditableText() :
	panelRenderUnit(this)
{	
	transformChangedEventHandler.SetFunction([&](auto event) {
		Vec2f size = this->GetTransform().size;		
		editableText.SetTransform({
			.parentPivot = Vec2f(0.5f),
			.pivot = Vec2f(0.5f),
			.size = size
			});
		});
	transformUpdatedEventDispatcher.AddHandler(transformChangedEventHandler);
	
	panelRenderUnit.SetFillColor({ 0.2, 0.2, 0.2, 0.9f });
	panelRenderUnit.SetBorderWidth(0);
	panelRenderUnit.SetCornerRadius(5);
	
	transformChangedEventHandler.SetFunction([&](auto event) {
		auto layoutOptions = editableText.textRenderUnit.GetLayoutOptions();
		layoutOptions.horizontallyUnderfittedOption = UI::TextHorizontallyUnderfittedOptions::Nothing;
		layoutOptions.verticallyUnderfittedOption = UI::TextVerticallyUnderfittedOptions::ResizeToFit;
		editableText.textRenderUnit.SetLayoutOptions(layoutOptions);		
		});

	editableText.SetParent(this);	
}

Graphics::RenderUnit* EditableText::GetRenderUnit(uint index)
{
	switch (index)
	{
	case 0:
		return &panelRenderUnit;
	default:
		return nullptr;
	}	
}
