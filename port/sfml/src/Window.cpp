/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

sf::Window and input devices implemented with the Emscripten HTML5 API.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include "../../glcompat/gl_compat.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {

std::deque<sf::Event> eventQueue;
bool keyState[sf::Keyboard::KeyCount] = {};
bool mouseState[sf::Mouse::ButtonCount] = {};
sf::Vector2i mousePos;
bool keyRepeat = true;
bool callbacksInstalled = false;
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE glContext = 0;
unsigned int canvasW = 0, canvasH = 0;
std::string pendingScreenshot;

const char* kCanvas = "#canvas";

// ------------------------------------------------------------------
// Keyboard mapping (KeyboardEvent.code / KeyboardEvent.key -> sf::Key)
// ------------------------------------------------------------------
struct CodeMap { const char* code; sf::Keyboard::Key key; };
const CodeMap kCodeMap[] = {
	{"Escape", sf::Keyboard::Escape},
	{"ControlLeft", sf::Keyboard::LControl}, {"ControlRight", sf::Keyboard::RControl},
	{"ShiftLeft", sf::Keyboard::LShift}, {"ShiftRight", sf::Keyboard::RShift},
	{"AltLeft", sf::Keyboard::LAlt}, {"AltRight", sf::Keyboard::RAlt},
	{"MetaLeft", sf::Keyboard::LSystem}, {"MetaRight", sf::Keyboard::RSystem},
	{"OSLeft", sf::Keyboard::LSystem}, {"OSRight", sf::Keyboard::RSystem},
	{"ContextMenu", sf::Keyboard::Menu},
	{"BracketLeft", sf::Keyboard::LBracket}, {"BracketRight", sf::Keyboard::RBracket},
	{"Semicolon", sf::Keyboard::Semicolon}, {"Comma", sf::Keyboard::Comma},
	{"Period", sf::Keyboard::Period}, {"Quote", sf::Keyboard::Quote},
	{"Slash", sf::Keyboard::Slash}, {"Backslash", sf::Keyboard::Backslash},
	{"Backquote", sf::Keyboard::Tilde}, {"Equal", sf::Keyboard::Equal},
	{"Minus", sf::Keyboard::Hyphen}, {"Space", sf::Keyboard::Space},
	{"Enter", sf::Keyboard::Enter}, {"NumpadEnter", sf::Keyboard::Enter},
	{"Backspace", sf::Keyboard::Backspace}, {"Tab", sf::Keyboard::Tab},
	{"PageUp", sf::Keyboard::PageUp}, {"PageDown", sf::Keyboard::PageDown},
	{"End", sf::Keyboard::End}, {"Home", sf::Keyboard::Home},
	{"Insert", sf::Keyboard::Insert}, {"Delete", sf::Keyboard::Delete},
	{"NumpadAdd", sf::Keyboard::Add}, {"NumpadSubtract", sf::Keyboard::Subtract},
	{"NumpadMultiply", sf::Keyboard::Multiply}, {"NumpadDivide", sf::Keyboard::Divide},
	{"ArrowLeft", sf::Keyboard::Left}, {"ArrowRight", sf::Keyboard::Right},
	{"ArrowUp", sf::Keyboard::Up}, {"ArrowDown", sf::Keyboard::Down},
	{"Pause", sf::Keyboard::Pause},
};

sf::Keyboard::Key MapKey(const char* code, const char* key) {
	// Letters follow the keyboard layout (like SFML), everything else
	// uses the physical key position.
	if (key[0] != 0 && key[1] == 0) {
		char c = key[0];
		if (c >= 'a' && c <= 'z') return static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (c - 'a'));
		if (c >= 'A' && c <= 'Z') return static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (c - 'A'));
	}
	if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' && code[4] == 0)
		return static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (code[3] - 'A'));
	if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' && code[6] == 0)
		return static_cast<sf::Keyboard::Key>(sf::Keyboard::Num0 + (code[5] - '0'));
	if (std::strncmp(code, "Numpad", 6) == 0 && code[6] >= '0' && code[6] <= '9' && code[7] == 0)
		return static_cast<sf::Keyboard::Key>(sf::Keyboard::Numpad0 + (code[6] - '0'));
	if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9') {
		int n = std::atoi(code + 1);
		if (n >= 1 && n <= 15) return static_cast<sf::Keyboard::Key>(sf::Keyboard::F1 + (n - 1));
	}
	for (const CodeMap& m : kCodeMap)
		if (std::strcmp(m.code, code) == 0) return m.key;
	return sf::Keyboard::Unknown;
}

