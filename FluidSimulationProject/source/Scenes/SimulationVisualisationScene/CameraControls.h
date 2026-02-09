#pragma once
#include "BlazeEngine/UI/Input/InputNode.h"

class CameraMouseFocusNode : public UI::InputNode
{
public:
	EventDispatcher<UI::UIMouseMotionEvent> mouseMotionEventDispatcher;
	EventDispatcher<UI::UIMouseScrollEvent> mouseScrollEventDispatcher;
	EventDispatcher<UI::UIKeyDownEvent> keyDownEventDispatcher;
	EventDispatcher<UI::UIKeyUpEvent> keyUpEventDispatcher;

	CameraMouseFocusNode();
	~CameraMouseFocusNode();
private:
	Input::MouseID caputredMouseID;

	void SelectedStateChanged(const SelectedStateChangedEvent& event);

	void KeyDown(const UI::UIKeyDownEvent& event);
	void KeyUp(const UI::UIKeyUpEvent& event);
	void MouseMotion(const UI::UIMouseMotionEvent& event);
	void MouseScroll(const UI::UIMouseScrollEvent& event);
	void MouseButtonDown(const UI::UIMouseButtonDownEvent& event);

	HitStatus HitTest(Vec2f) override;
};

class CameraControls
{
public:
	CameraControls();
	~CameraControls();

	void SetAsTargetNode(CameraMouseFocusNode* node);

	void Update();

	Mat4f GetViewMatrix() const;
	Vec2f GetCameraAngles() const;
	Vec3f GetCameraPos() const;

	void SetCameraAngles(Vec2f cameraAngles);
	void SetCameraPos(Vec3f pos);
private:
	float cameraSpeed;
	Vec2f cameraAngles;
	Quatf cameraRot;
	Vec3f cameraPos;
	CameraMouseFocusNode* node;

	bool firstFrame : 1;
	bool FORWARD : 1;
	bool LEFT : 1;
	bool BACKWARD : 1;
	bool RIGHT : 1;
	bool UP : 1;
	bool DOWN : 1;

	Stopwatch inputUpdateStopwatch;

	void KeyDown(const UI::UIKeyDownEvent& event);
	void KeyUp(const UI::UIKeyUpEvent& event);
	void MouseMotion(const UI::UIMouseMotionEvent& event);
	void MouseScroll(const UI::UIMouseScrollEvent& event);
};