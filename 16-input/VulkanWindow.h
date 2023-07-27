#pragma once

#include <vulkan/vulkan.hpp>
#include <functional>
#include <bitset>



class VulkanWindow {
public:

	// general function prototypes
	typedef void FrameCallback(VulkanWindow& window);
	typedef void RecreateSwapchainCallback(VulkanWindow& window,
		const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D newSurfaceExtent);
	typedef void CloseCallback(VulkanWindow& window);

	// input structures and enums
	struct MouseButton {
		enum EnumType {
			Left,
			Right,
			Middle,
			X1,
			X2,
			Unknown = 0xff,
		};
	};
	enum class ButtonState : uint8_t { Pressed, Released };
	enum class KeyState : uint8_t { Pressed, Released };
	struct Modifier {
		enum EnumType {
			Ctrl,
			Shift,
			Alt,
			Meta,
		};
	};
	struct MouseState {
		int posX, posY;  // position of the mouse in window client area coordinates (relative to the upper-left corner)
		int relX, relY;  // relative against the state of previous mouse callback
		std::bitset<16> buttons;
		std::bitset<16> mods;
	};
	enum class ScanCode : uint16_t {
		Unknown = 0, Escape = 1,
		One = 2, Two = 3, Three = 4, Four = 5, Five = 6, Six = 7, Seven = 8, Eight = 9, Nine = 10,
		Zero = 11, Minus = 12, Equal = 13, Backspace = 14, Tab = 15, Q = 16, W = 17, E = 18, R = 19,
		T = 20, Y = 21, U = 22, I = 23, O = 24, P = 25, LeftBracket = 26, RightBracket = 27, Enter = 28, Return = 28, LeftControl = 29,
		A = 30, S = 31, D = 32, F = 33, G = 34, H = 35, J = 36, K = 37, L = 38, Semicolon = 39,
		Apostrophe = 40, GraveAccent = 41, LeftShift = 42, Backslash = 43, Z = 44, X = 45, C = 46, V = 47, B = 48, N = 49,
		M = 50, Comma = 51, Period = 52, Slash = 53, RightShift = 54, KeypadMultiply = 55, LeftAlt = 56, Space = 57, CapsLock = 58,
		F1 = 59, F2 = 60, F3 = 61, F4 = 62, F5 = 63, F6 = 64, F7 = 65, F8 = 66, F9 = 67, F10 = 68, NumLock = 69, NumLockClear = 69,
		ScrollLock = 70, Keypad7 = 71, Keypad8 = 72, Keypad9 = 73, KeypadMinus = 74, Keypad4 = 75, Keypad5 = 76, Keypad6 = 77, KeypadPlus = 78, Keypad1 = 79,
		Keypad2 = 80, Keypad3 = 81, Keypad0 = 82, KeypadPeriod = 83, /* Unknown, */ /* Unknown, */ NonUSBackslash = 86, F11 = 87, F12 = 88,