// Decodes a KeyboardEvent.key value into a single printable code point.
sf::Uint32 PrintableCodepoint(const char* key) {
	const unsigned char* s = reinterpret_cast<const unsigned char*>(key);
	if (s[0] == 0) return 0;
	sf::Uint32 c;
	int len;
	if (s[0] < 0x80) { c = s[0]; len = 1; }
	else if ((s[0] & 0xE0) == 0xC0) { c = s[0] & 0x1F; len = 2; }
	else if ((s[0] & 0xF0) == 0xE0) { c = s[0] & 0x0F; len = 3; }
	else if ((s[0] & 0xF8) == 0xF0) { c = s[0] & 0x07; len = 4; }
	else return 0;
	for (int i = 1; i < len; i++) {
		if ((s[i] & 0xC0) != 0x80) return 0;
		c = (c << 6) | (s[i] & 0x3F);
	}
	if (s[len] != 0) return 0;  // named key like "Enter"
	if (c < 32 || c == 127) return 0;
	return c;
}

void PushKey(sf::Keyboard::Key k, bool pressed, const EmscriptenKeyboardEvent* e) {
	sf::Event ev;
	ev.type = pressed ? sf::Event::KeyPressed : sf::Event::KeyReleased;
	ev.key.code = k;
	ev.key.alt = e ? e->altKey : false;
	ev.key.control = e ? e->ctrlKey : false;
	ev.key.shift = e ? e->shiftKey : false;
	ev.key.system = e ? e->metaKey : false;
	eventQueue.push_back(ev);
}

EM_BOOL OnKey(int type, const EmscriptenKeyboardEvent* e, void*) {
	const bool down = (type == EMSCRIPTEN_EVENT_KEYDOWN);
	const sf::Keyboard::Key k = MapKey(e->code, e->key);

	// Leave browser shortcuts alone.
	const bool browserKey = e->ctrlKey || e->metaKey ||
	                        std::strcmp(e->code, "F5") == 0 || std::strcmp(e->code, "F11") == 0 ||
	                        std::strcmp(e->code, "F12") == 0;

	if (down) {
		if (!(e->repeat && !keyRepeat)) {
			if (k != sf::Keyboard::Unknown) {
				keyState[k] = true;
				PushKey(k, true, e);
			}
			if (!e->ctrlKey && !e->metaKey) {
				sf::Uint32 cp = PrintableCodepoint(e->key);
				if (cp) {
					sf::Event ev;
					ev.type = sf::Event::TextEntered;
					ev.text.unicode = cp;
					eventQueue.push_back(ev);
				}
			}
		}
	} else if (k != sf::Keyboard::Unknown) {
		keyState[k] = false;
		PushKey(k, false, e);
	}
	return browserKey ? EM_FALSE : EM_TRUE;
}

void ReleaseAllKeys() {
	for (int k = 0; k < sf::Keyboard::KeyCount; k++) {
		if (keyState[k]) {
			keyState[k] = false;
			PushKey(static_cast<sf::Keyboard::Key>(k), false, nullptr);
		}
	}
}

EM_BOOL OnBlur(int, const EmscriptenFocusEvent*, void*) {
	ReleaseAllKeys();
	return EM_FALSE;
}

// ------------------------------------------------------------------
// Mouse / touch
// ------------------------------------------------------------------
sf::Vector2i ToCanvasCoords(double x, double y) {
	double cssW = 0, cssH = 0;
	emscripten_get_element_css_size(kCanvas, &cssW, &cssH);
	if (cssW <= 0 || cssH <= 0) return sf::Vector2i((int)x, (int)y);
	return sf::Vector2i((int)std::floor(x * canvasW / cssW), (int)std::floor(y * canvasH / cssH));
}

void PushMouseMove(sf::Vector2i p) {
	mousePos = p;
	sf::Event ev;
	ev.type = sf::Event::MouseMoved;
	ev.mouseMove.x = p.x;
	ev.mouseMove.y = p.y;
	eventQueue.push_back(ev);
}

void PushMouseButton(sf::Mouse::Button b, bool pressed, sf::Vector2i p) {
	mouseState[b] = pressed;
	sf::Event ev;
	ev.type = pressed ? sf::Event::MouseButtonPressed : sf::Event::MouseButtonReleased;
	ev.mouseButton.button = b;
	ev.mouseButton.x = p.x;
	ev.mouseButton.y = p.y;
	eventQueue.push_back(ev);
}

