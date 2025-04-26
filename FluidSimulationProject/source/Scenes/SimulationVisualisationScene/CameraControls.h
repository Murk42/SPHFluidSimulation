#pragma once

class CameraMouseFocusNode :
	public UI::InputNode,
	public UI::UISelectEventHandler,
	public UI::UIMouseEventHandler,
	public UI::UIKeyboardEventHandler
{
public:
	EventDispatcher<UI::UIMouseEventHandler::MouseMotionEvent> mouseMotionEventDispatcher;
	EventDispatcher<UI::UIMouseEventHandler::MouseScrollEvent> mouseScrollEventDispatcher;
	EventDispatcher<UI::UIKeyboardEventHandler::KeyDownEvent> keyDownEventDispatcher;
	EventDispatcher<UI::UIKeyboardEventHandler::KeyUpEvent> keyUpEventDispatcher;

	~CameraMouseFocusNode();
private:
	void OnEvent(const UI::UISelectEventHandler::SelectedEvent& event) override;
	void OnEvent(const UI::UISelectEventHandler::DeselectedEvent& event) override;
	void OnEvent(const UI::UIKeyboardEventHandler::KeyDownEvent& event) override;
	void OnEvent(const UI::UIKeyboardEventHandler::KeyUpEvent& event) override;
	void OnEvent(const UI::UIMouseEventHandler::MouseMotionEvent& event) override;
	void OnEvent(const UI::UIMouseEventHandler::MouseScrollEvent& event) override;
	void OnEvent(const UI::UIMouseEventHandler::MouseButtonDownEvent& event) override;
	void OnEvent(const UI::UIMouseEventHandler::MouseEnterEvent& event) override;

	int HitTest(Vec2f) override;
};

class CameraControls :
	private EventHandler<UI::UIMouseEventHandler::MouseMotionEvent>,
	private EventHandler<UI::UIMouseEventHandler::MouseScrollEvent>,
	private EventHandler<UI::UIKeyboardEventHandler::KeyDownEvent>,
	private EventHandler<UI::UIKeyboardEventHandler::KeyUpEvent>,
	private EventHandler<Input::InputPostUpdateEvent>
{
public:
	CameraControls();
	~CameraControls();

	void SetAsTargetNode(CameraMouseFocusNode* node);

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

	void OnEvent(const UI::UIMouseEventHandler::MouseMotionEvent& event) override;
	void OnEvent(const UI::UIMouseEventHandler::MouseScrollEvent& event) override;
	void OnEvent(const UI::UIKeyboardEventHandler::KeyDownEvent& event) override;
	void OnEvent(const UI::UIKeyboardEventHandler::KeyUpEvent& event) override;
	void OnEvent(const Input::InputPostUpdateEvent& event);
};