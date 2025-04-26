#include "pch.h"
#include "SimVisUI.h"
#include "SimulationVisualisationScene.h"

SimVisUI::SimVisUI(SimulationVisualisationScene& scene)
	: Screen(&scene.window), scene(scene)
{	
	Graphics::FontAtlasesData::AddToFont(font, ArrayView<uint>{ 12, 20, 28 }, CharacterSet::ASCIICharacterSet());

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
	infoText.SetTextColor(0xffffffff);
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

	hashCounterText.SetParent(this);
	hashCounterText.SetFont(font);
	hashCounterText.SetFontHeight(12);
	hashCounterText.SetText("0");
	hashCounterText.SetTextColor(0x00ff00ff);

	cameraMouseFocusNode.SetParent(this);

	scene.cameraControls.SetAsTargetNode(&cameraMouseFocusNode);

	Input::GetInputPostUpdateEventDispatcher().AddHandler<&SimVisUI::Event_PostInputUpdate>(*this);
}

SimVisUI::~SimVisUI()
{
	Input::GetInputPostUpdateEventDispatcher().RemoveHandler<&SimVisUI::Event_PostInputUpdate>(*this);
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

void SimVisUI::Event_PostInputUpdate(const Input::InputPostUpdateEvent&)
{
	auto shiftState = Keyboard::GetFrameKeyState(Keyboard::Key::LSHIFT);
	auto upState = Keyboard::GetFrameKeyState(Keyboard::Key::UP);
	auto downState = Keyboard::GetFrameKeyState(Keyboard::Key::DOWN);
	if (upState.down && !shiftState.down || upState.pressed)
		++hashCounter;
	if (downState.down && !shiftState.down || downState.pressed)
		--hashCounter;
	hashCounterText.SetText(StringParsing::Convert(hashCounter));
	//scene.SPHSystemRenderer.SetUniform<uint>(4, hashCounter);
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