sf::Mouse::Button MapButton(unsigned short b) {
	switch (b) {
		case 1: return sf::Mouse::Middle;
		case 2: return sf::Mouse::Right;
		case 3: return sf::Mouse::XButton1;
		case 4: return sf::Mouse::XButton2;
		default: return sf::Mouse::Left;
	}
}

EM_BOOL OnMouse(int type, const EmscriptenMouseEvent* e, void*) {
	sf::Vector2i p = ToCanvasCoords(e->targetX, e->targetY);
	switch (type) {
		case EMSCRIPTEN_EVENT_MOUSEMOVE:
			PushMouseMove(p);
			return EM_FALSE;
		case EMSCRIPTEN_EVENT_MOUSEDOWN:
			if (p != mousePos) PushMouseMove(p);
			PushMouseButton(MapButton(e->button), true, p);
			return EM_TRUE;
		case EMSCRIPTEN_EVENT_MOUSEUP:
			PushMouseButton(MapButton(e->button), false, p);
			return EM_TRUE;
		default:
			return EM_FALSE;
	}
}

long activeTouch = -1;

EM_BOOL OnTouch(int type, const EmscriptenTouchEvent* e, void*) {
	for (int i = 0; i < e->numTouches; i++) {
		const EmscriptenTouchPoint& t = e->touches[i];
		if (!t.isChanged) continue;
		sf::Vector2i p = ToCanvasCoords(t.targetX, t.targetY);
		if (type == EMSCRIPTEN_EVENT_TOUCHSTART) {
			if (activeTouch != -1) continue;
			activeTouch = t.identifier;
			PushMouseMove(p);
			PushMouseButton(sf::Mouse::Left, true, p);
		} else if (t.identifier == activeTouch) {
			if (type == EMSCRIPTEN_EVENT_TOUCHMOVE) {
				PushMouseMove(p);
			} else {
				PushMouseButton(sf::Mouse::Left, false, p);
				activeTouch = -1;
			}
		}
	}
	return EM_TRUE;
}

EM_BOOL OnContextMenu(int, const EmscriptenMouseEvent*, void*) { return EM_TRUE; }

// ------------------------------------------------------------------
// Gamepads
// ------------------------------------------------------------------
struct PadState {
	bool connected = false;
	int numButtons = 0;
	int numAxes = 0;
	bool buttons[sf::Joystick::ButtonCount] = {};
	float x = 0, y = 0;   // -100 .. 100
};
PadState pads[sf::Joystick::Count];
bool gamepadApiAvailable = true;

void SampleGamepads(bool emitEvents) {
	if (!gamepadApiAvailable) return;
	if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS) {
		gamepadApiAvailable = false;
		return;
	}
	int n = emscripten_get_num_gamepads();
	for (int i = 0; i < sf::Joystick::Count; i++) {
		PadState& pad = pads[i];
		EmscriptenGamepadEvent g;
		bool ok = i < n && emscripten_get_gamepad_status(i, &g) == EMSCRIPTEN_RESULT_SUCCESS && g.connected;
		if (!ok) {
			pad.connected = false;
			continue;
		}
		pad.connected = true;
		pad.numButtons = std::min(g.numButtons, (int)sf::Joystick::ButtonCount);
		pad.numAxes = g.numAxes;

		float x = g.numAxes > 0 ? (float)g.axis[0] * 100.f : 0.f;
		float y = g.numAxes > 1 ? (float)g.axis[1] * 100.f : 0.f;
		// D-pad (standard mapping buttons 12..15) overrides the stick.
		if (g.numButtons > 15 && std::strcmp(g.mapping, "standard") == 0) {
			if (g.digitalButton[14]) x = -100.f;
			if (g.digitalButton[15]) x = 100.f;
			if (g.digitalButton[12]) y = -100.f;
			if (g.digitalButton[13]) y = 100.f;
		}
		if (emitEvents) {
			if (std::fabs(x - pad.x) > 2.f || (x == 0.f) != (pad.x == 0.f)) {
				sf::Event ev;
				ev.type = sf::Event::JoystickMoved;
				ev.joystickMove.joystickId = i;
				ev.joystickMove.axis = sf::Joystick::X;
				ev.joystickMove.position = x;
				eventQueue.push_back(ev);
				pad.x = x;
			}
			if (std::fabs(y - pad.y) > 2.f || (y == 0.f) != (pad.y == 0.f)) {
				sf::Event ev;
				ev.type = sf::Event::JoystickMoved;
				ev.joystickMove.joystickId = i;
				ev.joystickMove.axis = sf::Joystick::Y;
				ev.joystickMove.position = y;
				eventQueue.push_back(ev);
				pad.y = y;
			}
			for (int b = 0; b < pad.numButtons; b++) {
				bool pressed = g.digitalButton[b] != 0;
				if (pressed != pad.buttons[b]) {
					sf::Event ev;
					ev.type = pressed ? sf::Event::JoystickButtonPressed : sf::Event::JoystickButtonReleased;
					ev.joystickButton.joystickId = i;
					ev.joystickButton.button = b;
					eventQueue.push_back(ev);
					pad.buttons[b] = pressed;
				}
			}
		} else {
			pad.x = x;
			pad.y = y;
			for (int b = 0; b < pad.numButtons; b++) pad.buttons[b] = g.digitalButton[b] != 0;
		}
	}
}