		KeypadEnter = 96, RightControl = 97, KeypadDivide = 98, PrintScreen = 99, RightAlt = 100 /* RightAlt might be configured as AltGr on Windows. In that case, LeftControl press is generated by RightAlt as well. To change AltGr to Alt, switch to US-English keyboard layout. */,
		Home = 102, Up = 103, PageUp = 104, Left = 105, Right = 106, End = 107, Down = 108, PageDown = 109, Insert = 110, Delete = 111,
		Mute = 113, VolumeDown = 114, VolumeUp = 115,
		PauseBreak = 119,
		LeftGUI = 125, RightGUI = 126 /* untested */, Application = 127,
		Calculator = 140,
		Mail = 155, Back = 158, Forward = 159,
		MediaNext = 163, MediaPlayPause = 164, MediaPrev = 165, MediaStop = 166,
		MediaSelect = 171, BrowserHome = 172,
		Search = 217,
	};
	using KeyCode = uint32_t;  //< Single unicode character. It is a number representing particular code point as defined by Unicode Standard.
	static constexpr KeyCode fromUtf8(const char* s);
	static constexpr KeyCode fromAscii(char ch);
	static std::string toString(KeyCode k);
	static std::array<char, 5> toCharArray(KeyCode k);
	struct Key {
		static constexpr const KeyCode Null = 0;
		static constexpr const KeyCode A = 'A';  static constexpr const KeyCode B = 'B';
		static constexpr const KeyCode C = 'C';  static constexpr const KeyCode D = 'D';
		static constexpr const KeyCode E = 'E';  static constexpr const KeyCode F = 'F';
		static constexpr const KeyCode G = 'G';  static constexpr const KeyCode H = 'H';
		static constexpr const KeyCode I = 'I';  static constexpr const KeyCode J = 'J';
		static constexpr const KeyCode K = 'K';  static constexpr const KeyCode L = 'L';
		static constexpr const KeyCode M = 'M';  static constexpr const KeyCode N = 'N';
		static constexpr const KeyCode O = 'O';  static constexpr const KeyCode P = 'P';
		static constexpr const KeyCode Q = 'Q';  static constexpr const KeyCode R = 'R';
		static constexpr const KeyCode S = 'S';  static constexpr const KeyCode T = 'T';
		static constexpr const KeyCode U = 'U';  static constexpr const KeyCode V = 'V';
		static constexpr const KeyCode W = 'W';  static constexpr const KeyCode X = 'X';
		static constexpr const KeyCode Y = 'Y';  static constexpr const KeyCode Z = 'Z';
		static constexpr const KeyCode a = 'a';  static constexpr const KeyCode b = 'b';
		static constexpr const KeyCode c = 'c';  static constexpr const KeyCode d = 'd';
		static constexpr const KeyCode e = 'e';  static constexpr const KeyCode f = 'f';
		static constexpr const KeyCode g = 'g';  static constexpr const KeyCode h = 'h';
		static constexpr const KeyCode i = 'i';  static constexpr const KeyCode j = 'j';
		static constexpr const KeyCode k = 'k';  static constexpr const KeyCode l = 'l';
		static constexpr const KeyCode m = 'm';  static constexpr const KeyCode n = 'n';
		static constexpr const KeyCode o = 'o';  static constexpr const KeyCode p = 'p';
		static constexpr const KeyCode q = 'q';  static constexpr const KeyCode r = 'r';
		static constexpr const KeyCode s = 's';  static constexpr const KeyCode t = 't';
		static constexpr const KeyCode u = 'u';  static constexpr const KeyCode v = 'v';
		static constexpr const KeyCode w = 'w';  static constexpr const KeyCode x = 'x';
		static constexpr const KeyCode y = 'y';  static constexpr const KeyCode z = 'z';
		static constexpr const KeyCode One   = '1';  static constexpr const KeyCode Two   = '2';
		static constexpr const KeyCode Three = '3';  static constexpr const KeyCode Four  = '4';
		static constexpr const KeyCode Five  = '5';  static constexpr const KeyCode Six   = '6';
		static constexpr const KeyCode Seven = '7';  static constexpr const KeyCode Eight = '8';
		static constexpr const KeyCode Nine  = '9';  static constexpr const KeyCode Zero  = '0';
		static constexpr const KeyCode Exclam = '!';  static constexpr const KeyCode At = '@';
		static constexpr const KeyCode Hash = '#';  static constexpr const KeyCode Dollar = '$';
		static constexpr const KeyCode Percent = '%';  static constexpr const KeyCode Caret = '^';
		static constexpr const KeyCode Ampersand = '&';  static constexpr const KeyCode Asterisk = '*';
		static constexpr const KeyCode LeftParenthesis = '(';  static constexpr const KeyCode RightParenthesis = ')';
		static constexpr const KeyCode LeftSquareBracket = '[';  static constexpr const KeyCode RightSquareBracket = ']';
		static constexpr const KeyCode LeftCurlyBracket = '{';  static constexpr const KeyCode RightCurlyBracket = '}';
		static constexpr const KeyCode Space = ' ';  static constexpr const KeyCode Backspace = 0x08;
		static constexpr const KeyCode Enter = '\n';  static constexpr const KeyCode Escape = 0x1b;
		static constexpr const KeyCode Plus = '+';  static constexpr const KeyCode Minus = '-';
		static constexpr const KeyCode Slash = '/';  static constexpr const KeyCode Backslash = '\\';
		static constexpr const KeyCode Less = '<';  static constexpr const KeyCode Greater = '>';
		static constexpr const KeyCode Colon = ':';  static constexpr const KeyCode Semicolon = ';';
		static constexpr const KeyCode Dot = '.';  static constexpr const KeyCode Comma = ',';
		static constexpr const KeyCode Equal = '=';  static constexpr const KeyCode Underscore = '_';
		static constexpr const KeyCode Question = '?';  static constexpr const KeyCode Bar = '|';
		static constexpr const KeyCode DoubleQuote = '"';  static constexpr const KeyCode Apostrophe = '\'';
		static constexpr const KeyCode GraveAccent = '`';  static constexpr const KeyCode Tilde = '~';
		static constexpr const KeyCode Tab = '\t';
		static constexpr const KeyCode Copyright = 0xa9;  static constexpr const KeyCode Registered = 0xae;
		static constexpr const KeyCode Section = 0xa7;  static constexpr const KeyCode Paragraph = Section;
		static constexpr const KeyCode Pound = 0xa3;  static constexpr const KeyCode Euro = 0x20ac;
	};

