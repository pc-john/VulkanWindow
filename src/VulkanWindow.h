#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <bitset>
#include <exception>
#include <functional>



class VulkanWindow {
public:

	// general function prototypes
	typedef void FrameCallback(VulkanWindow& window);
	typedef void ResizeCallback(VulkanWindow& window,
		const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D newSurfaceExtent);
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
		float posX, posY;  // position of the mouse in window client area coordinates (relative to the upper-left corner)
		float relX, relY;  // relative against the state of previous mouse callback
		std::bitset<16> buttons;
		std::bitset<16> modifiers;
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
	enum class KeyCode : char32_t {
		Unknown = 0,
		A = 'a', B = 'b', C = 'c', D = 'd',
		E = 'e', F = 'f', G = 'g', H = 'h',
		I = 'i', J = 'j', K = 'k', L = 'l',
		M = 'm', N = 'n', O = 'o', P = 'p',
		Q = 'q', R = 'r', S = 's', T = 't',
		U = 'u', V = 'v', W = 'w', X = 'x',
		Y = 'y', Z = 'z',
		Space = ' ', Backspace = 0x08,
		Enter = '\n', Escape = 0x1b,
		Tab = '\t',
	};
	static constexpr KeyCode fromUtf8(const char* s);
	static constexpr KeyCode fromAscii(char ch);
	static std::string toString(KeyCode keyCode);
	static std::array<char, 5> toCharArray(KeyCode keyCode);

	// input function prototypes
	typedef void MouseMoveCallback(VulkanWindow& window, const MouseState& mouseState);
	typedef void MouseButtonCallback(VulkanWindow& window, MouseButton::EnumType button, ButtonState buttonState, const MouseState& mouseState);
	typedef void MouseWheelCallback(VulkanWindow& window, float wheelX, float wheelY, const MouseState& mouseState);
	typedef void KeyCallback(VulkanWindow& window, KeyState newKeyState, ScanCode scanCode, KeyCode key);

protected:

#if defined(USE_PLATFORM_WIN32)

	void* _hwnd = nullptr;  // void* is used instead of HWND type to avoid #include <windows.h>
	enum class FramePendingState { NotPending, Pending, TentativePending };
	FramePendingState _framePendingState;
	bool _visible = false;
	bool _hiddenWindowFramePending;

	static inline void* _hInstance = 0;  // void* is used instead of HINSTANCE type to avoid #include <windows.h>
	static inline uint16_t _windowClass = 0;  // uint16_t is used instead of ATOM type to avoid #include <windows.h>
	static inline const std::vector<const char*> _requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_win32_surface" };

#elif defined(USE_PLATFORM_XLIB)

	unsigned long _window = 0;  // unsigned long is used for Window type
	bool _framePending;
	bool _visible = false;
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
	struct libdecor_frame* _libdecorFrame = nullptr;
	struct wl_callback* _scheduledFrameCallback = nullptr;

	// state
	bool _forcedFrame;
	std::string _title;

	// globals
	static inline struct wl_display* _display = nullptr;
	static inline struct wl_registry* _registry;
	static inline struct wl_compositor* _compositor = nullptr;
	static inline struct xdg_wm_base* _xdgWmBase = nullptr;
	static inline struct zxdg_decoration_manager_v1* _zxdgDecorationManagerV1 = nullptr;
	static inline struct libdecor* _libdecorContext = nullptr;
	static inline struct wl_shm* _shm = nullptr;
	static inline struct wl_cursor_theme* _cursorTheme = nullptr;
	static inline struct wl_surface* _cursorSurface = nullptr;
	static inline int _cursorHotspotX;
	static inline int _cursorHotspotY;
	static inline struct wl_seat* _seat = nullptr;
	static inline struct wl_pointer* _pointer = nullptr;
	static inline struct wl_keyboard* _keyboard = nullptr;
	static inline struct xkb_context* _xkbContext = nullptr;
	static inline struct xkb_state* _xkbState = nullptr;
	static inline std::bitset<16> _modifiers;

	static inline const std::vector<const char*> _requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_wayland_surface" };

#elif defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2)

	struct SDL_Window* _window = nullptr;
	bool _framePending;
	bool _hiddenWindowFramePending;
	bool _visible = false;
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

#else
# error "Define one of USE_PLATFORM_* macros to use VulkanWindow."
#endif

	std::function<FrameCallback> _frameCallback;
	VkInstance _instance = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _device = nullptr;
	VkSurfaceKHR _surface = nullptr;
	PFN_vkGetInstanceProcAddr _vulkanGetInstanceProcAddr = nullptr;
	PFN_vkDeviceWaitIdle _vulkanDeviceWaitIdle;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR _vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR;

