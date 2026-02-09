#include "pch.h"
#include "SimVisUI.h"
#include "SimulationVisualisationScene.h"

SimVisUI::SimVisUI(ResourceManager& resourceManager, SimulationVisualisationScene& scene)
	: Screen(resourceManager), scene(scene)
{
	infoText.SetParent(this);
	infoText.SetTransform({ .pos = Vec2f(2, 2), .parentPivot = Vec2f(0, 0), .pivot = Vec2f(0, 0), });
	ReconstructInfoText();

	titleText.SetParent(this);
	titleText.SetTransform({ .pos = Vec2f(0, -5), .parentPivot = Vec2f(0.5f, 1.0f), .pivot = Vec2f(0.5f, 1.0f), });
	titleText.SetText("Simulation vizualisation");
	titleText.SetTextStyle({ .fontName = "default", .fontHeight = 28 });

	hashCounterText.SetParent(this);
	hashCounterText.SetText("0");
	hashCounterText.SetTextStyle({ .fontName = "default", .fontHeight = 12 });

	cameraMouseFocusNode.SetParent(this);

	scene.cameraControls.SetAsTargetNode(&cameraMouseFocusNode);
}

SimVisUI::~SimVisUI()
{
}

void SimVisUI::Update()
{
	//auto shiftState = Input::GetKeyFrameState(Input::Key::LSHIFT);
	//auto upState = Input::GetKeyFrameState(Input::Key::UP);
	//auto downState = Input::GetKeyFrameState(Input::Key::DOWN);
	//if (upState.down && !shiftState.down || upState.pressed)
	//	++hashCounter;
	//if (downState.down && !shiftState.down || downState.pressed)
	//	--hashCounter;
	//hashCounterText.BuildText(String::Parse(hashCounter), fontFace, 12, fontAtlas1);
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

void SimVisUI::SetInfo(u8String text)
{
	this->info = text;
	ReconstructInfoText();
}

void SimVisUI::ReconstructInfoText()
{
	u8String text;
	if (!info.Empty())
		text += info + "\n\n";

	text += "Number of particles: " + String::Parse(particleCount) + "\n\n";

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
		"FPS: " + String::Parse(FPS);

	infoText.SetText(text);
}