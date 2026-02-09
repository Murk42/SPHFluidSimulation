#include "pch.h"
#include "CameraControls.h"
#include "BlazeEngine/UI/Core/Screen.h"

CameraMouseFocusNode::CameraMouseFocusNode()
{
	InputNode::selectedStateChangedEventDispatcher.AddHandler<&CameraMouseFocusNode::SelectedStateChanged>(*this);
	InputNode::keyDownEventDispatcher.AddHandler<&CameraMouseFocusNode::KeyDown>(*this);
	InputNode::keyUpEventDispatcher.AddHandler<&CameraMouseFocusNode::KeyUp>(*this);
	InputNode::mouseMotionEventDispatcher.AddHandler<&CameraMouseFocusNode::MouseMotion>(*this);
	InputNode::mouseScrollEventDispatcher.AddHandler<&CameraMouseFocusNode::MouseScroll>(*this);
	InputNode::mouseButtonDownEventDispatcher.AddHandler<&CameraMouseFocusNode::MouseButtonDown>(*this);
}

CameraMouseFocusNode::~CameraMouseFocusNode()
{
	InputNode::selectedStateChangedEventDispatcher.RemoveHandler<&CameraMouseFocusNode::SelectedStateChanged>(*this);
	InputNode::keyDownEventDispatcher.RemoveHandler<&CameraMouseFocusNode::KeyDown>(*this);
	InputNode::keyUpEventDispatcher.RemoveHandler<&CameraMouseFocusNode::KeyUp>(*this);
	InputNode::mouseMotionEventDispatcher.RemoveHandler<&CameraMouseFocusNode::MouseMotion>(*this);
	InputNode::mouseScrollEventDispatcher.RemoveHandler<&CameraMouseFocusNode::MouseScroll>(*this);
	InputNode::mouseButtonDownEventDispatcher.RemoveHandler<&CameraMouseFocusNode::MouseButtonDown>(*this);

}
void CameraMouseFocusNode::SelectedStateChanged(const SelectedStateChangedEvent& event)
{
	if (IsSelected())
	{
		CaptureMouse(caputredMouseID);

		if (auto inputSubSystem = GetInputSubSystem())
			if (auto window = inputSubSystem->GetWindow())
			{
				Input::ShowCursor(false);
				window->SetRelativeMouseModeFlag(true);
				Input::ShowCursor(false);
			}
	}
	else
	{
		ReleaseMouse(caputredMouseID);

		if (auto inputSubSystem = GetInputSubSystem())
			if (auto window = inputSubSystem->GetWindow())
			{
				Input::ShowCursor(true);
				window->SetRelativeMouseModeFlag(false);
				Input::ShowCursor(true);
			}
	}
}
void CameraMouseFocusNode::KeyDown(const UI::UIKeyDownEvent& event)
{
	if (event.key == Input::Key::ESCAPE)
		Unselect();
	else
		keyDownEventDispatcher.Call(event);
}
void CameraMouseFocusNode::KeyUp(const UI::UIKeyUpEvent& event)
{
	if (event.key != Input::Key::ESCAPE)
		keyUpEventDispatcher.Call(event);
}
void CameraMouseFocusNode::MouseMotion(const UI::UIMouseMotionEvent& event)
{
	if (IsSelected())
		mouseMotionEventDispatcher.Call(event);
}
void CameraMouseFocusNode::MouseScroll(const UI::UIMouseScrollEvent& event)
{
	if (IsSelected())
		mouseScrollEventDispatcher.Call(event);
}
void CameraMouseFocusNode::MouseButtonDown(const UI::UIMouseButtonDownEvent& event)
{
	caputredMouseID = event.mouseID;
	Select();
}
UI::Node::HitStatus CameraMouseFocusNode::HitTest(Vec2f)
{
	return HitStatus::HitBlocking;
}