	VkExtent2D _surfaceExtent = {0,0};
	bool _resizePending = true;
	std::function<ResizeCallback> _resizeCallback;
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
	VulkanWindow(VulkanWindow&& other) noexcept;
	~VulkanWindow();
	void destroy() noexcept;
	VulkanWindow& operator=(VulkanWindow&& rhs) noexcept;

	// deleted constructors and operators
	VulkanWindow(const VulkanWindow&) = delete;
	VulkanWindow& operator=(const VulkanWindow&) = delete;

	// general methods
	VkSurfaceKHR create(VkInstance instance, VkExtent2D surfaceExtent, const char* title = "Vulkan window",
	                    PFN_vkGetInstanceProcAddr getInstanceProcAddr = ::vkGetInstanceProcAddr);
	void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);
	void show();
	void hide();
	void setVisible(bool value);
	void renderFrame();
	static void mainLoop();
	static void exitMainLoop();

	// callbacks
	void setFrameCallback(std::function<FrameCallback>&& cb);
	void setFrameCallback(const std::function<FrameCallback>& cb);
	void setResizeCallback(std::function<ResizeCallback>&& cb);
	void setResizeCallback(const std::function<ResizeCallback>& cb);
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
	const std::function<FrameCallback>& frameCallback() const;
	const std::function<ResizeCallback>& resizeCallback() const;
	const std::function<CloseCallback>& closeCallback() const;
	const std::function<MouseMoveCallback>& mouseMoveCallback() const;
	const std::function<MouseButtonCallback>& mouseButtonCallback() const;
	const std::function<MouseWheelCallback>& mouseWheelCallback() const;
	const std::function<KeyCallback>& keyCallback() const;

	// getters
	VkSurfaceKHR surface() const;
	VkExtent2D surfaceExtent() const;
	bool isVisible() const;

	// schedule methods
	void scheduleFrame();
	void scheduleResize();

	// exception handling
	static inline std::exception_ptr thrownException;

	// required Vulkan Instance extensions
	// (calling VulkanWindow::requiredExtensions() requires already initialized QGuiApplication on Qt,
	// and VulkanWindow::init(...) already called on SDL and GLFW)
	static const std::vector<const char*>& requiredExtensions();
	static std::vector<const char*>& appendRequiredExtensions(std::vector<const char*>& v);
	static uint32_t requiredExtensionCount();
	static const char* const* requiredExtensionNames();

};


// inline methods
inline void VulkanWindow::setVisible(bool value)  { if(value) show(); else hide(); }
inline void VulkanWindow::setFrameCallback(std::function<FrameCallback>&& cb)  { _frameCallback = std::move(cb); }
inline void VulkanWindow::setFrameCallback(const std::function<FrameCallback>& cb)  { _frameCallback = cb; }
inline void VulkanWindow::setResizeCallback(std::function<ResizeCallback>&& cb)  { _resizeCallback = move(cb); }
inline void VulkanWindow::setResizeCallback(const std::function<ResizeCallback>& cb)  { _resizeCallback = cb; }
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
inline const std::function<VulkanWindow::FrameCallback>& VulkanWindow::frameCallback() const  { return _frameCallback; }
inline const std::function<VulkanWindow::ResizeCallback>& VulkanWindow::resizeCallback() const  { return _resizeCallback; }
inline const std::function<VulkanWindow::CloseCallback>& VulkanWindow::closeCallback() const  { return _closeCallback; }
inline const std::function<VulkanWindow::MouseMoveCallback>& VulkanWindow::mouseMoveCallback() const  { return _mouseMoveCallback; }
inline const std::function<VulkanWindow::MouseButtonCallback>& VulkanWindow::mouseButtonCallback() const  { return _mouseButtonCallback; }
inline const std::function<VulkanWindow::MouseWheelCallback>& VulkanWindow::mouseWheelCallback() const  { return _mouseWheelCallback; }
inline const std::function<VulkanWindow::KeyCallback>& VulkanWindow::keyCallback() const  { return _keyCallback; }
inline VkSurfaceKHR VulkanWindow::surface() const  { return _surface; }
inline VkExtent2D VulkanWindow::surfaceExtent() const  { return _surfaceExtent; }
#if defined(USE_PLATFORM_WIN32) || defined(USE_PLATFORM_XLIB) || defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2) || defined(USE_PLATFORM_GLFW)
inline bool VulkanWindow::isVisible() const  { return _visible; }
#elif defined(USE_PLATFORM_WAYLAND)
inline bool VulkanWindow::isVisible() const  { return _xdgSurface != nullptr || _libdecorFrame != nullptr; }
#endif
inline void VulkanWindow::scheduleResize()  { _resizePending = true; scheduleFrame(); }
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