	// input function prototypes
	typedef void MouseMoveCallback(VulkanWindow& window, const MouseState& mouseState);
	typedef void MouseButtonCallback(VulkanWindow& window, MouseButton::EnumType button, ButtonState buttonState, const MouseState& mouseState);
	typedef void MouseWheelCallback(VulkanWindow& window, int wheelX, int wheelY, const MouseState& mouseState);
	typedef void KeyCallback(VulkanWindow& window, KeyState keyState, uint16_t scanCode, KeyCode key);

protected:

#if defined(USE_PLATFORM_WIN32)

	void* _hwnd = nullptr;  // void* is used instead of HWND type to avoid #include <windows.h>
	enum class FramePendingState { NotPending, Pending, TentativePending };
	FramePendingState _framePendingState;
	bool _visible;
	bool _hiddenWindowFramePending;

	static inline void* _hInstance = 0;  // void* is used instead of HINSTANCE type to avoid #include <windows.h>
	static inline uint16_t _windowClass = 0;  // uint16_t is used instead of ATOM type to avoid #include <windows.h>
	static inline const std::vector<const char*> _requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_win32_surface" };

#elif defined(USE_PLATFORM_XLIB)

	unsigned long _window = 0;  // unsigned long is used for Window type
	bool _framePending;
	bool _visible;
	bool _fullyObscured;
	bool _iconVisible;
	bool _minimized;

	static inline struct _XDisplay* _display = nullptr;  // struct _XDisplay* is used instead of Display* type
	static inline unsigned long _wmDeleteMessage;  // unsigned long is used for Atom type
	static inline unsigned long _wmStateProperty;  // unsigned long is used for Atom type
	static inline const std::vector<const char*> _requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_xlib_surface" };

	void updateMinimized();

#elif defined(USE_PLATFORM_WAYLAND)

	// objects
	struct wl_surface* _wlSurface = nullptr;
	struct xdg_surface* _xdgSurface = nullptr;
	struct xdg_toplevel* _xdgTopLevel = nullptr;
	struct zxdg_toplevel_decoration_v1* _decoration = nullptr;
	struct wl_callback* _scheduledFrameCallback = nullptr;

	// wl and xdg listeners
	struct WaylandListeners* _listeners = nullptr;

	// state
	bool _forcedFrame;
	std::string _title;

	// globals
	static inline struct wl_display* _display = nullptr;
	static inline struct wl_registry* _registry;
	static inline struct wl_compositor* _compositor = nullptr;
	static inline struct xdg_wm_base* _xdgWmBase = nullptr;
	static inline struct zxdg_decoration_manager_v1* _zxdgDecorationManagerV1 = nullptr;
	static inline struct wl_seat* _seat = nullptr;
	static inline struct wl_pointer* _pointer = nullptr;
	static inline struct wl_keyboard* _keyboard = nullptr;
	static inline struct xkb_context* _xkbContext = nullptr;
	static inline struct xkb_state* _xkbState = nullptr;

	static inline const std::vector<const char*> _requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_wayland_surface" };

#elif defined(USE_PLATFORM_SDL2)

	struct SDL_Window* _window = nullptr;
	bool _framePending;
	bool _hiddenWindowFramePending;
	bool _visible;
	bool _minimized;

#elif defined(USE_PLATFORM_GLFW)

	struct GLFWwindow* _window = nullptr;
	enum class FramePendingState { NotPending, Pending, TentativePending };
	FramePendingState _framePendingState;
	bool _visible;
	bool _minimized;

#elif defined(USE_PLATFORM_QT)

	class QWindow* _window = nullptr;
	friend class QtRenderingWindow;

#endif

	std::function<FrameCallback> _frameCallback;
	vk::Instance _instance;
	vk::PhysicalDevice _physicalDevice;
	vk::Device _device;
	vk::SurfaceKHR _surface;