CameraControls::CameraControls() :
	cameraSpeed(4.0f), node(nullptr), firstFrame(false),
	FORWARD(false), LEFT(false), BACKWARD(false), RIGHT(false), UP(false), DOWN(false)
{
}
CameraControls::~CameraControls()
{
	if (this->node != nullptr)
	{
		this->node->keyDownEventDispatcher.AddHandler<&CameraControls::KeyDown>(*this);
		this->node->keyUpEventDispatcher.AddHandler<&CameraControls::KeyUp>(*this);
		this->node->mouseMotionEventDispatcher.AddHandler<&CameraControls::MouseMotion>(*this);
		this->node->mouseScrollEventDispatcher.AddHandler < &CameraControls::MouseScroll>(*this);
	}
}
void CameraControls::SetAsTargetNode(CameraMouseFocusNode* node)
{
	if (this->node != nullptr)
	{
		this->node->keyDownEventDispatcher.RemoveHandler<&CameraControls::KeyDown>(*this);
		this->node->keyUpEventDispatcher.RemoveHandler<&CameraControls::KeyUp>(*this);
		this->node->mouseMotionEventDispatcher.RemoveHandler<&CameraControls::MouseMotion>(*this);
		this->node->mouseScrollEventDispatcher.RemoveHandler < &CameraControls::MouseScroll>(*this);
	}

	this->node = node;

	if (this->node != nullptr)
	{
		this->node->keyDownEventDispatcher.AddHandler<&CameraControls::KeyDown>(*this);
		this->node->keyUpEventDispatcher.AddHandler<&CameraControls::KeyUp>(*this);
		this->node->mouseMotionEventDispatcher.AddHandler<&CameraControls::MouseMotion>(*this);
		this->node->mouseScrollEventDispatcher.AddHandler < &CameraControls::MouseScroll>(*this);
		firstFrame = true;
	}
}
void CameraControls::Update()
{
	float dt = inputUpdateStopwatch.Reset();

	if (firstFrame)
	{
		dt = 0;
		firstFrame = false;
	}
	else
	{
		if (FORWARD)
			cameraPos += cameraRot * Vec3f(0, 0, dt) * cameraSpeed;
		if (BACKWARD)
			cameraPos += cameraRot * Vec3f(0, 0, -dt) * cameraSpeed;
		if (RIGHT)
			cameraPos += cameraRot * Vec3f(dt, 0, 0) * cameraSpeed;
		if (LEFT)
			cameraPos += cameraRot * Vec3f(-dt, 0, 0) * cameraSpeed;
		if (UP)
			cameraPos += cameraRot * Vec3f(0, dt, 0) * cameraSpeed;
		if (DOWN)
			cameraPos += cameraRot * Vec3f(0, -dt, 0) * cameraSpeed;
	}
}
Mat4f CameraControls::GetViewMatrix() const
{
    return Mat4f::RotationMatrix(cameraRot.Conjugated()) * Mat4f::TranslationMatrix(-cameraPos);
}
Vec2f CameraControls::GetCameraAngles() const
{
	return cameraAngles;
}
Vec3f CameraControls::GetCameraPos() const
{
	return cameraPos;
}
void CameraControls::SetCameraAngles(Vec2f cameraAngles)
{
	this->cameraAngles = cameraAngles;
}
void CameraControls::SetCameraPos(Vec3f pos)
{
	this->cameraPos = pos;
}
void CameraControls::MouseMotion(const UI::UIMouseMotionEvent& event)
{
	cameraAngles.x += -(float)event.delta.y / 500;
	cameraAngles.y += (float)event.delta.x / 500;
	cameraRot = Quatf(Vec3f(0, 1, 0), cameraAngles.y) * Quatf(Vec3f(1, 0, 0), cameraAngles.x);
}
void CameraControls::MouseScroll(const UI::UIMouseScrollEvent& event)
{
	cameraSpeed *= std::pow(0.9f, -event.amount.y);
	cameraSpeed = std::clamp(cameraSpeed, 0.01f, 20.0f);
}
void CameraControls::KeyDown(const UI::UIKeyDownEvent& event)
{
	using namespace Input;
	using enum Key;

	if (event.repeat)
		return;

	switch (event.key)
	{
	case W: this->FORWARD = true; break;
	case A: this->LEFT = true; break;
	case S: this->BACKWARD = true; break;
	case D: this->RIGHT = true; break;
	case SPACE:
		if (bool(event.modifier & KeyModifier::SHIFT))
			this->DOWN = true;
		else
			this->UP = true;
		break;
	default:
		break;
	}
}
void CameraControls::KeyUp(const UI::UIKeyUpEvent& event)
{
	using namespace Input;
	using enum Key;

	switch (event.key)
	{
	case W: this->FORWARD = false; break;
	case A: this->LEFT = false; break;
	case S: this->BACKWARD = false; break;
	case D: this->RIGHT = false; break;
	case SPACE:
			this->DOWN = false;
			this->UP = false;
		break;
	default:
		break;
	}
}
