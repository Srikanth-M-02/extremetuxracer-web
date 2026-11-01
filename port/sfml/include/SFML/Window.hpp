/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Minimal re-implementation of the SFML 2 Window module on top of the
Emscripten HTML5 API (canvas, keyboard, mouse, touch, gamepad).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_SFML_WINDOW_HPP
#define ETR_SFML_WINDOW_HPP

#include <SFML/System.hpp>
#include <deque>

namespace sf {

class VideoMode {
public:
	VideoMode() : width(0), height(0), bitsPerPixel(32) {}
	VideoMode(unsigned int w, unsigned int h, unsigned int bpp = 32) : width(w), height(h), bitsPerPixel(bpp) {}
	static VideoMode getDesktopMode();
	bool isValid() const { return width > 0 && height > 0; }
	unsigned int width, height, bitsPerPixel;
};

struct ContextSettings {
	enum Attribute { Default = 0, Core = 1, Debug = 4 };
	explicit ContextSettings(unsigned int depth = 0, unsigned int stencil = 0, unsigned int antialiasing = 0,
	                         unsigned int major = 1, unsigned int minor = 1, unsigned int attributes = Default, bool sRgb = false)
		: depthBits(depth), stencilBits(stencil), antialiasingLevel(antialiasing),
		  majorVersion(major), minorVersion(minor), attributeFlags(attributes), sRgbCapable(sRgb) {}
	unsigned int depthBits, stencilBits, antialiasingLevel, majorVersion, minorVersion;
	Uint32 attributeFlags;
	bool sRgbCapable;
};

namespace Style {
enum {
	None = 0,
	Titlebar = 1 << 0,
	Resize = 1 << 1,
	Close = 1 << 2,
	Fullscreen = 1 << 3,
	Default = Titlebar | Resize | Close
};
}

class Keyboard {
public:
	enum Key {
		Unknown = -1,
		A = 0, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
		Escape, LControl, LShift, LAlt, LSystem, RControl, RShift, RAlt, RSystem, Menu,
		LBracket, RBracket, Semicolon, Comma, Period, Quote, Slash, Backslash, Tilde, Equal, Hyphen,
		Space, Enter, Backspace, Tab, PageUp, PageDown, End, Home, Insert, Delete,
		Add, Subtract, Multiply, Divide, Left, Right, Up, Down,
		Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, Pause,
		KeyCount,
		// deprecated aliases kept by SFML 2.x
		Dash = Hyphen, BackSpace = Backspace, BackSlash = Backslash, SemiColon = Semicolon, Return = Enter
	};
	static bool isKeyPressed(Key key);
	static void setVirtualKeyboardVisible(bool) {}
};

class Window;

class Mouse {
public:
	enum Button { Left, Right, Middle, XButton1, XButton2, ButtonCount };
	enum Wheel { VerticalWheel, HorizontalWheel };
	static bool isButtonPressed(Button button);
	static Vector2i getPosition();
	static Vector2i getPosition(const Window& relativeTo);
	static void setPosition(const Vector2i&) {}
};

class Joystick {
public:
	enum { Count = 8, ButtonCount = 32, AxisCount = 8 };
	enum Axis { X, Y, Z, R, U, V, PovX, PovY };
	static bool isConnected(unsigned int joystick);
	static unsigned int getButtonCount(unsigned int joystick);
	static bool hasAxis(unsigned int joystick, Axis axis);
	static bool isButtonPressed(unsigned int joystick, unsigned int button);
	static float getAxisPosition(unsigned int joystick, Axis axis);
	static void update();
};

class Event {
public:
	struct SizeEvent { unsigned int width, height; };
	struct KeyEvent { Keyboard::Key code; bool alt, control, shift, system; };
	struct TextEvent { Uint32 unicode; };
	struct MouseMoveEvent { int x, y; };
	struct MouseButtonEvent { Mouse::Button button; int x, y; };
	struct MouseWheelEvent { int delta; int x, y; };
	struct MouseWheelScrollEvent { Mouse::Wheel wheel; float delta; int x, y; };
	struct JoystickConnectEvent { unsigned int joystickId; };
	struct JoystickMoveEvent { unsigned int joystickId; Joystick::Axis axis; float position; };
	struct JoystickButtonEvent { unsigned int joystickId; unsigned int button; };

	enum EventType {
		Closed, Resized, LostFocus, GainedFocus, TextEntered, KeyPressed, KeyReleased,
		MouseWheelMoved, MouseWheelScrolled, MouseButtonPressed, MouseButtonReleased, MouseMoved,
		MouseEntered, MouseLeft, JoystickButtonPressed, JoystickButtonReleased, JoystickMoved,
		JoystickConnected, JoystickDisconnected, TouchBegan, TouchMoved, TouchEnded, SensorChanged,
		Count
	};

	EventType type;
	union {
		SizeEvent size;
		KeyEvent key;
		TextEvent text;
		MouseMoveEvent mouseMove;
		MouseButtonEvent mouseButton;
		MouseWheelEvent mouseWheel;
		MouseWheelScrollEvent mouseWheelScroll;
		JoystickMoveEvent joystickMove;
		JoystickButtonEvent joystickButton;
		JoystickConnectEvent joystickConnect;
	};
};

typedef void (*GlFunctionPointer)();

class Context {
public:
	// No GL extensions are exposed beyond what the compat layer emulates.
	static GlFunctionPointer getFunction(const char*) { return nullptr; }
};

// The browser canvas. Only one window can exist.
class Window {
public:
	Window();
	virtual ~Window();
	void create(VideoMode mode, const String& title, Uint32 style = Style::Default,
	            const ContextSettings& settings = ContextSettings());
	void close();
	bool isOpen() const { return open; }
	bool pollEvent(Event& event);
	void display();
	void setFramerateLimit(unsigned int) {}
	void setVerticalSyncEnabled(bool) {}
	void setKeyRepeatEnabled(bool enabled);
	void setMouseCursorVisible(bool visible);
	void setTitle(const String& title);
	void setActive(bool = true) const {}
	Vector2u getSize() const;
	const ContextSettings& getSettings() const { return settings; }

	// Port extension: size requested by the browser (window resized).
	static Vector2u getAvailableSize();

protected:
	virtual void onCreate() {}

private:
	bool open;
	ContextSettings settings;
};

} // namespace sf

#endif