void InstallCallbacks() {
	if (callbacksInstalled) return;
	callbacksInstalled = true;
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnKey);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnKey);
	emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnBlur);
	emscripten_set_mousedown_callback(kCanvas, nullptr, EM_TRUE, OnMouse);
	emscripten_set_mouseup_callback(kCanvas, nullptr, EM_TRUE, OnMouse);
	emscripten_set_mousemove_callback(kCanvas, nullptr, EM_TRUE, OnMouse);
	emscripten_set_touchstart_callback(kCanvas, nullptr, EM_TRUE, OnTouch);
	emscripten_set_touchmove_callback(kCanvas, nullptr, EM_TRUE, OnTouch);
	emscripten_set_touchend_callback(kCanvas, nullptr, EM_TRUE, OnTouch);
	emscripten_set_touchcancel_callback(kCanvas, nullptr, EM_TRUE, OnTouch);
	EM_ASM({
		var c = document.querySelector('#canvas');
		if (c) c.addEventListener('contextmenu', function(e) { e.preventDefault(); });
	});
}

EM_JS(void, etr_fit_canvas, (), {
	if (Module.etrFitCanvas) Module.etrFitCanvas();
});

EM_JS(int, etr_available_width, (), {
	return Module.etrAvailableSize ? Module.etrAvailableSize()[0] : 1024;
});

EM_JS(int, etr_available_height, (), {
	return Module.etrAvailableSize ? Module.etrAvailableSize()[1] : 768;
});

EM_JS(void, etr_set_cursor_visible, (int visible), {
	var c = document.querySelector('#canvas');
	if (c) c.style.cursor = visible ? 'default' : 'none';
});

EM_JS(void, etr_set_title, (const char* title), {
	document.title = UTF8ToString(title);
});