	vk::Extent2D _surfaceExtent = vk::Extent2D(0,0);
	bool _swapchainResizePending = true;
	std::function<RecreateSwapchainCallback> _recreateSwapchainCallback;
	std::function<CloseCallback> _closeCallback;

	MouseState _mouseState = {};
	std::function<MouseMoveCallback> _mouseMoveCallback;
	std::function<MouseButtonCallback> _mouseButtonCallback;
	std::function<MouseWheelCallback> _mouseWheelCallback;
	std::function<KeyCallback> _keyCallback;

public:

	// initialization and finalization
	static void init();
	static void init(void* data);
	static void init(int& argc, char* argv[]);
	static void finalize() noexcept;

	// construction and destruction
	VulkanWindow() = default;
	VulkanWindow(VulkanWindow&& other);
	~VulkanWindow();
	void destroy() noexcept;
	VulkanWindow& operator=(VulkanWindow&& rhs) noexcept;

	// deleted constructors and operators
	VulkanWindow(const VulkanWindow&) = delete;
	VulkanWindow& operator=(const VulkanWindow&) = delete;

	// general methods
	vk::SurfaceKHR create(vk::Instance instance, vk::Extent2D surfaceExtent, const char* title = "Vulkan window");
	void show();
	void hide();
	void setVisible(bool value);
	void renderFrame();
	static void mainLoop();
	static void exitMainLoop();

	// callbacks
	void setRecreateSwapchainCallback(std::function<RecreateSwapchainCallback>&& cb);
	void setRecreateSwapchainCallback(const std::function<RecreateSwapchainCallback>& cb);
	void setFrameCallback(std::function<FrameCallback>&& cb, vk::PhysicalDevice physicalDevice, vk::Device device);
	void setFrameCallback(const std::function<FrameCallback>& cb, vk::PhysicalDevice physicalDevice, vk::Device device);
	void setCloseCallback(std::function<CloseCallback>&& cb);
	void setCloseCallback(const std::function<CloseCallback>& cb);
	void setMouseMoveCallback(std::function<MouseMoveCallback>&& cb);
	void setMouseMoveCallback(const std::function<MouseMoveCallback>& cb);
	void setMouseButtonCallback(std::function<MouseButtonCallback>&& cb);
	void setMouseButtonCallback(const std::function<MouseButtonCallback>& cb);
	void setMouseWheelCallback(std::function<MouseWheelCallback>&& cb);
	void setMouseWheelCallback(const std::function<MouseWheelCallback>& cb);
	void setKeyCallback(std::function<KeyCallback>&& cb);
	void setKeyCallback(const std::function<KeyCallback>& cb);
	const std::function<RecreateSwapchainCallback>& recreateSwapchainCallback() const;
	const std::function<FrameCallback>& frameCallback() const;
	const std::function<CloseCallback>& closeCallback() const;
	const std::function<MouseMoveCallback>& mouseMoveCallback() const;
	const std::function<MouseButtonCallback>& mouseButtonCallback() const;
	const std::function<MouseWheelCallback>& mouseWheelCallback() const;
	const std::function<KeyCallback>& keyCallback() const;

	// getters
	vk::SurfaceKHR surface() const;
	vk::Extent2D surfaceExtent() const;
	bool isVisible() const;

	// schedule methods
	void scheduleFrame();
	void scheduleSwapchainResize();

	// exception handling
	static inline std::exception_ptr thrownException;

	// required Vulkan Instance extensions
	static const std::vector<const char*>& requiredExtensions();
	static std::vector<const char*>& appendRequiredExtensions(std::vector<const char*>& v);
	static uint32_t requiredExtensionCount();
	static const char* const* requiredExtensionNames();

};


