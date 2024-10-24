#include "pch.h"
#include "SimVisUI.h"

SimVisUI::SimVisUI()
{
	Graphics::FontAtlasesData::AddToFont(font, { 12, 20, 28}, CharacterSet::ASCIICharacterSet());

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

	titleText.SetParent(this);
	titleText.SetTransform({
		.pos = Vec2f(0, -24),
		.parentPivot = Vec2f(0.5f, 1.0f),
		.pivot = Vec2f(0.5f, 1.0f),
		});
	titleText.SetFont(font);
	titleText.SetFontHeight(28);
	titleText.SetText("Simulation visualisation");

	cameraMouseFocusNode.SetParent(this);
}

void SimVisUI::SetFPS(uintMem FPS)
{
	this->FPS = FPS;
	ReconstructInfoText();
}

void SimVisUI::SetParticleCount(uintMem particleCount)
{
	this->particleCount = particleCount;
}

void SimVisUI::SetInfo(StringUTF8 text)
{
	this->info = text;
	ReconstructInfoText();
}

void SimVisUI::ReconstructInfoText()
{
	StringUTF8 text;
	if (!info.Empty())
		text += info + "\n\n";

	text += "Number of particles: " + StringParsing::Convert(particleCount) + "\n\n";

	if (implementationName.Empty())
		text += "T to rotate implementations (Don't do this it doesn't work currently). No implementation is selected currently.\n\n";
	else
		text += "T to rotate implementations (Don't do this it doesn't work currently). Current implementation is \"" + implementationName + "\"\n\n";

	text +=		
		"Press TAB to switch scenes\n"
		"Press I to switch to imaging mode. And I again to change back.\n"
		"\n"
		"Camera controls:\n"
		"\tX to start the simulation\n"
		"\tShift+X to stop the simulation\n"
		"\tRightArrow to step the simulation\n"
		"\tR to reset the simulation\n"
		"\tClick the screen to enter camera move mode\n"
		"\tESC to exit camera move mode\n"
		"\tIn camera move mode use W, A, S, D and the mouse to move the camera. Use the scroll-wheel to increase/decrease the speed\n"		
		"FPS: " + StringParsing::Convert(FPS);

	infoText.SetText(text);
}

CameraMouseFocusNode::~CameraMouseFocusNode()
{
	if (((SimVisUI*)GetScreen())->cameraHasMouseFocus)
	{
		GetScreen()->GetWindow()->SetWindowLockMouseFlag(false);
		Input::SetCursorType(Input::CursorType::Arrow);
	}
}

void CameraMouseFocusNode::OnEvent(UI::UISelectEventHandler::SelectedEvent event)
{	
	((SimVisUI*)GetScreen())->cameraHasMouseFocus = true;	
	GetScreen()->GetWindow()->SetWindowLockMouseFlag(true);	
	Input::SetCursorType(Input::CursorType::Crosshair);
}		

void CameraMouseFocusNode::OnEvent(UI::UISelectEventHandler::DeselectedEvent event)
{	
	((SimVisUI*)GetScreen())->cameraHasMouseFocus = false;
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