EM_JS(void, etr_download_file, (const char* name, const unsigned char* data, int len, const char* mime), {
	var bytes = HEAPU8.slice(data, data + len);
	var blob = new Blob([bytes], {type: UTF8ToString(mime)});
	var a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = UTF8ToString(name);
	document.body.appendChild(a);
	a.click();
	setTimeout(function() { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
});

void WritePngToVector(void* context, void* data, int size) {
	std::vector<unsigned char>* out = static_cast<std::vector<unsigned char>*>(context);
	const unsigned char* p = static_cast<const unsigned char*>(data);
	out->insert(out->end(), p, p + size);
}

void CaptureScreenshot() {
	const int w = (int)canvasW, h = (int)canvasH;
	std::vector<unsigned char> pixels((std::size_t)w * h * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	// flip vertically and drop alpha
	std::vector<unsigned char> rgb((std::size_t)w * h * 3);
	for (int y = 0; y < h; y++) {
		const unsigned char* src = pixels.data() + (std::size_t)(h - 1 - y) * w * 4;
		unsigned char* dst = rgb.data() + (std::size_t)y * w * 3;
		for (int x = 0; x < w; x++) {
			dst[x * 3] = src[x * 4];
			dst[x * 3 + 1] = src[x * 4 + 1];
			dst[x * 3 + 2] = src[x * 4 + 2];
		}
	}
	std::vector<unsigned char> png;
	stbi_write_png_to_func(WritePngToVector, &png, w, h, 3, rgb.data(), w * 3);
	std::string name = pendingScreenshot;
	std::size_t slash = name.find_last_of('/');
	if (slash != std::string::npos) name = name.substr(slash + 1);
	etr_download_file(name.c_str(), png.data(), (int)png.size(), "image/png");
}

} // namespace

namespace sf {

// ------------------------------------------------------------------
// VideoMode
// ------------------------------------------------------------------
VideoMode VideoMode::getDesktopMode() {
	return VideoMode(etr_available_width(), etr_available_height(), 32);
}

// ------------------------------------------------------------------
// Keyboard / Mouse / Joystick
// ------------------------------------------------------------------
bool Keyboard::isKeyPressed(Key key) {
	return key >= 0 && key < KeyCount && keyState[key];
}

bool Mouse::isButtonPressed(Button button) {
	return button >= 0 && button < ButtonCount && mouseState[button];
}

Vector2i Mouse::getPosition() { return mousePos; }
Vector2i Mouse::getPosition(const Window&) { return mousePos; }

bool Joystick::isConnected(unsigned int joystick) {
	return joystick < Count && pads[joystick].connected;
}

unsigned int Joystick::getButtonCount(unsigned int joystick) {
	return isConnected(joystick) ? pads[joystick].numButtons : 0;
}

bool Joystick::hasAxis(unsigned int joystick, Axis axis) {
	if (!isConnected(joystick)) return false;
	return axis == X || axis == Y;
}

bool Joystick::isButtonPressed(unsigned int joystick, unsigned int button) {
	return isConnected(joystick) && button < ButtonCount && pads[joystick].buttons[button];
}

float Joystick::getAxisPosition(unsigned int joystick, Axis axis) {
	if (!isConnected(joystick)) return 0.f;
	if (axis == X) return pads[joystick].x;
	if (axis == Y) return pads[joystick].y;
	return 0.f;
}

void Joystick::update() { SampleGamepads(false); }

// ------------------------------------------------------------------
// Window
// ------------------------------------------------------------------
Window::Window() : open(false) {}

Window::~Window() {}

Vector2u Window::getAvailableSize() {
	return Vector2u(etr_available_width(), etr_available_height());
}

void Window::create(VideoMode mode, const String& title, Uint32, const ContextSettings& ctxSettings) {
	settings = ctxSettings;
	canvasW = mode.width;
	canvasH = mode.height;
	emscripten_set_canvas_element_size(kCanvas, canvasW, canvasH);
	etr_fit_canvas();

	if (!glContext) {
		EmscriptenWebGLContextAttributes attrs;
		emscripten_webgl_init_context_attributes(&attrs);
		attrs.alpha = EM_FALSE;
		attrs.depth = EM_TRUE;
		attrs.stencil = ctxSettings.stencilBits > 0 ? EM_TRUE : EM_FALSE;
		attrs.antialias = EM_TRUE;
		attrs.premultipliedAlpha = EM_FALSE;
		attrs.preserveDrawingBuffer = EM_FALSE;
		attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
		attrs.majorVersion = 2;
		attrs.minorVersion = 0;
		glContext = emscripten_webgl_create_context(kCanvas, &attrs);
		if (glContext <= 0) {
			EM_ASM({ if (Module.etrFatal) Module.etrFatal('WebGL 2 is not available in this browser.'); });
			return;
		}
		emscripten_webgl_make_context_current(glContext);
		glc::Init();
		InstallCallbacks();
		SampleGamepads(false);
	}
	glc::SetDefaultFramebufferSize((int)canvasW, (int)canvasH);
	glcViewport(0, 0, canvasW, canvasH);
	setTitle(title);
	open = true;
	onCreate();
}

void Window::close() { open = false; }

bool Window::pollEvent(Event& event) {
	if (eventQueue.empty()) SampleGamepads(true);
	if (eventQueue.empty()) return false;
	event = eventQueue.front();
	eventQueue.pop_front();
	return true;
}

void Window::display() {
	// The browser presents the canvas when the frame callback returns.
	if (!pendingScreenshot.empty()) {
		CaptureScreenshot();
		pendingScreenshot.clear();
	}
}

void Window::setKeyRepeatEnabled(bool enabled) { keyRepeat = enabled; }

void Window::setMouseCursorVisible(bool visible) { etr_set_cursor_visible(visible ? 1 : 0); }

void Window::setTitle(const String& title) { etr_set_title(title.toUtf8().c_str()); }

Vector2u Window::getSize() const { return Vector2u(canvasW, canvasH); }

void RenderWindow::requestScreenshot(const std::string& filename) const { pendingScreenshot = filename; }

} // namespace sf