// inline methods
inline void VulkanWindow::setVisible(bool value)  { if(value) show(); else hide(); }
inline void VulkanWindow::setRecreateSwapchainCallback(std::function<RecreateSwapchainCallback>&& cb)  { _recreateSwapchainCallback = move(cb); }
inline void VulkanWindow::setRecreateSwapchainCallback(const std::function<RecreateSwapchainCallback>& cb)  { _recreateSwapchainCallback = cb; }
inline void VulkanWindow::setFrameCallback(std::function<FrameCallback>&& cb, vk::PhysicalDevice physicalDevice, vk::Device device)  { _frameCallback = std::move(cb); _physicalDevice = physicalDevice; _device = device; }
inline void VulkanWindow::setFrameCallback(const std::function<FrameCallback>& cb, vk::PhysicalDevice physicalDevice, vk::Device device)  { _frameCallback = cb; _physicalDevice = physicalDevice; _device = device; }
inline void VulkanWindow::setCloseCallback(std::function<CloseCallback>&& cb)  { _closeCallback = move(cb); }
inline void VulkanWindow::setCloseCallback(const std::function<CloseCallback>& cb)  { _closeCallback = cb; }
inline void VulkanWindow::setMouseMoveCallback(std::function<MouseMoveCallback>&& cb)  { _mouseMoveCallback = move(cb); }
inline void VulkanWindow::setMouseMoveCallback(const std::function<MouseMoveCallback>& cb)  { _mouseMoveCallback = cb; }
inline void VulkanWindow::setMouseButtonCallback(std::function<MouseButtonCallback>&& cb)  { _mouseButtonCallback = move(cb); }
inline void VulkanWindow::setMouseButtonCallback(const std::function<MouseButtonCallback>& cb)  { _mouseButtonCallback = cb; }
inline void VulkanWindow::setMouseWheelCallback(std::function<MouseWheelCallback>&& cb)  { _mouseWheelCallback = move(cb); }
inline void VulkanWindow::setMouseWheelCallback(const std::function<MouseWheelCallback>& cb)  { _mouseWheelCallback = cb; }
inline void VulkanWindow::setKeyCallback(std::function<KeyCallback>&& cb)  { _keyCallback = move(cb); }
inline void VulkanWindow::setKeyCallback(const std::function<KeyCallback>& cb)  { _keyCallback = cb; }
inline const std::function<VulkanWindow::RecreateSwapchainCallback>& VulkanWindow::recreateSwapchainCallback() const  { return _recreateSwapchainCallback; }
inline const std::function<VulkanWindow::FrameCallback>& VulkanWindow::frameCallback() const  { return _frameCallback; }
inline const std::function<VulkanWindow::CloseCallback>& VulkanWindow::closeCallback() const  { return _closeCallback; }
inline const std::function<VulkanWindow::MouseMoveCallback>& VulkanWindow::mouseMoveCallback() const  { return _mouseMoveCallback; }
inline const std::function<VulkanWindow::MouseButtonCallback>& VulkanWindow::mouseButtonCallback() const  { return _mouseButtonCallback; }
inline const std::function<VulkanWindow::MouseWheelCallback>& VulkanWindow::mouseWheelCallback() const  { return _mouseWheelCallback; }
inline const std::function<VulkanWindow::KeyCallback>& VulkanWindow::keyCallback() const  { return _keyCallback; }
inline vk::SurfaceKHR VulkanWindow::surface() const  { return _surface; }
inline vk::Extent2D VulkanWindow::surfaceExtent() const  { return _surfaceExtent; }
#if defined(USE_PLATFORM_WIN32) || defined(USE_PLATFORM_XLIB) || defined(USE_PLATFORM_SDL2) || defined(USE_PLATFORM_GLFW)
inline bool VulkanWindow::isVisible() const  { return _visible; }
#elif defined(USE_PLATFORM_WAYLAND)
inline bool VulkanWindow::isVisible() const  { return _xdgSurface != nullptr; }
#endif
inline void VulkanWindow::scheduleSwapchainResize()  { _swapchainResizePending = true; scheduleFrame(); }
#if defined(USE_PLATFORM_WIN32) || defined(USE_PLATFORM_XLIB) || defined(USE_PLATFORM_WAYLAND)
inline const std::vector<const char*>& VulkanWindow::requiredExtensions()  { return _requiredInstanceExtensions; }
inline std::vector<const char*>& VulkanWindow::appendRequiredExtensions(std::vector<const char*>& v)  { v.insert(v.end(), _requiredInstanceExtensions.begin(), _requiredInstanceExtensions.end()); return v; }
inline uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(_requiredInstanceExtensions.size()); }
inline const char* const* VulkanWindow::requiredExtensionNames()  { return _requiredInstanceExtensions.data(); }
#endif
inline constexpr VulkanWindow::KeyCode VulkanWindow::fromAscii(char ch)  { return VulkanWindow::KeyCode(ch); }


// nifty counter / Schwarz counter
// (VulkanWindow::finalize() must be called after all VulkanWindow objects were destroyed.
// We are doing it using Nifty counter programming idiom.)
struct VulkanWindowInitAndFinalizer
{
	VulkanWindowInitAndFinalizer();
	~VulkanWindowInitAndFinalizer();
};
static VulkanWindowInitAndFinalizer vulkanWindowInitAndFinalizer;
