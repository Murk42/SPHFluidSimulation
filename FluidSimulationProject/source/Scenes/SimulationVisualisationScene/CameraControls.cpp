#include "pch.h"
#include "CameraControls.h"

CameraMouseFocusNode::~CameraMouseFocusNode()
{	
	GetScreen()->GetWindow()->SetMouseGrabbedFlag(false);
	Input::SetCursorType(Input::CursorType::Arrow);	
}
void CameraMouseFocusNode::OnEvent(const UI::UISelectEventHandler::SelectedEvent& event)
{	
	GetScreen()->GetWindow()->SetMouseGrabbedFlag(true);
	Input::SetCursorType(Input::CursorType::Crosshair);
}
void CameraMouseFocusNode::OnEvent(const UI::UISelectEventHandler::DeselectedEvent& event)
{
	GetScreen()->GetWindow()->SetMouseGrabbedFlag(false);
	Input::SetCursorType(Input::CursorType::Arrow);
}
void CameraMouseFocusNode::OnEvent(const UI::UIKeyboardEventHandler::KeyDownEvent& event)
{
	if (event.key == Keyboard::Key::ESCAPE)
		event.inputManager.SelectNode(nullptr);
	else
		keyDownEventDispatcher.Call(event);
}
void CameraMouseFocusNode::OnEvent(const UI::UIKeyboardEventHandler::KeyUpEvent& event)
{
	if (event.key != Keyboard::Key::ESCAPE)
		keyUpEventDispatcher.Call(event);
}
void CameraMouseFocusNode::OnEvent(const UI::UIMouseEventHandler::MouseMotionEvent& event)
{
	if (event.inputManager.GetSelectedNode() == this)
		mouseMotionEventDispatcher.Call(event);
}
void CameraMouseFocusNode::OnEvent(const UI::UIMouseEventHandler::MouseScrollEvent& event)
{
	if (event.inputManager.GetSelectedNode() == this)
		mouseScrollEventDispatcher.Call(event);
}
void CameraMouseFocusNode::OnEvent(const UI::UIMouseEventHandler::MouseButtonDownEvent& event)
{
	event.inputManager.SelectNode(this);
}
void CameraMouseFocusNode::OnEvent(const UI::UIMouseEventHandler::MouseEnterEvent& event)
{
	if (event.inputManager.GetSelectedNode() == this)
		Input::SetCursorType(Input::CursorType::Crosshair);
}
int CameraMouseFocusNode::HitTest(Vec2f)
{
	return 1;
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
		this->node->mouseMotionEventDispatcher.AddHandler(*this);
		this->node->mouseScrollEventDispatcher.AddHandler(*this);
		this->node->keyDownEventDispatcher.AddHandler(*this);
		this->node->keyUpEventDispatcher.AddHandler(*this);
		Input::GetInputPostUpdateEventDispatcher().RemoveHandler(*this);
	}
}
void CameraControls::SetAsTargetNode(CameraMouseFocusNode* node)
{		
    if (this->node != nullptr)
    {
		this->node->mouseMotionEventDispatcher.RemoveHandler(*this);
		this->node->mouseScrollEventDispatcher.RemoveHandler(*this);
		this->node->keyDownEventDispatcher.RemoveHandler(*this);
		this->node->keyUpEventDispatcher.RemoveHandler(*this);

		if (node == nullptr)
			Input::GetInputPostUpdateEventDispatcher().RemoveHandler(*this);
    }
	else if (node != nullptr)
		Input::GetInputPostUpdateEventDispatcher().AddHandler(*this);

    this->node = node;


	if (this->node != nullptr)
	{
		this->node->keyUpEventDispatcher.AddHandler(*this);
		this->node->keyDownEventDispatcher.AddHandler(*this);
		this->node->mouseScrollEventDispatcher.AddHandler(*this);
		this->node->mouseMotionEventDispatcher.AddHandler(*this);
		firstFrame = true;
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
void CameraControls::OnEvent(const UI::UIMouseEventHandler::MouseMotionEvent& event)
{
	cameraAngles.x += -(float)event.delta.y / 500;
	cameraAngles.y += (float)event.delta.x / 500;
	cameraRot = Quatf(Vec3f(0, 1, 0), cameraAngles.y) * Quatf(Vec3f(1, 0, 0), cameraAngles.x);
}
void CameraControls::OnEvent(const UI::UIMouseEventHandler::MouseScrollEvent& event)
{
	cameraSpeed *= pow(0.9f, -event.value.y);
	cameraSpeed = std::clamp(cameraSpeed, 0.01f, 20.0f);
}
void CameraControls::OnEvent(const UI::UIKeyboardEventHandler::KeyDownEvent& event)
{
	using namespace Keyboard;
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
void CameraControls::OnEvent(const UI::UIKeyboardEventHandler::KeyUpEvent& event)
{
	using namespace Keyboard;
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
void CameraControls::OnEvent(const Input::InputPostUpdateEvent& event)
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
