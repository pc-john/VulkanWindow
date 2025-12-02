#if defined(USE_PLATFORM_WIN32)
# define VK_USE_PLATFORM_WIN32_KHR
# ifndef NOMINMAX
#  define NOMINMAX  // avoid the definition of min and max macros by windows.h
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN  // reduce amount of included files by windows.h
# endif
# include <windows.h>
# include <windowsx.h>
# include <tchar.h>
# include <type_traits>
#elif defined(USE_PLATFORM_XLIB)
# define VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xutil.h>
# include <map>
#elif defined(USE_PLATFORM_WAYLAND)
# define VK_USE_PLATFORM_WAYLAND_KHR
# include "xdg-shell-client-protocol.h"
# include "xdg-decoration-client-protocol.h"
# include <wayland-cursor.h>
# include <climits>
# include <cstring>
# include <dlfcn.h>
# include <sys/mman.h>
# include <unistd.h>
# include <map>
#elif defined(USE_PLATFORM_SDL3)
# include <SDL3/SDL_error.h>
# include <SDL3/SDL_events.h>
# include <SDL3/SDL_hints.h>
# include <SDL3/SDL_init.h>
# include <SDL3/SDL_keycode.h>
# include <SDL3/SDL_properties.h>
# include <SDL3/SDL_scancode.h>
# include <SDL3/SDL_timer.h>
# include <SDL3/SDL_video.h>
# include <SDL3/SDL_vulkan.h>
#elif defined(USE_PLATFORM_SDL2)
# include "SDL.h"
# include "SDL_vulkan.h"
# include <cmath>
# include <memory>
#elif defined(USE_PLATFORM_GLFW)
# include <vulkan/vulkan.h>  // include Vulkan before GLFW/glfw3.h otherwise some glfw functions are not present
# define GLFW_INCLUDE_NONE  // do not include OpenGL headers
# include <GLFW/glfw3.h>
# include <cmath>
#elif defined(USE_PLATFORM_QT)
# include <QGuiApplication>
# include <QWindow>
# include <QVulkanInstance>
# include <QMouseEvent>
# include <QWheelEvent>
# include <fstream>
#endif
#include "VulkanWindow.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <iostream>  // for debugging

// xcbcommon types and funcs
// (we avoid dependency on include xkbcommon/xkbcommon.h to lessen VulkanWindow dependencies)
#if defined(USE_PLATFORM_XLIB)
typedef uint32_t xkb_keysym_t;
extern "C" uint32_t xkb_keysym_to_utf32(xkb_keysym_t keysym);
#endif

// xkb type and function definitions
// (we avoid dependency on include xkbcommon/xkbcommon.h to lessen VulkanWindow dependencies;
// instead we replace the include by the following enums and structs)
#if defined(USE_PLATFORM_WAYLAND)
enum xkb_context_flags {
	XKB_CONTEXT_NO_FLAGS = 0,
};
enum xkb_keymap_format {
	XKB_KEYMAP_FORMAT_TEXT_V1 = 1,
};
enum xkb_keymap_compile_flags {
	XKB_KEYMAP_COMPILE_NO_FLAGS = 0,
};
enum xkb_state_component {};
typedef uint32_t xkb_keycode_t;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_mod_mask_t;
typedef uint32_t xkb_layout_index_t;
extern "C" struct xkb_context* xkb_context_new(enum xkb_context_flags flags);
extern "C" struct xkb_keymap* xkb_keymap_new_from_string(
	struct xkb_context* context, const char* string,
	enum xkb_keymap_format format, enum xkb_keymap_compile_flags flags);
extern "C" void xkb_keymap_unref(struct xkb_keymap* keymap);
extern "C" struct xkb_state* xkb_state_new(struct xkb_keymap* keymap);
extern "C" void xkb_state_unref(struct xkb_state* state);
extern "C" enum xkb_state_component xkb_state_update_mask(
	struct xkb_state *state, xkb_mod_mask_t depressed_mods,
	xkb_mod_mask_t latched_mods, xkb_mod_mask_t locked_mods,
	xkb_layout_index_t depressed_layout, xkb_layout_index_t latched_layout,
	xkb_layout_index_t locked_layout);
extern "C" uint32_t xkb_state_key_get_utf32(struct xkb_state* state, xkb_keycode_t key);
extern "C" void xkb_context_unref(struct xkb_context* context);
#endif

// libdecor enums and structs
// (we avoid dependency on include libdecor-0/libdecor.h to lessen VulkanWindow dependencies;
// instead we replace the include by the following enums and structs)
#if defined(USE_PLATFORM_WAYLAND)
enum libdecor_error {
	LIBDECOR_ERROR_COMPOSITOR_INCOMPATIBLE,
	LIBDECOR_ERROR_INVALID_FRAME_CONFIGURATION,
};
enum libdecor_window_state {
	LIBDECOR_WINDOW_STATE_NONE = 0,
	LIBDECOR_WINDOW_STATE_ACTIVE = 1 << 0,
	LIBDECOR_WINDOW_STATE_MAXIMIZED = 1 << 1,
	LIBDECOR_WINDOW_STATE_FULLSCREEN = 1 << 2,
	LIBDECOR_WINDOW_STATE_TILED_LEFT = 1 << 3,
	LIBDECOR_WINDOW_STATE_TILED_RIGHT = 1 << 4,
	LIBDECOR_WINDOW_STATE_TILED_TOP = 1 << 5,
	LIBDECOR_WINDOW_STATE_TILED_BOTTOM = 1 << 6,
	LIBDECOR_WINDOW_STATE_SUSPENDED = 1 << 7,
};
struct libdecor_configuration;
struct libdecor_interface {
	void (*error)(struct libdecor* context, enum libdecor_error error, const char* message);
	void (*reserved0)();
	void (*reserved1)();
	void (*reserved2)();
	void (*reserved3)();
	void (*reserved4)();
	void (*reserved5)();
	void (*reserved6)();
	void (*reserved7)();
	void (*reserved8)();
	void (*reserved9)();
};
struct libdecor_frame_interface {
	void (*configure)(struct libdecor_frame* frame,
		struct libdecor_configuration* configuration, void* user_data);
	void (*close)(struct libdecor_frame* frame, void* user_data);
	void (*commit)(struct libdecor_frame* frame, void* user_data);
	void (*dismiss_popup)(struct libdecor_frame* frame,
		const char* seat_name, void* user_data);
	void (*reserved0)();
	void (*reserved1)();
	void (*reserved2)();
	void (*reserved3)();
	void (*reserved4)();
	void (*reserved5)();
	void (*reserved6)();
	void (*reserved7)();
	void (*reserved8)();
	void (*reserved9)();
};
struct libdecor_frame_private_workaround {  // taken from libdecor.c to workaround missing libdecor_frame_set_user_data();
	// libdecor_frame_set_user_data() is already available in the master branch since 2024-01-15, but it is missing
	// in libdecor 0.1.0 to 0.2.2, e.g. in all libdecor releases as of today (2025-02-27);
	// it is expected to come out in the first release after 0.2.2;
	// to workaround it, we use this structure;
	// libdecor_frame_private stays the same for its first 5 members for all libdecor versions from 0.1.0 to 0.2.2;
	int ref_count;
	struct libdecor* context;
	struct wl_surface* wl_surface;
	const struct libdecor_frame_interface* iface;
	void* user_data;
	// all following members after user_data omitted
};
struct libdecor_frame_workaround {  // taken from libdecor-plugin.h to workaround missing libdecor_frame_set_user_data();
	// for more details, see comment in libdecor_frame_private_workaround structure;
	// libdecor_frame stays the same for libdecor 0.1.0 to 0.2.2, e.g. in all libdecor releases as of today (2025-02-27)
	struct libdecor_frame_private* priv;
	struct wl_list link;
};
#endif


using namespace std;



class VulkanWindowPrivate : public VulkanWindow {
public:
#if defined(USE_PLATFORM_WIN32)
	static LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
#elif defined(USE_PLATFORM_WAYLAND)
	static void registryListenerGlobal(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
	static void registryListenerGlobalRemove(void*, wl_registry*, uint32_t);
	static void xdgWmBaseListenerPing(void*, xdg_wm_base* xdg, uint32_t serial);
	static void xdgSurfaceListenerConfigure(void* data, xdg_surface* xdgSurface, uint32_t serial);
	static void xdgToplevelListenerConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array*);
	static void xdgToplevelListenerClose(void* data, xdg_toplevel* xdgTopLevel);
	static void libdecorError(libdecor* context, libdecor_error error, const char* message);
	static void libdecorFrameConfigure(libdecor_frame* frame, libdecor_configuration* config, void* data);
	static void libdecorFrameClose(libdecor_frame* frame, void* data);
	static void libdecorFrameCommit(libdecor_frame* frame, void* data);
	static void libdecorFrameDismissPopup(libdecor_frame* frame, const char* seatName, void* data);
	static void frameListenerDone(void *data, wl_callback* cb, uint32_t time);
	static void syncListenerDone(void *data, wl_callback* cb, uint32_t time);
	static void seatListenerCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
	static void pointerListenerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface,
	                                 wl_fixed_t surface_x, wl_fixed_t surface_y);
	static void pointerListenerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
	static void pointerListenerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
	static void pointerListenerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	static void pointerListenerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
	static void keyboardListenerKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size);
	static void keyboardListenerEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
	static void keyboardListenerLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
	static void keyboardListenerKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t scanCode, uint32_t state);
	static void keyboardListenerModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed,
	                                      uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
#endif
};



//
// global variables for various platforms
//
#if defined(USE_PLATFORM_WIN32)

struct win32 {

	// general win32 global variables
	static inline void* hInstance = 0;  // void* is used instead of HINSTANCE type to avoid #include <windows.h>
	static inline uint16_t windowClass = 0;  // uint16_t is used instead of ATOM type to avoid #include <windows.h>
	static inline const vector<const char*> requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_win32_surface" };

	// list of windows waiting for frame rendering
	// (the windows have _framePendingState set to FramePendingState::Pending or TentativePending)
	static inline vector<VulkanWindow*> framePendingWindows;

	// scan code to key conversion table
	// (the table is updated upon each keyboard layout change)
	static inline VulkanWindow::KeyCode keyConversionTable[128];

};

// required instance extensions functions
const std::vector<const char*>& VulkanWindow::requiredExtensions()  { return win32::requiredInstanceExtensions; }
std::vector<const char*>& VulkanWindow::appendRequiredExtensions(std::vector<const char*>& v)  { v.insert(v.end(), win32::requiredInstanceExtensions.begin(), win32::requiredInstanceExtensions.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(win32::requiredInstanceExtensions.size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return win32::requiredInstanceExtensions.data(); }

#elif defined(USE_PLATFORM_XLIB)

struct xlib {

	// xlib global variables
	static inline struct _XDisplay* display = nullptr;  // struct _XDisplay* is used instead of Display* type
	static inline bool externalDisplayHandle;
	static inline map<Window, VulkanWindow*> vulkanWindowMap;
	static inline bool running;  // bool indicating that application is running and it shall not leave main loop
	static inline unsigned long wmDeleteMessage;  // unsigned long is used for Atom type
	static inline unsigned long wmStateProperty;  // unsigned long is used for Atom type
	static inline unsigned long netWmName;  // unsigned long is used for Atom type
	static inline unsigned long utf8String;  // unsigned long is used for Atom type
	static inline const vector<const char*> requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_xlib_surface" };

};

// required instance extensions functions
const std::vector<const char*>& VulkanWindow::requiredExtensions()  { return xlib::requiredInstanceExtensions; }
std::vector<const char*>& VulkanWindow::appendRequiredExtensions(std::vector<const char*>& v)  { v.insert(v.end(), xlib::requiredInstanceExtensions.begin(), xlib::requiredInstanceExtensions.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(xlib::requiredInstanceExtensions.size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return xlib::requiredInstanceExtensions.data(); }

#elif defined(USE_PLATFORM_WAYLAND)

struct wayland {

	// Wayland global variables
	static inline struct wl_display* display = nullptr;
	static inline bool externalDisplayHandle;
	static inline struct wl_registry* registry;
	static inline struct wl_compositor* compositor = nullptr;
	static inline struct xdg_wm_base* xdgWmBase = nullptr;
	static inline struct zxdg_decoration_manager_v1* zxdgDecorationManagerV1 = nullptr;
	static inline struct libdecor* libdecorContext = nullptr;
	static inline struct wl_shm* shm = nullptr;
	static inline struct wl_cursor_theme* cursorTheme = nullptr;
	static inline struct wl_surface* cursorSurface = nullptr;
	static inline int cursorHotspotX;
	static inline int cursorHotspotY;
	static inline struct wl_seat* seat = nullptr;
	static inline struct wl_pointer* pointer = nullptr;
	static inline struct wl_keyboard* keyboard = nullptr;
	static inline struct xkb_context* xkbContext = nullptr;
	static inline struct xkb_state* xkbState = nullptr;
	static inline std::bitset<16> modifiers;
	static inline bool running;  // bool indicating that application is running and it shall not leave main loop
	static inline VulkanWindowPrivate* windowUnderPointer = nullptr;
	static inline VulkanWindowPrivate* windowWithKbFocus = nullptr;
	static inline const char* vulkanWindowTag = "VulkanWindow";
	static inline const vector<const char*> requiredInstanceExtensions =
		{ "VK_KHR_surface", "VK_KHR_wayland_surface" };

	// libdecor functions (dynamically linked)
	struct Funcs {
		struct libdecor* (*libdecor_new)(struct wl_display* display, const struct libdecor_interface* iface);
		void (*libdecor_unref)(struct libdecor* context);
		void (*libdecor_frame_unref)(struct libdecor_frame* frame);
		void (*libdecor_frame_set_user_data)(struct libdecor_frame* frame, void* user_data);
		struct libdecor_frame* (*libdecor_decorate)(struct libdecor* context, struct wl_surface* surface,
			const struct libdecor_frame_interface* iface, void* user_data);
		void (*libdecor_frame_set_title)(struct libdecor_frame* frame, const char* title);
		void (*libdecor_frame_set_minimized)(struct libdecor_frame* frame);
		void (*libdecor_frame_set_maximized)(struct libdecor_frame* frame);
		void (*libdecor_frame_unset_maximized)(struct libdecor_frame* frame);
		void (*libdecor_frame_set_fullscreen)(struct libdecor_frame *frame, struct wl_output *output);
		void (*libdecor_frame_unset_fullscreen)(struct libdecor_frame *frame);
		void (*libdecor_frame_map)(struct libdecor_frame* frame);
		bool (*libdecor_configuration_get_window_state)(struct libdecor_configuration* configuration,
			enum libdecor_window_state* window_state);
		bool (*libdecor_configuration_get_content_size)(struct libdecor_configuration* configuration,
			struct libdecor_frame* frame, int* width, int* height);
		struct libdecor_state* (*libdecor_state_new)(int width, int height);
		void (*libdecor_frame_commit)(struct libdecor_frame* frame,
			struct libdecor_state* state, struct libdecor_configuration* configuration);
		void (*libdecor_state_free)(struct libdecor_state* state);
		int (*libdecor_dispatch)(struct libdecor* context, int timeout);
	};
	static inline Funcs funcs;
	static inline void* libdecorHandle = nullptr;

};

// required instance extensions functions
const std::vector<const char*>& VulkanWindow::requiredExtensions()  { return wayland::requiredInstanceExtensions; }
std::vector<const char*>& VulkanWindow::appendRequiredExtensions(std::vector<const char*>& v)  { v.insert(v.end(), wayland::requiredInstanceExtensions.begin(), wayland::requiredInstanceExtensions.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(wayland::requiredInstanceExtensions.size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return wayland::requiredInstanceExtensions.data(); }

#elif defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2)

struct sdl {

	// SDL global variables
	static inline bool initialized = false;
	static inline bool running;
	static inline constexpr const char* windowPointerName = "VulkanWindow";
#if defined(USE_PLATFORM_SDL3)
	static inline vector<const char*> requiredInstanceExtensions;
#endif

};

#elif defined(USE_PLATFORM_GLFW)

struct glfw {

	// bool indicating that application is running and it shall not leave main loop
	static inline bool running;

	// list of windows waiting for frame rendering
	// (the windows have _framePendingState set to FramePendingState::Pending or TentativePending)
	static inline vector<VulkanWindow*> framePendingWindows;

};

#elif defined(USE_PLATFORM_QT)

// QtRenderingWindow is customized QWindow class for Vulkan rendering
class QtRenderingWindow : public QWindow {
public:
	VulkanWindow* vulkanWindow;
	int timer = 0;
	QtRenderingWindow(QWindow* parent, VulkanWindow* vulkanWindow_) : QWindow(parent), vulkanWindow(vulkanWindow_)  {}
	bool event(QEvent* event) override;
	void scheduleFrameTimer();
};

struct qt {

	// Qt global variables
	static inline aligned_storage<sizeof(QGuiApplication), alignof(QGuiApplication)>::type qGuiApplicationMemory;
	static inline QGuiApplication* qGuiApplication = nullptr;
	static inline aligned_storage<sizeof(QVulkanInstance), alignof(QVulkanInstance)>::type qVulkanInstanceMemory;
	static inline QVulkanInstance* qVulkanInstance = nullptr;

# if !defined(_WIN32)
	// alternative command line arguments
	// (if the user does not use VulkanWindow::init(argc, argv),
	// we get command line arguments by various API functions)
	static inline vector<char> altArgBuffer;
	static inline vector<char*> altArgv;
	static inline int altArgc;
# endif

};

#endif



//
// global functions for various platforms
//
#if defined(USE_PLATFORM_WIN32)

// Win32 UTF-8 string to wstring conversion
static wstring utf8toWString(const string& s)
{
	// get string lengths
	int l1 = int(s.length());  // length() returns number of bytes of the string but number of characters might be lower because it is utf8 string
	if(l1 == 0)  return {};
	l1++;  // include null terminating character
	int l2 = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), l1, NULL, 0);
	if(l2 == 0)
		throw runtime_error("MultiByteToWideChar(): The function failed.");

	// perform the conversion
	wstring r(l2, '\0'); // resize the string and initialize it with zeros because we have no simple way to leave it unitialized
	if(MultiByteToWideChar(CP_UTF8, 0, s.c_str(), l1, r.data(), l2) == 0)
		throw runtime_error("MultiByteToWideChar(): The function failed.");
	return r;
}

// Win32 wchar_t (UTF-16LE) to Unicode character number (code point) conversion
static VulkanWindow::KeyCode wchar16ToKeyCode(WCHAR wch16)
{
	// make sure that we are not dealing with two WCHARs (surrogate pair)
	if((wch16 & 0xf800) == 0xd800)
		return VulkanWindow::KeyCode(0xfffd);  // return "replacement character" indicating the error

	// Win32 uses UTF-16LE (little endian) that can be converted directly to KeyCode
	return VulkanWindow::KeyCode(wch16);
};

// remove VulkanWindow from framePendingWindows; VulkanWindow MUST be in framePendingWindows
static void removeFromFramePendingWindows(VulkanWindow* w)
{
	if(win32::framePendingWindows.size() != 1) {
		for(size_t i=0; i<win32::framePendingWindows.size(); i++)
			if(win32::framePendingWindows[i] == w) {
				win32::framePendingWindows[i] = win32::framePendingWindows.back();
				break;
			}
	}
	win32::framePendingWindows.pop_back();
}

static VulkanWindow::ScanCode getScanCodeOfSpecialKey(WPARAM wParam)
{
	switch(wParam) {
	case VK_VOLUME_MUTE: return VulkanWindow::ScanCode::Mute;
	case VK_VOLUME_DOWN: return VulkanWindow::ScanCode::VolumeDown;
	case VK_VOLUME_UP:   return VulkanWindow::ScanCode::VolumeUp;
	case VK_MEDIA_NEXT_TRACK: return VulkanWindow::ScanCode::MediaNext;
	case VK_MEDIA_PLAY_PAUSE: return VulkanWindow::ScanCode::MediaPlayPause;
	case VK_MEDIA_PREV_TRACK: return VulkanWindow::ScanCode::MediaPrev;
	case VK_MEDIA_STOP:       return VulkanWindow::ScanCode::MediaStop;
	case VK_BROWSER_SEARCH: return VulkanWindow::ScanCode::Search;
	case VK_BROWSER_HOME: return VulkanWindow::ScanCode::BrowserHome;
	case VK_LAUNCH_MAIL: return VulkanWindow::ScanCode::Mail;
	case VK_LAUNCH_MEDIA_SELECT: return VulkanWindow::ScanCode::MediaSelect;
	case VK_LAUNCH_APP2: return VulkanWindow::ScanCode::Calculator;
	default: return VulkanWindow::ScanCode::Unknown;
	}
}

static void initKeyConversionTable()
{
	for(uint8_t scanCode=0; scanCode<128; scanCode++)
	{
		// get scan code
		int vk = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK);
		if(vk == 0) {
			win32::keyConversionTable[scanCode] = VulkanWindow::KeyCode::Unknown;
			continue;
		}

		// get virtual code
		int wch16 = (MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) & 0xffff);
		if(wch16 == 0) {
			win32::keyConversionTable[scanCode] = VulkanWindow::KeyCode::Unknown;
			continue;
		}

		// convert WCHAR (=UTF-16 on Win32) to UTF-8
		VulkanWindow::KeyCode key = wchar16ToKeyCode(wch16);

		// normalize case
		// (convert A..Z into a..z)
		using UnderlyingType = underlying_type<VulkanWindow::KeyCode>::type;
		if(key >= VulkanWindow::KeyCode('A') && key <= VulkanWindow::KeyCode('Z'))
			key = VulkanWindow::KeyCode(UnderlyingType(key) + 32);

		// update table
		win32::keyConversionTable[scanCode] = key;
	}
}

#elif defined(USE_PLATFORM_WAYLAND)

// listeners
static const wl_registry_listener registryListener{
	VulkanWindowPrivate::registryListenerGlobal,
	VulkanWindowPrivate::registryListenerGlobalRemove,
};
static const xdg_wm_base_listener xdgWmBaseListener{
	VulkanWindowPrivate::xdgWmBaseListenerPing,
};
static const xdg_surface_listener xdgSurfaceListener{
	VulkanWindowPrivate::xdgSurfaceListenerConfigure,
};
static const xdg_toplevel_listener xdgToplevelListener{
	VulkanWindowPrivate::xdgToplevelListenerConfigure,
	VulkanWindowPrivate::xdgToplevelListenerClose,
};
static libdecor_interface libdecorInterface{
	VulkanWindowPrivate::libdecorError,
};
static libdecor_frame_interface libdecorFrameInterface{
	VulkanWindowPrivate::libdecorFrameConfigure,
	VulkanWindowPrivate::libdecorFrameClose,
	VulkanWindowPrivate::libdecorFrameCommit,
	VulkanWindowPrivate::libdecorFrameDismissPopup,
};
static const wl_callback_listener frameListener{
	VulkanWindowPrivate::frameListenerDone,
};
static const wl_callback_listener syncListener{
	VulkanWindowPrivate::syncListenerDone,
};
static const wl_seat_listener seatListener{
	VulkanWindowPrivate::seatListenerCapabilities,
};
static const wl_pointer_listener pointerListener{
	VulkanWindowPrivate::pointerListenerEnter,
	VulkanWindowPrivate::pointerListenerLeave,
	VulkanWindowPrivate::pointerListenerMotion,
	VulkanWindowPrivate::pointerListenerButton,
	VulkanWindowPrivate::pointerListenerAxis,
};
static const wl_keyboard_listener keyboardListener{
	VulkanWindowPrivate::keyboardListenerKeymap,
	VulkanWindowPrivate::keyboardListenerEnter,
	VulkanWindowPrivate::keyboardListenerLeave,
	VulkanWindowPrivate::keyboardListenerKey,
	VulkanWindowPrivate::keyboardListenerModifiers,
};

// registry global object notification
void VulkanWindowPrivate::registryListenerGlobal(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
	if(strcmp(interface, wl_compositor_interface.name) == 0)
		wayland::compositor = static_cast<wl_compositor*>(
			wl_registry_bind(registry, name, &wl_compositor_interface, 1));
	else if(strcmp(interface, xdg_wm_base_interface.name) == 0)
		wayland::xdgWmBase = static_cast<xdg_wm_base*>(
			wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
	else if(strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
		wayland::zxdgDecorationManagerV1 = static_cast<zxdg_decoration_manager_v1*>(
			wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
	else if(strcmp(interface, wl_seat_interface.name) == 0)
		wayland::seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
	else if(strcmp(interface, wl_shm_interface.name) == 0)
		wayland::shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
}

// registry global object removal notification
void VulkanWindowPrivate::registryListenerGlobalRemove(void*, wl_registry*, uint32_t)
{
}

// ping-pong message
void VulkanWindowPrivate::xdgWmBaseListenerPing(void*, xdg_wm_base* xdg, uint32_t serial)
{
	xdg_wm_base_pong(xdg, serial);
};

// make sure all window state changes were processed by Wayland server
// by waiting on all sync events
static void waitAllSyncEvents(unsigned& numSyncEventsOnTheFly)
{
	// if numSyncEventsOnTheFly is not zero, dispatch Wayland events until it becomes zero
	while(numSyncEventsOnTheFly != 0)
		if(wl_display_dispatch(wayland::display) == -1)  // it blocks if there are no events
			throw runtime_error("wl_display_dispatch() failed.");
}

#elif defined(USE_PLATFORM_GLFW)

// GLFW error handling
static void throwError(const string& funcName)
{
	const char* errorString;
	int errorCode = glfwGetError(&errorString);
	throw runtime_error(string("VulkanWindow: ") + funcName + "() function failed. Error code: " +
	                    to_string(errorCode) + ". Error string: " + errorString);
}
static void checkError(const string& funcName)
{
	const char* errorString;
	int errorCode = glfwGetError(&errorString);
	if(errorCode != GLFW_NO_ERROR)
		throw runtime_error(string("VulkanWindow: ") + funcName + "() function failed. Error code: " +
		                    to_string(errorCode) + ". Error string: " + errorString);
}

static bool canUpdateSavedGeometry(GLFWwindow* w)
{
	if(glfwGetWindowAttrib(w, GLFW_MAXIMIZED))  return false;
	if(glfwGetWindowAttrib(w, GLFW_ICONIFIED))  return false;
	if(glfwGetWindowAttrib(w, GLFW_VISIBLE) == GLFW_FALSE)  return false;
	if(glfwGetWindowMonitor(w) != nullptr)  return false;
	return true;
};

#endif



#if defined(USE_PLATFORM_WIN32) || (defined(USE_PLATFORM_GLFW) && defined(_WIN32)) || (defined(USE_PLATFORM_QT) && defined(_WIN32))

static const VulkanWindow::ScanCode scanCodeConversionTable[512] = {
	// normal keys:
	// (changes compared to values that is carried by each VulkanWindow::ScanCode enum item: NumLock->PauseBreak)
	/*0-1*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Escape,
	/*2-6*/ VulkanWindow::ScanCode::One, VulkanWindow::ScanCode::Two, VulkanWindow::ScanCode::Three, VulkanWindow::ScanCode::Four, VulkanWindow::ScanCode::Five,
	/*7-11*/ VulkanWindow::ScanCode::Six, VulkanWindow::ScanCode::Seven, VulkanWindow::ScanCode::Eight, VulkanWindow::ScanCode::Nine, VulkanWindow::ScanCode::Zero,
	/*12-15*/ VulkanWindow::ScanCode::Minus, VulkanWindow::ScanCode::Equal, VulkanWindow::ScanCode::Backspace, VulkanWindow::ScanCode::Tab,
	/*16-22*/ VulkanWindow::ScanCode::Q, VulkanWindow::ScanCode::W, VulkanWindow::ScanCode::E, VulkanWindow::ScanCode::R, VulkanWindow::ScanCode::T, VulkanWindow::ScanCode::Y, VulkanWindow::ScanCode::U,
	/*23-28*/ VulkanWindow::ScanCode::I, VulkanWindow::ScanCode::O, VulkanWindow::ScanCode::P, VulkanWindow::ScanCode::LeftBracket, VulkanWindow::ScanCode::RightBracket, VulkanWindow::ScanCode::Enter,
	/*29-34*/ VulkanWindow::ScanCode::LeftControl, VulkanWindow::ScanCode::A, VulkanWindow::ScanCode::S, VulkanWindow::ScanCode::D, VulkanWindow::ScanCode::F, VulkanWindow::ScanCode::G,
	/*35-40*/ VulkanWindow::ScanCode::H, VulkanWindow::ScanCode::J, VulkanWindow::ScanCode::K, VulkanWindow::ScanCode::L, VulkanWindow::ScanCode::Semicolon, VulkanWindow::ScanCode::Apostrophe,
	/*41-47*/ VulkanWindow::ScanCode::GraveAccent, VulkanWindow::ScanCode::LeftShift, VulkanWindow::ScanCode::Backslash, VulkanWindow::ScanCode::Z, VulkanWindow::ScanCode::X, VulkanWindow::ScanCode::C, VulkanWindow::ScanCode::V,
	/*48-54*/ VulkanWindow::ScanCode::B, VulkanWindow::ScanCode::N, VulkanWindow::ScanCode::M, VulkanWindow::ScanCode::Comma, VulkanWindow::ScanCode::Period, VulkanWindow::ScanCode::Slash, VulkanWindow::ScanCode::RightShift,
	/*55-58*/ VulkanWindow::ScanCode::KeypadMultiply, VulkanWindow::ScanCode::LeftAlt, VulkanWindow::ScanCode::Space, VulkanWindow::ScanCode::CapsLock,
	/*59-64*/ VulkanWindow::ScanCode::F1, VulkanWindow::ScanCode::F2, VulkanWindow::ScanCode::F3, VulkanWindow::ScanCode::F4, VulkanWindow::ScanCode::F5, VulkanWindow::ScanCode::F6,
	/*65-70*/ VulkanWindow::ScanCode::F7, VulkanWindow::ScanCode::F8, VulkanWindow::ScanCode::F9, VulkanWindow::ScanCode::F10, VulkanWindow::ScanCode::PauseBreak, VulkanWindow::ScanCode::ScrollLock,
	/*71-77*/ VulkanWindow::ScanCode::Keypad7, VulkanWindow::ScanCode::Keypad8, VulkanWindow::ScanCode::Keypad9, VulkanWindow::ScanCode::KeypadMinus, VulkanWindow::ScanCode::Keypad4, VulkanWindow::ScanCode::Keypad5, VulkanWindow::ScanCode::Keypad6,
	/*78-83*/ VulkanWindow::ScanCode::KeypadPlus, VulkanWindow::ScanCode::Keypad1, VulkanWindow::ScanCode::Keypad2, VulkanWindow::ScanCode::Keypad3, VulkanWindow::ScanCode::Keypad0, VulkanWindow::ScanCode::KeypadPeriod,
	/*84-88*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::NonUSBackslash, VulkanWindow::ScanCode::F11, VulkanWindow::ScanCode::F12,
	/*89-95*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*96-101*/ VulkanWindow::ScanCode::KeypadEnter, VulkanWindow::ScanCode::RightControl, VulkanWindow::ScanCode::KeypadDivide, VulkanWindow::ScanCode::PrintScreen, VulkanWindow::ScanCode::RightAlt, VulkanWindow::ScanCode::Unknown,
	/*102-108*/ VulkanWindow::ScanCode::Home, VulkanWindow::ScanCode::Up, VulkanWindow::ScanCode::PageUp, VulkanWindow::ScanCode::Left, VulkanWindow::ScanCode::Right, VulkanWindow::ScanCode::End, VulkanWindow::ScanCode::Down,
	/*109-112*/ VulkanWindow::ScanCode::PageDown, VulkanWindow::ScanCode::Insert, VulkanWindow::ScanCode::Delete, VulkanWindow::ScanCode::Unknown,
	/*113-118*/ VulkanWindow::ScanCode::Mute, VulkanWindow::ScanCode::VolumeDown, VulkanWindow::ScanCode::VolumeUp, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*119-124*/ VulkanWindow::ScanCode::PauseBreak, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*125-127*/ VulkanWindow::ScanCode::LeftGUI, VulkanWindow::ScanCode::RightGUI, VulkanWindow::ScanCode::Application,

	/*128-130*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*131-135*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*136-140*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*141-145*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*146-150*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*151-155*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*156-160*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*161-165*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*166-170*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*171-175*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*176-180*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*181-185*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*186-190*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*191-195*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*196-200*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*201-205*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*206-210*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*211-215*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*216-220*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*221-225*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*226-230*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*231-235*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*236-240*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*241-245*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*246-250*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*251-255*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,

	// extended keys:
	// (changes compared to normal keys: Enter->KeypadEnter, LeftControl->RightControl, Slash->KeypadDivide, KeypadMultiply->PrintScreen, LeftAlt->RightAlt,
	// Keypad7->Home, Keypad8->Up, Keypad9->PageUp, Keypad4->Left, Keypad6->Right, Keypad1->End, Keypad2->Down, Keypad3->PageDown, Keypad0->Insert, KeypadPeriod->Delete,
	// 91->LeftGUI, 92->RightGUI, 93->Application)
	/*0-1*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Escape,
	/*2-6*/ VulkanWindow::ScanCode::One, VulkanWindow::ScanCode::Two, VulkanWindow::ScanCode::Three, VulkanWindow::ScanCode::Four, VulkanWindow::ScanCode::Five,
	/*7-11*/ VulkanWindow::ScanCode::Six, VulkanWindow::ScanCode::Seven, VulkanWindow::ScanCode::Eight, VulkanWindow::ScanCode::Nine, VulkanWindow::ScanCode::Zero,
	/*12-15*/ VulkanWindow::ScanCode::Minus, VulkanWindow::ScanCode::Equal, VulkanWindow::ScanCode::Backspace, VulkanWindow::ScanCode::Tab,
	/*16-22*/ VulkanWindow::ScanCode::Q, VulkanWindow::ScanCode::W, VulkanWindow::ScanCode::E, VulkanWindow::ScanCode::R, VulkanWindow::ScanCode::T, VulkanWindow::ScanCode::Y, VulkanWindow::ScanCode::U,
	/*23-28*/ VulkanWindow::ScanCode::I, VulkanWindow::ScanCode::O, VulkanWindow::ScanCode::P, VulkanWindow::ScanCode::LeftBracket, VulkanWindow::ScanCode::RightBracket, VulkanWindow::ScanCode::KeypadEnter,
	/*29-34*/ VulkanWindow::ScanCode::RightControl, VulkanWindow::ScanCode::A, VulkanWindow::ScanCode::S, VulkanWindow::ScanCode::D, VulkanWindow::ScanCode::F, VulkanWindow::ScanCode::G,
	/*35-40*/ VulkanWindow::ScanCode::H, VulkanWindow::ScanCode::J, VulkanWindow::ScanCode::K, VulkanWindow::ScanCode::L, VulkanWindow::ScanCode::Semicolon, VulkanWindow::ScanCode::Apostrophe,
	/*41-47*/ VulkanWindow::ScanCode::GraveAccent, VulkanWindow::ScanCode::LeftShift, VulkanWindow::ScanCode::Backslash, VulkanWindow::ScanCode::Z, VulkanWindow::ScanCode::X, VulkanWindow::ScanCode::C, VulkanWindow::ScanCode::V,
	/*48-54*/ VulkanWindow::ScanCode::B, VulkanWindow::ScanCode::N, VulkanWindow::ScanCode::M, VulkanWindow::ScanCode::Comma, VulkanWindow::ScanCode::Period, VulkanWindow::ScanCode::KeypadDivide, VulkanWindow::ScanCode::RightShift,
	/*55-56*/ VulkanWindow::ScanCode::PrintScreen, VulkanWindow::ScanCode::RightAlt /* RightAlt might be configured as AltGr. In that case, LeftControl press is generated by RightAlt as well. To change AltGr to Alt, switch to US-English keyboard layout. */,
	/*57-58*/ VulkanWindow::ScanCode::Space, VulkanWindow::ScanCode::CapsLock,
	/*59-64*/ VulkanWindow::ScanCode::F1, VulkanWindow::ScanCode::F2, VulkanWindow::ScanCode::F3, VulkanWindow::ScanCode::F4, VulkanWindow::ScanCode::F5, VulkanWindow::ScanCode::F6,
	/*65-70*/ VulkanWindow::ScanCode::F7, VulkanWindow::ScanCode::F8, VulkanWindow::ScanCode::F9, VulkanWindow::ScanCode::F10, VulkanWindow::ScanCode::NumLock, VulkanWindow::ScanCode::ScrollLock,
	/*71-77*/ VulkanWindow::ScanCode::Home, VulkanWindow::ScanCode::Up, VulkanWindow::ScanCode::PageUp, VulkanWindow::ScanCode::KeypadMinus, VulkanWindow::ScanCode::Left, VulkanWindow::ScanCode::Keypad5, VulkanWindow::ScanCode::Right,
	/*78-83*/ VulkanWindow::ScanCode::KeypadPlus, VulkanWindow::ScanCode::End, VulkanWindow::ScanCode::Down, VulkanWindow::ScanCode::PageDown, VulkanWindow::ScanCode::Insert, VulkanWindow::ScanCode::Delete,
	/*84-88*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::NonUSBackslash, VulkanWindow::ScanCode::F11, VulkanWindow::ScanCode::F12,
	/*89-95*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::LeftGUI, VulkanWindow::ScanCode::RightGUI, VulkanWindow::ScanCode::Application, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*96-101*/ VulkanWindow::ScanCode::KeypadEnter, VulkanWindow::ScanCode::RightControl, VulkanWindow::ScanCode::KeypadDivide, VulkanWindow::ScanCode::PrintScreen, VulkanWindow::ScanCode::RightAlt, VulkanWindow::ScanCode::Unknown,
	/*102-108*/ VulkanWindow::ScanCode::Home, VulkanWindow::ScanCode::Up, VulkanWindow::ScanCode::PageUp, VulkanWindow::ScanCode::Left, VulkanWindow::ScanCode::Right, VulkanWindow::ScanCode::End, VulkanWindow::ScanCode::Down,
	/*109-112*/ VulkanWindow::ScanCode::PageDown, VulkanWindow::ScanCode::Insert, VulkanWindow::ScanCode::Delete, VulkanWindow::ScanCode::Unknown,
	/*113-118*/ VulkanWindow::ScanCode::Mute, VulkanWindow::ScanCode::VolumeDown, VulkanWindow::ScanCode::VolumeUp, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*119-124*/ VulkanWindow::ScanCode::PauseBreak, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*125-127*/ VulkanWindow::ScanCode::LeftGUI, VulkanWindow::ScanCode::RightGUI, VulkanWindow::ScanCode::Application,

	/*128-130*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*131-135*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*136-140*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*141-145*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*146-150*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*151-155*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*156-160*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*161-165*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*166-170*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*171-175*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*176-180*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*181-185*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*186-190*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*191-195*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*196-200*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*201-205*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*206-210*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*211-215*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*216-220*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*221-225*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*226-230*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*231-235*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*236-240*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*241-245*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*246-250*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*251-255*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
};

/** Translates native scan code into VulkanWindow::ScanCode.
 *  The nativeScanCode is expected to be within 0..511 range.
 *  The values outside the range are handled by returning VulkanWindow::ScanCode::Unknown. */
static VulkanWindow::ScanCode translateScanCode(int nativeScanCode)
{
	if(nativeScanCode <= 0 || nativeScanCode == 0x100)
		return VulkanWindow::ScanCode::Unknown;
	if(nativeScanCode >= sizeof(scanCodeConversionTable)/sizeof(VulkanWindow::ScanCode))
		return VulkanWindow::ScanCode::Unknown;
	VulkanWindow::ScanCode scanCode = scanCodeConversionTable[nativeScanCode];
	if(scanCode == VulkanWindow::ScanCode::Unknown)
		return VulkanWindow::ScanCode(1000 + nativeScanCode);
	return scanCode;
}

#endif


#if defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2)
#if defined(USE_PLATFORM_SDL3)
static const VulkanWindow::ScanCode scanCodeConversionTable[SDL_SCANCODE_COUNT] = {
#else
static const VulkanWindow::ScanCode scanCodeConversionTable[SDL_NUM_SCANCODES] = {
#endif
	/*0..3*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*4..10*/ VulkanWindow::ScanCode::A, VulkanWindow::ScanCode::B, VulkanWindow::ScanCode::C, VulkanWindow::ScanCode::D, VulkanWindow::ScanCode::E, VulkanWindow::ScanCode::F, VulkanWindow::ScanCode::G,
	/*11..17*/ VulkanWindow::ScanCode::H, VulkanWindow::ScanCode::I, VulkanWindow::ScanCode::J, VulkanWindow::ScanCode::K, VulkanWindow::ScanCode::L, VulkanWindow::ScanCode::M, VulkanWindow::ScanCode::N,
	/*18..24*/ VulkanWindow::ScanCode::O, VulkanWindow::ScanCode::P, VulkanWindow::ScanCode::Q, VulkanWindow::ScanCode::R, VulkanWindow::ScanCode::S, VulkanWindow::ScanCode::T, VulkanWindow::ScanCode::U,
	/*25..29*/ VulkanWindow::ScanCode::V, VulkanWindow::ScanCode::W, VulkanWindow::ScanCode::X, VulkanWindow::ScanCode::Y, VulkanWindow::ScanCode::Z,
	/*30..34*/ VulkanWindow::ScanCode::One, VulkanWindow::ScanCode::Two, VulkanWindow::ScanCode::Three, VulkanWindow::ScanCode::Four, VulkanWindow::ScanCode::Five,
	/*35..39*/ VulkanWindow::ScanCode::Six, VulkanWindow::ScanCode::Seven, VulkanWindow::ScanCode::Eight, VulkanWindow::ScanCode::Nine, VulkanWindow::ScanCode::Zero,
	/*40..44*/ VulkanWindow::ScanCode::Return, VulkanWindow::ScanCode::Escape, VulkanWindow::ScanCode::Backspace, VulkanWindow::ScanCode::Tab, VulkanWindow::ScanCode::Space,
	/*45..48*/ VulkanWindow::ScanCode::Minus, VulkanWindow::ScanCode::Equal, VulkanWindow::ScanCode::LeftBracket, VulkanWindow::ScanCode::RightBracket,
	/*49..52*/ VulkanWindow::ScanCode::Backslash, VulkanWindow::ScanCode::NonUSBackslash, VulkanWindow::ScanCode::Semicolon, VulkanWindow::ScanCode::Apostrophe,
	/*53..57*/ VulkanWindow::ScanCode::GraveAccent, VulkanWindow::ScanCode::Comma, VulkanWindow::ScanCode::Period, VulkanWindow::ScanCode::Slash, VulkanWindow::ScanCode::CapsLock,
	/*58..63*/ VulkanWindow::ScanCode::F1, VulkanWindow::ScanCode::F2, VulkanWindow::ScanCode::F3, VulkanWindow::ScanCode::F4, VulkanWindow::ScanCode::F5, VulkanWindow::ScanCode::F6,
	/*64..69*/ VulkanWindow::ScanCode::F7, VulkanWindow::ScanCode::F8, VulkanWindow::ScanCode::F9, VulkanWindow::ScanCode::F10, VulkanWindow::ScanCode::F11, VulkanWindow::ScanCode::F12,
	/*70..73*/ VulkanWindow::ScanCode::PrintScreen, VulkanWindow::ScanCode::ScrollLock, VulkanWindow::ScanCode::PauseBreak, VulkanWindow::ScanCode::Insert,
	/*74..78*/ VulkanWindow::ScanCode::Home, VulkanWindow::ScanCode::PageUp, VulkanWindow::ScanCode::Delete, VulkanWindow::ScanCode::End, VulkanWindow::ScanCode::PageDown,
	/*79..83*/ VulkanWindow::ScanCode::Right, VulkanWindow::ScanCode::Left, VulkanWindow::ScanCode::Down, VulkanWindow::ScanCode::Up, VulkanWindow::ScanCode::NumLockClear,
	/*84..88*/ VulkanWindow::ScanCode::KeypadDivide, VulkanWindow::ScanCode::KeypadMultiply, VulkanWindow::ScanCode::KeypadMinus, VulkanWindow::ScanCode::KeypadPlus, VulkanWindow::ScanCode::KeypadEnter,
	/*89..94*/ VulkanWindow::ScanCode::Keypad1, VulkanWindow::ScanCode::Keypad2, VulkanWindow::ScanCode::Keypad3, VulkanWindow::ScanCode::Keypad4, VulkanWindow::ScanCode::Keypad5, VulkanWindow::ScanCode::Keypad6,
	/*95..99*/ VulkanWindow::ScanCode::Keypad7, VulkanWindow::ScanCode::Keypad8, VulkanWindow::ScanCode::Keypad9, VulkanWindow::ScanCode::Keypad0, VulkanWindow::ScanCode::KeypadPeriod,
	/*100..104*/ VulkanWindow::ScanCode::NonUSBackslash, VulkanWindow::ScanCode::Application, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::MediaSelect,
	/*105..109*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*110..114*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*115..119*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*120..124*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*125..129*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Mute, VulkanWindow::ScanCode::VolumeUp, VulkanWindow::ScanCode::VolumeDown,
	/*130..134*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*135..139*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*140..144*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*145..149*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*150..154*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*155..159*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*160..164*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*165..169*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*170..174*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*175..179*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*180..184*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*185..189*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*190..194*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*195..199*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*200..204*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*205..209*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*210..214*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*215..219*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*220..224*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::LeftControl,
	/*225..229*/ VulkanWindow::ScanCode::LeftShift, VulkanWindow::ScanCode::LeftAlt, VulkanWindow::ScanCode::LeftGUI, VulkanWindow::ScanCode::RightControl, VulkanWindow::ScanCode::RightShift,
	/*230..234*/ VulkanWindow::ScanCode::RightAlt, VulkanWindow::ScanCode::RightGUI, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*235..239*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*240..244*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*245..249*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*250..254*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*255..259*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::MediaNext, VulkanWindow::ScanCode::MediaPrev,
	/*260..264*/ VulkanWindow::ScanCode::MediaStop, VulkanWindow::ScanCode::MediaPlayPause, VulkanWindow::ScanCode::Mute, VulkanWindow::ScanCode::MediaSelect, VulkanWindow::ScanCode::Unknown,
	/*265..269*/ VulkanWindow::ScanCode::Mail, VulkanWindow::ScanCode::Calculator, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Search, VulkanWindow::ScanCode::BrowserHome,
	/*270..274*/ VulkanWindow::ScanCode::Back, VulkanWindow::ScanCode::Forward, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*275..279*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
	/*280..284*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Calculator,
	/*285..289*/ VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown, VulkanWindow::ScanCode::Unknown,
};

/** Translates SDL scan code into VulkanWindow::ScanCode.
 *  The sdlScanCode is expected to be within 0..511 range.
 *  The values outside the range are handled by returning VulkanWindow::ScanCode::Unknown. */
static VulkanWindow::ScanCode translateScanCode(int sdlScanCode)
{
	if(sdlScanCode <= 0)
		return VulkanWindow::ScanCode::Unknown;
	if(sdlScanCode >= int(sizeof(scanCodeConversionTable)/sizeof(VulkanWindow::ScanCode)))
		return VulkanWindow::ScanCode(1000 + sdlScanCode);
	VulkanWindow::ScanCode scanCode = scanCodeConversionTable[sdlScanCode];
	if(scanCode == VulkanWindow::ScanCode::Unknown)
		return VulkanWindow::ScanCode(1000 + sdlScanCode);
	return scanCode;
}

#endif



void VulkanWindow::init()
{
#if defined(USE_PLATFORM_WIN32)

	// handle multiple init attempts
	if(win32::windowClass)
		return;

	// register window class with the first window
	win32::hInstance = GetModuleHandle(NULL);
	win32::windowClass =
		RegisterClassExW(
			&(const WNDCLASSEXW&)WNDCLASSEXW{
				sizeof(WNDCLASSEXW),  // cbSize
				0,                    // style
				VulkanWindowPrivate::wndProc,  // lpfnWndProc
				0,                    // cbClsExtra
				sizeof(LONG_PTR),     // cbWndExtra
				HINSTANCE(win32::hInstance),  // hInstance
				LoadIcon(NULL, IDI_APPLICATION),  // hIcon
				LoadCursor(NULL, IDC_ARROW),  // hCursor
				(HBRUSH)(COLOR_WINDOW + 1),  // hbrBackground
				NULL,                 // lpszMenuName
				L"VulkanWindow",      // lpszClassName
				LoadIcon(NULL, IDI_APPLICATION)  // hIconSm
			}
		);
	if(!win32::windowClass)
		throw runtime_error("Cannot register window class.");

	// init keyboard stuff
	// (WM_INPUTLANGCHANGE is sent after keyboard layout was changed;
	// any layout change since application start is reported this way)
	initKeyConversionTable();

#elif defined(USE_PLATFORM_XLIB)

	if(xlib::display)
		return;

	// open X connection
	xlib::display = XOpenDisplay(nullptr);
	if(xlib::display == nullptr)
		throw runtime_error("Can not open display. No X-server running or wrong DISPLAY variable.");
	xlib::externalDisplayHandle = false;

	// get atoms
	xlib::wmDeleteMessage = XInternAtom(xlib::display, "WM_DELETE_WINDOW", False);
	xlib::wmStateProperty = XInternAtom(xlib::display, "WM_STATE", False);
	xlib::netWmName  = XInternAtom(xlib::display, "_NET_WM_NAME", False);
	xlib::utf8String = XInternAtom(xlib::display, "UTF8_STRING", False);

#elif defined(USE_PLATFORM_WAYLAND)

	init(nullptr);

#elif defined(USE_PLATFORM_SDL3)

	// handle multiple init attempts
	if(sdl::initialized)
		return;

	// set hints
	SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");  // do not send SDL_EVENT_QUIT/SDL_QUIT event when the last window closes;
	                                                       // we shall exit main loop after VulkanWindow::exitMainLoop() is called
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");  // allow screensaver

	// initialize SDL
	if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
		throw runtime_error(string("SDL_InitSubSystem(SDL_INIT_VIDEO) function failed. Error details: ") + SDL_GetError());
	sdl::initialized = true;

	// initialize Vulkan
	if(!SDL_Vulkan_LoadLibrary(nullptr))
		throw runtime_error(string("VulkanWindow: SDL_Vulkan_LoadLibrary(nullptr) function failed. Error details: ") + SDL_GetError());

#elif defined(USE_PLATFORM_SDL2)

	// handle multiple init attempts
	if(sdl::initialized)
		return;

	// set hints
	SDL_SetHint("SDL_QUIT_ON_LAST_WINDOW_CLOSE", "0");   // do not send SDL_EVENT_QUIT/SDL_QUIT event when the last window closes; we shall exit main loop
	                                                     // after VulkanWindow::exitMainLoop() is called; the hint is supported since SDL 2.0.22,
	                                                     // therefore we pass it as string and not as macro define because it does not exist on previous versions
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");  // allow screensaver

	// initialize SDL
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		throw runtime_error(string("SDL_InitSubSystem(SDL_INIT_VIDEO) function failed. Error details: ") + SDL_GetError());
	sdl::initialized = true;

	// initialize Vulkan
	if(SDL_Vulkan_LoadLibrary(nullptr) != 0)
		throw runtime_error(string("VulkanWindow: SDL_Vulkan_LoadLibrary(nullptr) function failed. Error details: ") + SDL_GetError());

#elif defined(USE_PLATFORM_GLFW)

	// initialize GLFW
	// (it is safe to call glfwInit() multiple times)
	if(!glfwInit())
		throwError("glfwInit");

#elif defined(USE_PLATFORM_QT)

	if(qt::qGuiApplication)
		return;

# if defined(_WIN32)

	// construct QGuiApplication
	qt::qGuiApplication = reinterpret_cast<QGuiApplication*>(&qt::qGuiApplicationMemory);
	new(qt::qGuiApplication) QGuiApplication(__argc, __argv);

# else

	// get command line arguments
	ifstream f("/proc/self/cmdline", ios::binary);
	altArgBuffer.clear();
	int c = f.get();
	while(f) {
		altArgBuffer.push_back(char(c));
		c = f.get();
	}
	if(altArgBuffer.size()==0 || altArgBuffer.back()!='\0')
		altArgBuffer.push_back('\0');
	altArgv.clear();
	altArgv.push_back(&altArgBuffer[0]);
	for(int i=0,c=int(altArgBuffer.size())-1; i<c; i++)
		if(altArgBuffer[i] == '\0')
			altArgv.push_back(&altArgBuffer[i+1]);
	altArgc = int(altArgv.size());
	altArgv.push_back(nullptr);  // argv[argc] must be nullptr

	// construct QGuiApplication
	qGuiApplication = reinterpret_cast<QGuiApplication*>(&qGuiApplicationMemory);
	new(qGuiApplication) QGuiApplication(altArgc, altArgv.data());

# endif

#endif
}


void VulkanWindow::init(void* data)
{
#if defined(USE_PLATFORM_XLIB)

	// use data as Display* handle

	if(xlib::display)
		return;

	if(data) {

		// use data as Display* handle
		xlib::display = reinterpret_cast<Display*>(data);
		xlib::externalDisplayHandle = true;

	}
	else {

		// open X connection
		xlib::display = XOpenDisplay(nullptr);
		if(xlib::display == nullptr)
			throw runtime_error("Can not open display. No X-server running or wrong DISPLAY variable.");
		xlib::externalDisplayHandle = false;

	}

	// get atoms
	xlib::wmDeleteMessage = XInternAtom(xlib::display, "WM_DELETE_WINDOW", False);
	xlib::wmStateProperty = XInternAtom(xlib::display, "WM_STATE", False);
	xlib::netWmName  = XInternAtom(xlib::display, "_NET_WM_NAME", False);
	xlib::utf8String = XInternAtom(xlib::display, "UTF8_STRING", False);

#elif defined(USE_PLATFORM_WAYLAND)

	// use data as wl_display* handle

	if(wayland::display)
		return;

	if(data) {
		wayland::display = reinterpret_cast<wl_display*>(data);
		wayland::externalDisplayHandle = true;
	}
	else {

		// open Wayland connection
		wayland::display = wl_display_connect(nullptr);
		if(wayland::display == nullptr)
			throw runtime_error("Cannot connect to Wayland display. No Wayland server is running or invalid WAYLAND_DISPLAY variable.");
		wayland::externalDisplayHandle = false;

	}

	// registry listener
	wayland::registry = wl_display_get_registry(wayland::display);
	if(wayland::registry == nullptr)
		throw runtime_error("Cannot get Wayland registry object.");
	if(wl_registry_add_listener(wayland::registry, &registryListener, nullptr))
		throw runtime_error("wl_registry_add_listener() failed.");
	if(wl_display_roundtrip(wayland::display) == -1)
		throw runtime_error("wl_display_roundtrip() failed.");

	// make sure we have all required global objects
	if(wayland::compositor == nullptr)
		throw runtime_error("Cannot get Wayland wl_compositor object.");
	if(wayland::xdgWmBase == nullptr)
		throw runtime_error("Cannot get Wayland xdg_wm_base object.");
	if(wayland::shm == nullptr)
		throw runtime_error("Cannot get Wayland wl_shm object.");
	if(wayland::seat == nullptr)
		throw runtime_error("Cannot get Wayland wl_seat object.");

	// add listeners
	if(xdg_wm_base_add_listener(wayland::xdgWmBase, &xdgWmBaseListener, nullptr))
		throw runtime_error("xdg_wm_base_add_listener() failed.");
	if(wl_seat_add_listener(wayland::seat, &seatListener, nullptr))
		throw runtime_error("wl_seat_add_listener() failed.");

	// xkb_context
	wayland::xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if(wayland::xkbContext == NULL)
		throw runtime_error("VulkanWindow::init(): Cannot create XKB context.");

	// libdecor
	if(!wayland::zxdgDecorationManagerV1) {

		// load libdecor library
		wayland::libdecorHandle = dlopen("libdecor-0.so.0", RTLD_NOW);
		if(wayland::libdecorHandle == nullptr)
			throw runtime_error("Cannot activate window decorations. There is no support for server-side decorations "
			                    "in Wayland server (zxdg_decoration_manager_v1 protocol required) and "
			                    "cannot open libdecor-0.so.0 library for client-side decorations.");

		// function pointers
		reinterpret_cast<void*&>(wayland::funcs.libdecor_new)                 = dlsym(wayland::libdecorHandle, "libdecor_new");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_unref)               = dlsym(wayland::libdecorHandle, "libdecor_unref");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_unref)         = dlsym(wayland::libdecorHandle, "libdecor_frame_unref");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_set_user_data) = dlsym(wayland::libdecorHandle, "libdecor_frame_set_user_data");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_decorate)            = dlsym(wayland::libdecorHandle, "libdecor_decorate");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_set_title)     = dlsym(wayland::libdecorHandle, "libdecor_frame_set_title");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_set_minimized) = dlsym(wayland::libdecorHandle, "libdecor_frame_set_minimized");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_set_maximized) = dlsym(wayland::libdecorHandle, "libdecor_frame_set_maximized");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_unset_maximized) = dlsym(wayland::libdecorHandle, "libdecor_frame_unset_maximized");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_set_fullscreen) = dlsym(wayland::libdecorHandle, "libdecor_frame_set_fullscreen");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_unset_fullscreen) = dlsym(wayland::libdecorHandle, "libdecor_frame_unset_fullscreen");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_map)           = dlsym(wayland::libdecorHandle, "libdecor_frame_map");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_configuration_get_window_state) = dlsym(wayland::libdecorHandle, "libdecor_configuration_get_window_state");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_configuration_get_content_size) = dlsym(wayland::libdecorHandle, "libdecor_configuration_get_content_size");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_state_new)           = dlsym(wayland::libdecorHandle, "libdecor_state_new");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_frame_commit)        = dlsym(wayland::libdecorHandle, "libdecor_frame_commit");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_state_free)          = dlsym(wayland::libdecorHandle, "libdecor_state_free");
		reinterpret_cast<void*&>(wayland::funcs.libdecor_dispatch)            = dlsym(wayland::libdecorHandle, "libdecor_dispatch");
		if(!wayland::funcs.libdecor_new || !wayland::funcs.libdecor_unref || !wayland::funcs.libdecor_frame_unref || !wayland::funcs.libdecor_decorate ||
		   !wayland::funcs.libdecor_frame_set_title || !wayland::funcs.libdecor_frame_set_minimized || !wayland::funcs.libdecor_frame_set_maximized ||
		   !wayland::funcs.libdecor_frame_unset_maximized || !wayland::funcs.libdecor_frame_set_fullscreen ||
		   !wayland::funcs.libdecor_frame_unset_fullscreen || !wayland::funcs.libdecor_frame_map ||
		   !wayland::funcs.libdecor_configuration_get_window_state || !wayland::funcs.libdecor_configuration_get_content_size ||
		   !wayland::funcs.libdecor_state_new || !wayland::funcs.libdecor_frame_commit || !wayland::funcs.libdecor_state_free || !wayland::funcs.libdecor_dispatch)
		{
			throw runtime_error("Cannot retrieve all function pointers out of libdecor-0.so.");
		}

		// workaround for missing libdecor_frame_set_user_data() in versions 0.1.0 to 0.2.2
		if(wayland::funcs.libdecor_frame_set_user_data == nullptr)
			wayland::funcs.libdecor_frame_set_user_data =
				[](struct libdecor_frame* frame, void* user_data) -> void {
					auto* priv = reinterpret_cast<libdecor_frame_workaround*>(frame)->priv;
					reinterpret_cast<libdecor_frame_private_workaround*>(priv)->user_data = user_data;
				};

		// create libdecor context
		wayland::libdecorContext = wayland::funcs.libdecor_new(wayland::display, &libdecorInterface);
		if(!wayland::libdecorContext)
			throw runtime_error("libdecor_new() failed.");

	}

	// cursor size
	const char* cursorSizeString = getenv("XCURSOR_SIZE");
	char* endp = nullptr;
	int cursorSize = 0;
	if(cursorSizeString) {
		auto cursorSizeLong = strtol(cursorSizeString, &endp, 10);
		if(endp-cursorSizeString == ptrdiff_t(strlen(cursorSizeString)) &&
		   cursorSizeLong > 0 && cursorSizeLong <= INT_MAX)
		{
			cursorSize = int(cursorSizeLong);
		}
	}
	if(cursorSize == 0)
		cursorSize = 24;

	// cursor theme name
	const char* cursorThemeName = getenv("XCURSOR_THEME");

	// load cursor theme
	wayland::cursorTheme = wl_cursor_theme_load(cursorThemeName, cursorSize, wayland::shm);
	if(wayland::cursorTheme == nullptr)
		throw runtime_error("Failed to load default cursor theme.");

	// cursor surface
	// (we need cursor for VulkanWindow, otherwise no cursor might be shown inside the window)
	wayland::cursorSurface = wl_compositor_create_surface(wayland::compositor);
	if(!wayland::cursorSurface)
		throw runtime_error("wl_compositor_create_surface() failed to create cursor surface.");
	wl_cursor* cursor = wl_cursor_theme_get_cursor(wayland::cursorTheme, "left_ptr");
	if(!cursor)
		throw runtime_error("Cursor error: Cannot load \"left_ptr\" cursor.");
	wl_cursor_image* cursorImage = cursor->images[0];
	wayland::cursorHotspotX = cursorImage->hotspot_x;
	wayland::cursorHotspotY = cursorImage->hotspot_y;
	wl_buffer* cursorBuffer = wl_cursor_image_get_buffer(cursorImage);
	if(!cursorBuffer)
		throw runtime_error("wl_cursor_image_get_buffer() failed.");
	wl_surface_attach(wayland::cursorSurface, cursorBuffer, 0, 0);
	wl_surface_commit(wayland::cursorSurface);

	// process wm_base and wl_seat listeners
	if(wl_display_roundtrip(wayland::display) == -1)
		throw runtime_error("wl_display_roundtrip() failed.");

#elif defined(USE_PLATFORM_QT)

	// use data as pointer to
	// tuple<QGuiApplication*,QVulkanInstance*>

	if(qt::qGuiApplication)
		return;

	// get objects from data parameter
	if(data) {
		auto& d = *reinterpret_cast<tuple<QGuiApplication*,QVulkanInstance*>*>(data);
		qt::qGuiApplication = get<0>(d);
		qt::qVulkanInstance = get<1>(d);
	}

	// construct QGuiApplication
	// using VulkanWindow::init()
	if(qt::qGuiApplication == nullptr)
		init();

#else

	// on all other platforms,
	// just perform init()
	init();

#endif
}


#if defined(USE_PLATFORM_QT)

// use argc and argv
// for QGuiApplication initialization
void VulkanWindow::init(int& argc, char* argv[])
{
	if(qt::qGuiApplication)
		return;

	// construct QGuiApplication
	qt::qGuiApplication = reinterpret_cast<QGuiApplication*>(&qt::qGuiApplicationMemory);
	new(qt::qGuiApplication) QGuiApplication(argc, argv);
}

#else

// on all other platforms,
// just perform init()
void VulkanWindow::init(int&, char*[])
{
	init();
}

#endif


void VulkanWindow::finalize() noexcept
{
#if defined(USE_PLATFORM_WIN32)

	// release resources
	// (do not throw in the finalization code,
	// so ignore the errors in release builds and assert in debug builds)
	if(win32::windowClass) {
# ifdef NDEBUG
		UnregisterClassW(LPWSTR(MAKEINTATOM(win32::windowClass)), HINSTANCE(win32::hInstance));
# else
		if(!UnregisterClassW(LPWSTR(MAKEINTATOM(win32::windowClass)), HINSTANCE(win32::hInstance)))
			assert(0 && "UnregisterClass(): The function failed.");
# endif
		win32::windowClass = 0;
	}

#elif defined(USE_PLATFORM_XLIB)

	if(xlib::display) {
		if(!xlib::externalDisplayHandle)
			XCloseDisplay(xlib::display);
		xlib::display = nullptr;
		xlib::vulkanWindowMap.clear();
	}

#elif defined(USE_PLATFORM_WAYLAND)

	if(wayland::pointer) {
		wl_pointer_release(wayland::pointer);
		wayland::pointer = nullptr;
	}
	if(wayland::keyboard) {
		wl_keyboard_release(wayland::keyboard);
		wayland::keyboard = nullptr;
	}
	if(wayland::cursorSurface) {
		wl_surface_destroy(wayland::cursorSurface);
		wayland::cursorSurface = nullptr;
	}
	if(wayland::cursorTheme) {
		wl_cursor_theme_destroy(wayland::cursorTheme);
		wayland::cursorTheme = nullptr;
	}
	if(wayland::libdecorContext) {
		wayland::funcs.libdecor_unref(wayland::libdecorContext);
		wayland::libdecorContext = nullptr;
	}
	if(wayland::shm) {
		wl_shm_destroy(wayland::shm);
		wayland::shm = nullptr;
	}
	if(wayland::seat) {
		wl_seat_release(wayland::seat);
		wayland::seat = nullptr;
	}
	if(wayland::xkbState) {
		xkb_state_unref(wayland::xkbState);
		wayland::xkbState = nullptr;
	}
	if(wayland::xkbContext) {
		xkb_context_unref(wayland::xkbContext);
		wayland::xkbContext = nullptr;
	}
	if(wayland::xdgWmBase) {
		xdg_wm_base_destroy(wayland::xdgWmBase);
		wayland::xdgWmBase = nullptr;
	}
	if(wayland::display) {
		if(!wayland::externalDisplayHandle)
			wl_display_disconnect(wayland::display);
		wayland::display = nullptr;
	}
	if(wayland::libdecorHandle) {
		dlclose(wayland::libdecorHandle);
		wayland::libdecorHandle = nullptr;
	}
	wayland::registry = nullptr;
	wayland::compositor = nullptr;
	wayland::xdgWmBase = nullptr;
	wayland::zxdgDecorationManagerV1 = nullptr;

#elif defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2)

	// finalize SDL
	if(sdl::initialized) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_Quit();
		sdl::initialized = false;
	}

#elif defined(USE_PLATFORM_GLFW)

	// finalize GLFW
	// (it is safe to call glfwTerminate() even if GLFW was not initialized)
	glfwTerminate();
# if 1 // test error by assert
	assert(glfwGetError(nullptr) == GLFW_NO_ERROR && "VulkanWindow: glfwTerminate() function failed.");
# else // print error if any
	const char* errorString;
	int errorCode = glfwGetError(&errorString);
	if(errorCode != GLFW_NO_ERROR) {
		cout <<  && "VulkanWindow: glfwTerminate() function failed. Error code: 0x"
		     << hex << errorCode << ". Error string: " << errorString << endl;
		assert(0 && "VulkanWindow: glfwTerminate() function failed.");
	}
# endif

#elif defined(USE_PLATFORM_QT)

	// delete QVulkanInstance object
	// but only if we own it
	if(qt::qVulkanInstance == reinterpret_cast<QVulkanInstance*>(&qt::qVulkanInstanceMemory))
		qt::qVulkanInstance->~QVulkanInstance();
	qt::qVulkanInstance = nullptr;

	// destroy QGuiApplication object
	// but only if we own it
	if(qt::qGuiApplication == reinterpret_cast<QGuiApplication*>(&qt::qGuiApplicationMemory))
		qt::qGuiApplication->~QGuiApplication();
	qt::qGuiApplication = nullptr;

#endif
}



VulkanWindow::~VulkanWindow()
{
	destroy();
}


void VulkanWindow::destroy() noexcept
{
	// skip not created windows
	if(_any.handle == nullptr)
		return;

	// destroy surface except Qt platform
#if !defined(USE_PLATFORM_QT)
	if(_instance && _surface)
	{
		// get function pointer
		// (we do not store the pointer because it is used only once in life-time of the VulkanWindow)
		PFN_vkDestroySurfaceKHR vulkanDestroySurfaceKHR =
			reinterpret_cast<PFN_vkDestroySurfaceKHR>(_vulkanGetInstanceProcAddr(_instance, "vkDestroySurfaceKHR"));

		// destroy surface
		vulkanDestroySurfaceKHR(_instance, _surface, nullptr);
		_surface = nullptr;
	}
#else
	// do not destroy surface on Qt platform
	// because QtRenderingWindow owns the surface object and it will destroy the surface by itself
	_surface = nullptr;
#endif

#if defined(USE_PLATFORM_WIN32)

	// release resources
	// (do not throw in destructor, so ignore the errors in release builds
	// and assert in debug builds)
# ifdef NDEBUG
	DestroyWindow(HWND(_win32.hwnd));
	_win32.hwnd = nullptr;
# else
	_win32.visible = false;
	assert(win32::windowClass && "VulkanWindow::destroy(): Window class does not exist. "
	                             "Did you called VulkanWindow::finalize() prematurely?");
	if(!DestroyWindow(HWND(_win32.hwnd)))
		assert(0 && "DestroyWindow(): The function failed.");
	_win32.hwnd = nullptr;
# endif

#elif defined(USE_PLATFORM_XLIB)

	// release resources
	xlib::vulkanWindowMap.erase(_xlib.window);
	XDestroyWindow(xlib::display, _xlib.window);
	_xlib.window = 0;

#elif defined(USE_PLATFORM_WAYLAND)

	// invalidate pointers to this object
	// (maybe, leave events are sent on surface destroy and these are not necessary (?))
	if(wayland::windowUnderPointer == this)
		wayland::windowUnderPointer = nullptr;
	if(wayland::windowWithKbFocus == this)
		wayland::windowWithKbFocus = nullptr;

	// release resources
	if(_wayland.scheduledFrameCallback) {
		wl_callback_destroy(_wayland.scheduledFrameCallback);
		_wayland.scheduledFrameCallback = nullptr;
	}
	if(_wayland.libdecorFrame) {
		wayland::funcs.libdecor_frame_unref(_wayland.libdecorFrame);
		_wayland.libdecorFrame = nullptr;
	}
	if(_wayland.decoration) {
		zxdg_toplevel_decoration_v1_destroy(_wayland.decoration);
		_wayland.decoration = nullptr;
	}
	if(_wayland.xdgTopLevel) {
		xdg_toplevel_destroy(_wayland.xdgTopLevel);
		_wayland.xdgTopLevel = nullptr;
	}
	if(_wayland.xdgSurface) {
		xdg_surface_destroy(_wayland.xdgSurface);
		_wayland.xdgSurface = nullptr;
	}
	if(_wayland.wlSurface) {
		wl_surface_destroy(_wayland.wlSurface);
		_wayland.wlSurface = nullptr;
	}

#elif defined(USE_PLATFORM_SDL3)

	// get windowID
	auto windowID = SDL_GetWindowID(_sdl.window);
	assert(windowID != 0 && "SDL_GetWindowID(): The function failed.");

	// destroy window
	SDL_DestroyWindow(_sdl.window);
	_sdl.window = nullptr;

	// get all events in the queue
	SDL_PumpEvents();
	vector<SDL_Event> buf(16);
	while(true) {
		int num = SDL_PeepEvents(&buf[buf.size()-16], 16, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
		if(num < 0) {
			assert(0 && "SDL_PeepEvents(): The function failed.");
			break;
		}
		if(num == 16) {
			buf.resize(buf.size() + 16);
			continue;
		}
		buf.resize(buf.size() - 16 + num);
		break;
	};

	// fill the queue again
	// while skipping all events of deleted window
	// (we need to skip those that are processed in VulkanWindow::mainLoop() because they might cause SIGSEGV)
	for(SDL_Event& event : buf) {
		if(event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
			if(event.window.windowID == windowID)
				continue;
		int num = SDL_PeepEvents(&event, 1, SDL_ADDEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
		if(num != 1)
			assert(0 && "SDL_PeepEvents(): The function failed.");
	}

#elif defined(USE_PLATFORM_SDL2)

	// get windowID
	auto windowID = SDL_GetWindowID(_sdl.window);
	assert(windowID != 0 && "SDL_GetWindowID(): The function failed.");

	// destroy window
	SDL_DestroyWindow(_sdl.window);
	_sdl.window = nullptr;

	// get all events in the queue
	SDL_PumpEvents();
	vector<SDL_Event> buf(16);
	while(true) {
		int num = SDL_PeepEvents(&buf[buf.size()-16], 16, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
		if(num < 0) {
			assert(0 && "SDL_PeepEvents(): The function failed.");
			break;
		}
		if(num == 16) {
			buf.resize(buf.size() + 16);
			continue;
		}
		buf.resize(buf.size() - 16 + num);
		break;
	};

	// fill the queue again
	// while skipping all events of deleted window
	// (we need to skip those that are processed in VulkanWindow::mainLoop() because they might cause SIGSEGV)
	for(SDL_Event& event : buf) {
		if(event.type == SDL_WINDOWEVENT)
			if(event.window.windowID == windowID)
				continue;
		int num = SDL_PeepEvents(&event, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
		if(num != 1)
			assert(0 && "SDL_PeepEvents(): The function failed.");
	}

#elif defined(USE_PLATFORM_GLFW)

	// destroy window
	glfwDestroyWindow(_glfw.window);
	_glfw.window = nullptr;

	// cancel pending frame, if any
	if(_glfw.framePendingState != FramePendingState::NotPending) {
		_glfw.framePendingState = FramePendingState::NotPending;
		for(size_t i=0; i<glfw::framePendingWindows.size(); i++)
			if(glfw::framePendingWindows[i] == this) {
				glfw::framePendingWindows[i] = glfw::framePendingWindows.back();
				glfw::framePendingWindows.pop_back();
				break;
			}
	}

#elif defined(USE_PLATFORM_QT)

	// delete QtRenderingWindow
	delete _qt.window;
	_qt.window = nullptr;

#endif
}


VulkanWindow::VulkanWindow(VulkanWindow&& other) noexcept
{
#if defined(USE_PLATFORM_WIN32)

	// move members
	_win32 = other._win32;
	other._win32.hwnd = nullptr;

	// update pointers to this object
	if(_win32.hwnd)
		SetWindowLongPtr(HWND(_win32.hwnd), 0, LONG_PTR(this));
	for(VulkanWindow*& w : win32::framePendingWindows)
		if(w == &other) {
			w = this;
			break;
		}

#elif defined(USE_PLATFORM_XLIB)

	// move Xlib members
	_xlib = other._xlib;
	other._xlib.window = 0;

	// update pointers to this object
	if(_xlib.window != 0)
		xlib::vulkanWindowMap[_xlib.window] = this;

#elif defined(USE_PLATFORM_WAYLAND)

	// process all pending sync events
	// because they are delivered to the current object address
	waitAllSyncEvents(_wayland.numSyncEventsOnTheFly);

	// move Wayland members
	_wayland = other._wayland;
	if(_wayland.wlSurface)
		wl_surface_set_user_data(_wayland.wlSurface, this);
	if(_wayland.xdgSurface)
		xdg_surface_set_user_data(_wayland.xdgSurface, this);
	if(_wayland.xdgTopLevel)
		xdg_toplevel_set_user_data(_wayland.xdgTopLevel, this);
	if(_wayland.libdecorFrame)
		wayland::funcs.libdecor_frame_set_user_data(_wayland.libdecorFrame, this);
	if(_wayland.scheduledFrameCallback)
		wl_callback_set_user_data(_wayland.scheduledFrameCallback, this);
	other._wayland.wlSurface = nullptr;
	other._wayland.xdgSurface = nullptr;
	other._wayland.xdgTopLevel = nullptr;
	other._wayland.decoration = nullptr;
	other._wayland.libdecorFrame = nullptr;
	other._wayland.scheduledFrameCallback = nullptr;

	// update pointers to this object
	if(wayland::windowUnderPointer == &other)
		wayland::windowUnderPointer = static_cast<VulkanWindowPrivate*>(this);
	if(wayland::windowWithKbFocus == &other)
		wayland::windowWithKbFocus = static_cast<VulkanWindowPrivate*>(this);

#elif defined(USE_PLATFORM_SDL3)

	// move SDL members
	_sdl = other._sdl;
	other._sdl.window = nullptr;

	if(_sdl.window != nullptr)
	{
		// update pointer to this object
		SDL_PropertiesID props = SDL_GetWindowProperties(_sdl.window);
		assert(props != 0 && "VulkanWindow: SDL_GetWindowProperties() function failed while updating VulkanWindow pointer.");
		if(!SDL_SetPointerProperty(props, sdl::windowPointerName, this))
			assert(0 && "VulkanWindow: SDL_SetPointerProperty() function failed while updating VulkanWindow pointer.");
	}

#elif defined(USE_PLATFORM_SDL2)

	// move SDL members
	_sdl = other._sdl;
	other._sdl.window = nullptr;

	// update pointer to this object
	if(_sdl.window)
		SDL_SetWindowData(_sdl.window, sdl::windowPointerName, this);

#elif defined(USE_PLATFORM_GLFW)

	// move GLFW members
	_glfw = other._glfw;
	other._glfw.window = nullptr;

	// update pointers to this object
	if(_glfw.window)
		glfwSetWindowUserPointer(_glfw.window, this);
	for(VulkanWindow*& w : glfw::framePendingWindows)
		if(w == &other) {
			w = this;
			break;
		}

#elif defined(USE_PLATFORM_QT)

	// move Qt members
	_qt = other._qt;
	if(_qt.window) {
		other._qt.window = nullptr;
		static_cast<QtRenderingWindow*>(_qt.window)->vulkanWindow = this;
	}

#endif

	// move members
	_frameCallback = move(other._frameCallback);
	_instance = move(other._instance);
	_physicalDevice = move(other._physicalDevice);
	_device = move(other._device);
	_surface = other._surface;
	other._surface = nullptr;
	_vulkanGetInstanceProcAddr = other._vulkanGetInstanceProcAddr;
	_vulkanDeviceWaitIdle = other._vulkanDeviceWaitIdle;
	_vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR = other._vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR;
	_surfaceExtent = other._surfaceExtent;
	_resizePending = other._resizePending;
	_resizeCallback = move(other._resizeCallback);
	_closeCallback = move(other._closeCallback);
	_mouseState = other._mouseState;
	_mouseMoveCallback = move(other._mouseMoveCallback);
	_mouseButtonCallback = move(other._mouseButtonCallback);
	_mouseWheelCallback = move(other._mouseWheelCallback);
	_keyCallback = move(other._keyCallback);
	_title = move(other._title);
}


VulkanWindow& VulkanWindow::operator=(VulkanWindow&& other) noexcept
{
	// destroy previous content
	destroy();

#if defined(USE_PLATFORM_WIN32)

	// move members
	_win32 = other._win32;
	other._win32.hwnd = nullptr;

	// update pointers to this object
	if(_win32.hwnd)
		SetWindowLongPtr(HWND(_win32.hwnd), 0, LONG_PTR(this));
	for(VulkanWindow*& w : win32::framePendingWindows)
		if(w == &other) {
			w = this;
			break;
		}

#elif defined(USE_PLATFORM_XLIB)

	// move Xlib members
	_xlib = other._xlib;
	other._xlib.window = 0;

	// update pointers to this object
	if(_xlib.window != 0)
		xlib::vulkanWindowMap[_xlib.window] = this;

#elif defined(USE_PLATFORM_WAYLAND)

	// process all pending sync events
	// because they are delivered to the current object address
	waitAllSyncEvents(_wayland.numSyncEventsOnTheFly);

	// move Wayland members
	_wayland = other._wayland;
	if(_wayland.wlSurface)
		wl_surface_set_user_data(_wayland.wlSurface, this);
	if(_wayland.xdgSurface)
		xdg_surface_set_user_data(_wayland.xdgSurface, this);
	if(_wayland.xdgTopLevel)
		xdg_toplevel_set_user_data(_wayland.xdgTopLevel, this);
	if(_wayland.libdecorFrame)
		wayland::funcs.libdecor_frame_set_user_data(_wayland.libdecorFrame, this);
	if(_wayland.scheduledFrameCallback)
		wl_callback_set_user_data(_wayland.scheduledFrameCallback, this);
	other._wayland.wlSurface = nullptr;
	other._wayland.xdgSurface = nullptr;
	other._wayland.xdgTopLevel = nullptr;
	other._wayland.decoration = nullptr;
	other._wayland.libdecorFrame = nullptr;
	other._wayland.scheduledFrameCallback = nullptr;

	// update pointers to this object
	if(wayland::windowUnderPointer == &other)
		wayland::windowUnderPointer = static_cast<VulkanWindowPrivate*>(this);
	if(wayland::windowWithKbFocus == &other)
		wayland::windowWithKbFocus = static_cast<VulkanWindowPrivate*>(this);

#elif defined(USE_PLATFORM_SDL3)

	// move SDL members
	_sdl = other._sdl;
	other._sdl.window = nullptr;

	if(_sdl.window != nullptr)
	{
		// set pointer to this
		SDL_PropertiesID props = SDL_GetWindowProperties(_sdl.window);
		assert(props != 0 && "VulkanWindow: SDL_GetWindowProperties() function failed while updating VulkanWindow pointer.");
		if(!SDL_SetPointerProperty(props, sdl::windowPointerName, this))
			assert(0 && "VulkanWindow: SDL_SetPointerProperty() function failed while updating VulkanWindow pointer.");
	}

#elif defined(USE_PLATFORM_SDL2)

	// move SDL members
	_sdl = other._sdl;
	other._sdl.window = nullptr;

	// set pointer to this
	if(_sdl.window)
		SDL_SetWindowData(_sdl.window, sdl::windowPointerName, this);

#elif defined(USE_PLATFORM_GLFW)

	// move GLFW members
	_glfw = other._glfw;
	other._glfw.window = nullptr;

	// update pointers to this object
	if(_glfw.window)
		glfwSetWindowUserPointer(_glfw.window, this);
	for(VulkanWindow*& w : glfw::framePendingWindows)
		if(w == &other) {
			w = this;
			break;
		}

#elif defined(USE_PLATFORM_QT)

	// move Qt members
	_qt = other._qt;
	if(_qt.window) {
		other._qt.window = nullptr;
		static_cast<QtRenderingWindow*>(_qt.window)->vulkanWindow = this;
	}

#endif

	// move members
	_frameCallback = move(other._frameCallback);
	_instance = move(other._instance);
	_physicalDevice = move(other._physicalDevice);
	_device = move(other._device);
	_surface = other._surface;
	other._surface = nullptr;
	_vulkanGetInstanceProcAddr = other._vulkanGetInstanceProcAddr;
	_vulkanDeviceWaitIdle = other._vulkanDeviceWaitIdle;
	_vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR = other._vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR;
	_surfaceExtent = other._surfaceExtent;
	_resizePending = other._resizePending;
	_resizeCallback = move(other._resizeCallback);
	_closeCallback = move(other._closeCallback);
	_mouseState = other._mouseState;
	_mouseMoveCallback = move(other._mouseMoveCallback);
	_mouseButtonCallback = move(other._mouseButtonCallback);
	_mouseWheelCallback = move(other._mouseWheelCallback);
	_keyCallback = move(other._keyCallback);
	_title = move(other._title);

	return *this;
}


VkSurfaceKHR VulkanWindow::create(VkInstance instance, VkExtent2D surfaceExtent, const string& title,
                                  PFN_vkGetInstanceProcAddr getInstanceProcAddr)
{
	// destroy any previous window data
	// (this makes calling create() multiple times safe operation)
	destroy();

	_title = title;

	return createInternal(instance, surfaceExtent, getInstanceProcAddr);
}


VkSurfaceKHR VulkanWindow::createInternal(VkInstance instance, VkExtent2D surfaceExtent,
                                          PFN_vkGetInstanceProcAddr getInstanceProcAddr)
{
	// asserts for valid usage
	assert(instance && "The parameter instance must not be null.");
#if defined(USE_PLATFORM_WIN32)
	assert(win32::windowClass && "VulkanWindow class was not initialized. Call VulkanWindow::init() before VulkanWindow::create().");
#elif defined(USE_PLATFORM_XLIB)
	assert(xlib::display && "VulkanWindow class was not initialized. Call VulkanWindow::init() before VulkanWindow::create().");
#elif defined(USE_PLATFORM_WAYLAND)
	assert(wayland::display && "VulkanWindow class was not initialized. Call VulkanWindow::init() before VulkanWindow::create().");
#elif defined(USE_PLATFORM_SDL3) || defined(USE_PLATFORM_SDL2)
	assert(sdl::initialized && "VulkanWindow class was not initialized. Call VulkanWindow::init() before VulkanWindow::create().");
#elif defined(USE_PLATFORM_QT)
	assert(qt::qGuiApplication && "VulkanWindow class was not initialized. Call VulkanWindow::init() before VulkanWindow::create().");
#endif

	// set Vulkan instance
	_instance = instance;

	// set surface extent
	_surfaceExtent = surfaceExtent;

	// set Vulkan function pointers
	_vulkanGetInstanceProcAddr = getInstanceProcAddr;
	if(_vulkanGetInstanceProcAddr == nullptr)
		throw runtime_error("VulkanWindow: getInstanceProcAddr parameter passed into VulkanWindow::create() must not be nullptr.");
	_vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
		getInstanceProcAddr(_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
	if(_vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkGetPhysicalDeviceSurfaceCapabilitiesKHR function pointer.");

#if defined(USE_PLATFORM_WIN32)

	// init variables
	_win32.framePendingState = FramePendingState::NotPending;
	_win32.visible = false;
	_win32.hiddenWindowFramePending = false;
	_win32.titleBarLeftButtonDownMsgOnHold = false;

	// create window
	_win32.hwnd =
		CreateWindowExW(
			WS_EX_CLIENTEDGE,  // dwExStyle
			LPWSTR(MAKEINTATOM(win32::windowClass)),  // lpClassName
			utf8toWString(_title).c_str(),  // lpWindowName
			WS_OVERLAPPEDWINDOW,  // dwStyle
			CW_USEDEFAULT, CW_USEDEFAULT,  // x,y
			surfaceExtent.width, surfaceExtent.height,  // width, height
			NULL, NULL, HINSTANCE(win32::hInstance), NULL  // hWndParent, hMenu, hInstance, lpParam
		);
	if(_win32.hwnd == NULL)
		throw runtime_error("VulkanWindow: Cannot create window. The function CreateWindowEx() failed.");

	// store this pointer with the window data
	SetWindowLongPtr(HWND(_win32.hwnd), 0, LONG_PTR(this));

	// create surface
	PFN_vkCreateWin32SurfaceKHR vulkanCreateWin32SurfaceKHR =
		reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(getInstanceProcAddr(_instance, "vkCreateWin32SurfaceKHR"));
	if(vulkanCreateWin32SurfaceKHR == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkCreateWin32SurfaceKHR function pointer.");
	VkResult r =
		vulkanCreateWin32SurfaceKHR(
			_instance,  // instance
			&(const VkWin32SurfaceCreateInfoKHR&)VkWin32SurfaceCreateInfoKHR{  // pCreateInfo
				VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // sType
				nullptr,  // pNext
				0,  // flags
				HINSTANCE(win32::hInstance),  // hinstance
				HWND(_win32.hwnd)  // hwnd
			},
			nullptr,  // pAllocator
			reinterpret_cast<VkSurfaceKHR*>(&_surface)  // pSurface
		);
	if(r != VK_SUCCESS)
		throw runtime_error(string("VulkanWindow: vkCreateWin32SurfaceKHR() failed (return code: ") + to_string(r) + ").");

	return _surface;

#elif defined(USE_PLATFORM_XLIB)

	// init variables
	_xlib.framePending = true;
	_xlib.visible = false;
	_xlib.fullyObscured = false;
	_xlib.iconVisible = false;
	_xlib.minimized = false;

	// create window
	XSetWindowAttributes attr;
	attr.event_mask = ExposureMask | StructureNotifyMask | VisibilityChangeMask | PropertyChangeMask |
	                  PointerMotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
	_xlib.window =
		XCreateWindow(
			xlib::display,  // display
			DefaultRootWindow(xlib::display),  // parent
			0, 0,  // x,y
			surfaceExtent.width, surfaceExtent.height,  // width, height
			0,  // border_width
			CopyFromParent,  // depth
			InputOutput,  // class
			CopyFromParent,  // visual
			CWEventMask,  // valuemask
			&attr  // attributes
		);
	if(xlib::vulkanWindowMap.emplace(_xlib.window, this).second == false)
		throw runtime_error("VulkanWindow: The window already exists.");
	XSetWMProtocols(xlib::display, _xlib.window, &xlib::wmDeleteMessage, 1);
	XSetStandardProperties(xlib::display, _xlib.window, _title.c_str(), _title.c_str(), None, NULL, 0, NULL);
	XChangeProperty(
		xlib::display,
		_xlib.window,
		xlib::netWmName,  // property
		xlib::utf8String,  // type
		8,  // format
		PropModeReplace,  // mode
		reinterpret_cast<const unsigned char*>(_title.c_str()),  // data
		_title.size()  // nelements
	);

	// create surface
	PFN_vkCreateXlibSurfaceKHR vulkanCreateXlibSurfaceKHR =
		reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(getInstanceProcAddr(_instance, "vkCreateXlibSurfaceKHR"));
	if(vulkanCreateXlibSurfaceKHR == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkCreateXlibSurfaceKHR function pointer.");
	VkResult r =
		vulkanCreateXlibSurfaceKHR(
			instance,  // instance
			&(const VkXlibSurfaceCreateInfoKHR&)VkXlibSurfaceCreateInfoKHR{  // pCreateInfo
				VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,  // sType
				nullptr,  // pNext
				0,  // flags
				xlib::display,  // dpy
				_xlib.window  // window
			},
			nullptr,  // pAllocator
			reinterpret_cast<VkSurfaceKHR*>(&_surface)  // pSurface
		);
	if(r != VK_SUCCESS)
		throw runtime_error(string("VulkanWindow: vkCreateXlibSurfaceKHR() failed (return code: ") + to_string(r) + ").");

	return _surface;

#elif defined(USE_PLATFORM_WAYLAND)

	// init variables
	_wayland.forcedFrame = false;
	_wayland.xdgSurface = nullptr;
	_wayland.xdgTopLevel = nullptr;
	_wayland.decoration = nullptr;
	_wayland.libdecorFrame = nullptr;
	_wayland.scheduledFrameCallback = nullptr;
	_wayland.windowState = WindowState::Hidden;

	// create wl surface
	_wayland.wlSurface = wl_compositor_create_surface(wayland::compositor);
	if(_wayland.wlSurface == nullptr)
		throw runtime_error("VulkanWindow: wl_compositor_create_surface() failed.");

	// set tag on surface
	wl_proxy_set_tag(reinterpret_cast<wl_proxy*>(_wayland.wlSurface), &wayland::vulkanWindowTag);

	// associate surface with VulkanWindow
	wl_surface_set_user_data(_wayland.wlSurface, this);

	// create surface
	PFN_vkCreateWaylandSurfaceKHR vulkanCreateWaylandSurfaceKHR =
		reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(getInstanceProcAddr(_instance, "vkCreateWaylandSurfaceKHR"));
	if(vulkanCreateWaylandSurfaceKHR == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkCreateWaylandSurfaceKHR function pointer.");
	VkResult r =
		vulkanCreateWaylandSurfaceKHR(
			instance,  // instance
			&(const VkWaylandSurfaceCreateInfoKHR&)VkWaylandSurfaceCreateInfoKHR{  // pCreateInfo
				VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,  // sType
				nullptr,  // pNext
				0,  // flags
				wayland::display,  // display
				_wayland.wlSurface  // surface
			},
			nullptr,  // pAllocator
			reinterpret_cast<VkSurfaceKHR*>(&_surface)  // pSurface
		);
	if(r != VK_SUCCESS)
		throw runtime_error(string("VulkanWindow: vkCreateWaylandSurfaceKHR() failed (return code: ") + to_string(r) + ").");
	if(wl_display_flush(wayland::display) == -1)
		throw runtime_error("VulkanWindow: wl_display_flush() failed.");
	return _surface;

#elif defined(USE_PLATFORM_SDL3)

	// init variables
	_sdl.framePending = true;
	_sdl.hiddenWindowFramePending = false;
	_sdl.visible = false;
	_sdl.minimized = false;

	// create Vulkan window
	_sdl.window = SDL_CreateWindow(
		_title.c_str(),  // title
		surfaceExtent.width, surfaceExtent.height,  // w,h
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN  // flags
	);
	if(_sdl.window == nullptr)
		throw runtime_error(string("VulkanWindow: SDL_CreateWindow() function failed. Error details: ") + SDL_GetError());

	// set pointer to this
	SDL_PropertiesID props = SDL_GetWindowProperties(_sdl.window);
	if(props == 0)
		throw runtime_error(string("VulkanWindow: SDL_GetWindowProperties() function failed. Error details: ") + SDL_GetError());
	if(!SDL_SetPointerProperty(props, sdl::windowPointerName, this))
		throw runtime_error(string("VulkanWindow: SDL_SetPointerProperty() function failed. Error details: ") + SDL_GetError());

	// create surface
	if(!SDL_Vulkan_CreateSurface(_sdl.window, VkInstance(instance), nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface)))
		throw runtime_error(string("VulkanWindow: SDL_Vulkan_CreateSurface() function failed. Error details: ") + SDL_GetError());

	return _surface;

#elif defined(USE_PLATFORM_SDL2)

	// init variables
	_sdl.framePending = true;
	_sdl.hiddenWindowFramePending = false;
	_sdl.visible = false;
	_sdl.minimized = false;

	// create Vulkan window
	_sdl.window = SDL_CreateWindow(
		_title.c_str(),  // title
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,  // x,y
		surfaceExtent.width, surfaceExtent.height,  // w,h
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN  // flags
	);
	if(_sdl.window == nullptr)
		throw runtime_error(string("VulkanWindow: SDL_CreateWindow() function failed. Error details: ") + SDL_GetError());

	// set pointer to this
	SDL_SetWindowData(_sdl.window, sdl::windowPointerName, this);

	// create surface
	if(!SDL_Vulkan_CreateSurface(_sdl.window, VkInstance(instance), reinterpret_cast<VkSurfaceKHR*>(&_surface)))
		throw runtime_error(string("VulkanWindow: SDL_Vulkan_CreateSurface() function failed. Error details: ") + SDL_GetError());

	return _surface;

#elif defined(USE_PLATFORM_GLFW)

	// init variables
	_glfw.framePendingState = FramePendingState::NotPending;
	_glfw.visible = false;
	_glfw.minimized = false;
	_glfw.savedPosX = 0;
	_glfw.savedPosY = 0;
	_glfw.savedWidth = surfaceExtent.width;
	_glfw.savedHeight = surfaceExtent.height;

	// create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
# if !defined(_WIN32)
	glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);  // disabled focus on show prevents fail inside
		// glfwShowWindow(): "Wayland: Focusing a window requires user interaction"; seen on glfw 3.3.8 and Kubuntu 22.10,
		// however we need the focus on show on Win32 to get proper window focus
# endif
	_glfw.window = glfwCreateWindow(surfaceExtent.width, surfaceExtent.height, _title.c_str(), nullptr, nullptr);
	if(_glfw.window == nullptr)
		throwError("glfwCreateWindow");

	// setup window
	glfwSetWindowUserPointer(_glfw.window, this);
	glfwSetWindowRefreshCallback(
		_glfw.window,
		[](GLFWwindow* window) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			w->scheduleFrame();
		}
	);
	glfwSetFramebufferSizeCallback(
		_glfw.window,
		[](GLFWwindow* window, int width, int height) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			w->scheduleResize();
		}
	);
	glfwSetWindowIconifyCallback(
		_glfw.window,
		[](GLFWwindow* window, int iconified) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));

			w->_glfw.minimized = iconified;

			if(w->_glfw.minimized) {
				// cancel pending frame, if any, on window minimalization
				if(w->_glfw.framePendingState != FramePendingState::NotPending) {
					w->_glfw.framePendingState = FramePendingState::NotPending;
					for(size_t i=0; i<glfw::framePendingWindows.size(); i++)
						if(glfw::framePendingWindows[i] == w) {
							glfw::framePendingWindows[i] = glfw::framePendingWindows.back();
							glfw::framePendingWindows.pop_back();
							break;
						}
				}
			}
			else
				// schedule frame on window un-minimalization
				w->scheduleFrame();
		}
	);
	glfwSetWindowCloseCallback(
		_glfw.window,
		[](GLFWwindow* window) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			if(w->_closeCallback)
				w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
			else {
				w->hide();
				VulkanWindow::exitMainLoop();
			}
		}
	);

	glfwSetCursorPosCallback(
		_glfw.window,
		[](GLFWwindow* window, double xpos, double ypos)
		{
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			float x = float(xpos);
			float y = float(ypos);
			if(w->_mouseState.posX != x ||
			   w->_mouseState.posY != y)
			{
				w->_mouseState.relX = x - w->_mouseState.posX;
				w->_mouseState.relY = y - w->_mouseState.posY;
				w->_mouseState.posX = x;
				w->_mouseState.posY = y;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}
		}
	);
	glfwSetMouseButtonCallback(
		_glfw.window,
		[](GLFWwindow* window, int button, int action, int mods)
		{
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			MouseButton::EnumType b;
			switch(button) {
			case GLFW_MOUSE_BUTTON_LEFT:   b = MouseButton::Left; break;
			case GLFW_MOUSE_BUTTON_RIGHT:  b = MouseButton::Right; break;
			case GLFW_MOUSE_BUTTON_MIDDLE: b = MouseButton::Middle; break;
			case GLFW_MOUSE_BUTTON_4:      b = MouseButton::X1; break;
			case GLFW_MOUSE_BUTTON_5:      b = MouseButton::X2; break;
			default: b = MouseButton::Unknown;
			}
			ButtonState buttonState = (action == GLFW_PRESS) ? ButtonState::Pressed : ButtonState::Released;
			w->_mouseState.buttons.set(b, action == GLFW_PRESS);
			w->_mouseState.modifiers.set(Modifier::Ctrl,  mods & GLFW_MOD_CONTROL);
			w->_mouseState.modifiers.set(Modifier::Shift, mods & GLFW_MOD_SHIFT);
			w->_mouseState.modifiers.set(Modifier::Alt,   mods & GLFW_MOD_ALT);
			w->_mouseState.modifiers.set(Modifier::Meta,  mods & GLFW_MOD_SUPER);
			if(w->_mouseButtonCallback)
				w->_mouseButtonCallback(*w, b, buttonState, w->_mouseState);
		}
	);
	glfwSetScrollCallback(
		_glfw.window,
		[](GLFWwindow* window, double xoffset, double yoffset) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			if(w->_mouseWheelCallback)
				w->_mouseWheelCallback(*w, -float(xoffset), float(yoffset), w->_mouseState);
		}
	);
	glfwSetKeyCallback(
		_glfw.window,
		[](GLFWwindow* window, int key, int nativeScanCode, int action, int mods) {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
			if(action != GLFW_REPEAT) {
				w->_mouseState.modifiers.set(Modifier::Ctrl,  mods & GLFW_MOD_CONTROL);
				w->_mouseState.modifiers.set(Modifier::Shift, mods & GLFW_MOD_SHIFT);
				w->_mouseState.modifiers.set(Modifier::Alt,   mods & GLFW_MOD_ALT);
				w->_mouseState.modifiers.set(Modifier::Meta,  mods & GLFW_MOD_SUPER);
				if(w->_keyCallback) {
# ifdef _WIN32
					ScanCode scanCode = translateScanCode(nativeScanCode);
# else
					ScanCode scanCode = ScanCode(nativeScanCode - 8);
# endif
					if(key >= 'A' && key <= 'Z')
						key += 32;
					w->_keyCallback(*w, (action == GLFW_PRESS) ? KeyState::Pressed : KeyState::Released, scanCode, KeyCode(key));
				}
			}
		}
	);
	glfwSetWindowPosCallback(
		_glfw.window,
		[](GLFWwindow* window, int posX, int posY) {
			if(canUpdateSavedGeometry(window)) {
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
				w->_glfw.savedPosX = posX;
				w->_glfw.savedPosY = posY;
			}
		}
	);
	glfwSetWindowSizeCallback(
		_glfw.window,
		[](GLFWwindow* window, int width, int height) {
			if(canUpdateSavedGeometry(window)) {
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
				w->_glfw.savedWidth = width;
				w->_glfw.savedHeight = height;
			}
		}
	);

	// create surface
	if(glfwCreateWindowSurface(instance, _glfw.window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface)) != VK_SUCCESS)
		throwError("glfwCreateWindowSurface");

	return _surface;

#elif defined(USE_PLATFORM_QT)

	// create QVulkanInstance
	if(qt::qVulkanInstance == nullptr) {
		qt::qVulkanInstance = reinterpret_cast<QVulkanInstance*>(&qt::qVulkanInstanceMemory);
		new(qt::qVulkanInstance) QVulkanInstance;
		qt::qVulkanInstance->setVkInstance(instance);
		qt::qVulkanInstance->create();
	}

	// setup QtRenderingWindow
	_qt.window = new QtRenderingWindow(nullptr, this);
	_qt.window->setSurfaceType(QSurface::VulkanSurface);
	_qt.window->setVulkanInstance(qt::qVulkanInstance);
	_qt.window->resize(surfaceExtent.width, surfaceExtent.height);
	_qt.window->create();

	// return Vulkan surface
	_surface = QVulkanInstance::surfaceForWindow(_qt.window);
	if(!_surface)
		throw runtime_error("VulkanWindow::init(): Failed to create surface.");
	return _surface;

#endif
}


void VulkanWindow::setDevice(VkDevice device, VkPhysicalDevice physicalDevice)
{
	assert(_instance && "VulkanWindow::setDevice(): Call VulkanWindow::create() first.");

	_physicalDevice = physicalDevice;
	_device = device;

	// initialize vkDeviceWaitIdle function pointer
	PFN_vkGetDeviceProcAddr vulkanGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
		_vulkanGetInstanceProcAddr(_instance, "vkGetDeviceProcAddr"));
	if(vulkanGetDeviceProcAddr == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkGetDeviceProcAddr function pointer.");
	_vulkanDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
		vulkanGetDeviceProcAddr(_device, "vkDeviceWaitIdle"));
	if(_vulkanDeviceWaitIdle == nullptr)
		throw runtime_error("VulkanWindow: Failed to get vkDeviceWaitIdle function pointer.");
}


void VulkanWindow::renderFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_device && "VulkanWindow::_device is null, indicating invalid VulkanWindow object. Call VulkanWindow::setDevice() before the first frame is rendered.");

	// recreate swapchain if requested
	if(_resizePending) {

		// make sure that we finished all the rendering
		// (this is necessary for swapchain re-creation)
		VkResult r =
			_vulkanDeviceWaitIdle(_device);
		if(r != VK_SUCCESS)
			throw runtime_error(string("VulkanWindow: vkDeviceWaitIdle() failed "
			                    "(return code: ") + to_string(r) + ").");

		// get surface capabilities
		// On Win32 and Xlib, currentExtent, minImageExtent and maxImageExtent of returned surfaceCapabilites are all equal.
		// It means that we can create a new swapchain only with imageExtent being equal to the window size.
		// The currentExtent might become 0,0 on Win32 and Xlib platform, for example, when the window is minimized.
		// If the currentExtent is not 0,0, both width and height must be greater than 0.
		// On Wayland, currentExtent might be 0xffffffff, 0xffffffff with the meaning that the window extent
		// will be determined by the extent of the swapchain.
		// Wayland's minImageExtent is 1,1 and maxImageExtent is the maximum supported surface size.
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		r =
			_vulkanGetPhysicalDeviceSurfaceCapabilitiesKHR(
				_physicalDevice,
				_surface,
				&surfaceCapabilities
			);
		if(r != VK_SUCCESS)
			throw runtime_error(string("VulkanWindow: vkGetPhysicalDeviceSurfaceCapabilities() failed "
			                    "(return code: ") + to_string(r) + ").");

#if defined(USE_PLATFORM_WIN32)
		_surfaceExtent = surfaceCapabilities.currentExtent;
#elif defined(USE_PLATFORM_XLIB)
		_surfaceExtent = surfaceCapabilities.currentExtent;
#elif defined(USE_PLATFORM_WAYLAND)
		// do nothing here
		// because _surfaceExtent is set in _xdgToplevelListener's configure callback
#elif defined(USE_PLATFORM_SDL3)
		// get surface size using SDL
		if(!SDL_GetWindowSizeInPixels(_sdl.window, reinterpret_cast<int*>(&_surfaceExtent.width), reinterpret_cast<int*>(&_surfaceExtent.height)))
			throw runtime_error(string("VulkanWindow: SDL_GetWindowSizeInPixels() function failed. Error details: ") + SDL_GetError());
		_surfaceExtent.width  = clamp(_surfaceExtent.width,  surfaceCapabilities.minImageExtent.width,  surfaceCapabilities.maxImageExtent.width);
		_surfaceExtent.height = clamp(_surfaceExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
#elif defined(USE_PLATFORM_SDL2)
		// get surface size using SDL
		SDL_Vulkan_GetDrawableSize(_sdl.window, reinterpret_cast<int*>(&_surfaceExtent.width), reinterpret_cast<int*>(&_surfaceExtent.height));
		_surfaceExtent.width  = clamp(_surfaceExtent.width,  surfaceCapabilities.minImageExtent.width,  surfaceCapabilities.maxImageExtent.width);
		_surfaceExtent.height = clamp(_surfaceExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
#elif defined(USE_PLATFORM_GLFW)
		// get surface size using GLFW
		// (0xffffffff values might be returned on Wayland)
		if(surfaceCapabilities.currentExtent.width != 0xffffffff && surfaceCapabilities.currentExtent.height != 0xffffffff)
			_surfaceExtent = surfaceCapabilities.currentExtent;
		else {
			glfwGetFramebufferSize(_glfw.window, reinterpret_cast<int*>(&_surfaceExtent.width), reinterpret_cast<int*>(&_surfaceExtent.height));
			checkError("glfwGetFramebufferSize");
			_surfaceExtent.width  = clamp(_surfaceExtent.width,  surfaceCapabilities.minImageExtent.width,  surfaceCapabilities.maxImageExtent.width);
			_surfaceExtent.height = clamp(_surfaceExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		}
#elif defined(USE_PLATFORM_QT)
		// get surface size using Qt
		// (0xffffffff values might be returned on Wayland)
		if(surfaceCapabilities.currentExtent.width != 0xffffffff && surfaceCapabilities.currentExtent.height != 0xffffffff)
			_surfaceExtent = surfaceCapabilities.currentExtent;
		else {
			QSize size = _qt.window->size();
			auto ratio = _qt.window->devicePixelRatio();
			_surfaceExtent = VkExtent2D{uint32_t(float(size.width()) * ratio + 0.5f), uint32_t(float(size.height()) * ratio + 0.5f)};
			_surfaceExtent.width  = clamp(_surfaceExtent.width,  surfaceCapabilities.minImageExtent.width,  surfaceCapabilities.maxImageExtent.width);
			_surfaceExtent.height = clamp(_surfaceExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		}
		cout << "New Qt window size in device independent pixels: " << _qt.window->width() << "x" << _qt.window->height()
		     << ", in physical pixels: " << _surfaceExtent.width << "x" << _surfaceExtent.height << endl;
#endif

		// zero size swapchain is not allowed,
		// so we will repeat the resize and rendering attempt after the next window resize
		// (this may happen on Win32-based and Xlib-based systems, for instance;
		// in reality, it never happened on my KDE 5.80.0 (Kubuntu 21.04) and KDE 5.44.0 (Kubuntu 18.04.5)
		// because window minimalizing just unmaps the window)
		if(_surfaceExtent.width == 0 && _surfaceExtent.height == 0)
			return;  // new frame will be scheduled on the next window resize

		// recreate swapchain
		_resizePending = false;
		_resizeCallback(*this, surfaceCapabilities, _surfaceExtent);
	}

	// render scene
#if !defined(USE_PLATFORM_QT)
	_frameCallback(*this);
#else
# if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	qt::qVulkanInstance->presentAboutToBeQueued(_qt.window);
# endif
	_frameCallback(*this);
	qt::qVulkanInstance->presentQueued(_qt.window);
#endif
}


#if defined(USE_PLATFORM_WIN32)


bool VulkanWindow::isVisible() const
{
	return _win32.hwnd && _win32.visible;
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	// show window
	ShowWindow(HWND(_win32.hwnd), SW_SHOW);
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// hide window
	ShowWindow(HWND(_win32.hwnd), SW_HIDE);
}


void VulkanWindow::mainLoop()
{
	// run Win32 event loop
	MSG msg;
	BOOL r;
	thrownException = nullptr;
	while((r = GetMessage(&msg, NULL, 0, 0)) != 0) {

		// handle errors
		if(r == -1)
			throw runtime_error("GetMessage(): The function failed.");

		// handle message
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		// handle exceptions raised in window procedure
		if(thrownException)
			rethrow_exception(thrownException);

	}
}


// window's message handling procedure
LRESULT VulkanWindowPrivate::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
try {
	// mouse functions
	auto handleModifiers =
		[](VulkanWindowPrivate* w, WPARAM wParam) -> void
		{
			w->_mouseState.modifiers.set(Modifier::Ctrl,  wParam & MK_CONTROL);
			w->_mouseState.modifiers.set(Modifier::Shift, wParam & MK_SHIFT);
			w->_mouseState.modifiers.set(Modifier::Alt,   GetKeyState(VK_MENU) < 0);
			w->_mouseState.modifiers.set(Modifier::Meta,  GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN));
		};
	auto handleMouseMove =
		[](VulkanWindowPrivate* w, float x, float y)
		{
			if(x != w->_mouseState.posX || y != w->_mouseState.posY) {
				w->_mouseState.relX = x - w->_mouseState.posX;
				w->_mouseState.relY = y - w->_mouseState.posY;
				w->_mouseState.posX = x;
				w->_mouseState.posY = y;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}
		};
	auto handleMouseButton =
		[](HWND hwnd, MouseButton::EnumType mouseButton, ButtonState buttonState, WPARAM wParam, LPARAM lParam) -> LRESULT
		{
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));

			// handle modifiers
			w->_mouseState.modifiers.set(Modifier::Ctrl,  wParam & MK_CONTROL);
			w->_mouseState.modifiers.set(Modifier::Shift, wParam & MK_SHIFT);
			w->_mouseState.modifiers.set(Modifier::Alt,   GetKeyState(VK_MENU) < 0);
			w->_mouseState.modifiers.set(Modifier::Meta,  GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN));

			// handle mouse move, if any
			float x = float(GET_X_LPARAM(lParam));
			float y = float(GET_Y_LPARAM(lParam));
			if(x != w->_mouseState.posX || y != w->_mouseState.posY) {
				w->_mouseState.relX = x - w->_mouseState.posX;
				w->_mouseState.relY = y - w->_mouseState.posY;
				w->_mouseState.posX = x;
				w->_mouseState.posY = y;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}

			// set new state and capture mouse
			bool prevAny = w->_mouseState.buttons.any();
			w->_mouseState.buttons.set(mouseButton, buttonState==ButtonState::Pressed);
			bool newAny = w->_mouseState.buttons.any();
			if(prevAny != newAny) {
				if(newAny)
					SetCapture(hwnd);
				else
					ReleaseCapture();
			}

			// callback
			if(w->_mouseButtonCallback)
				w->_mouseButtonCallback(*w, mouseButton, buttonState, w->_mouseState);

			return 0;
		};
	auto getMouseXButton =
		[](WPARAM wParam) -> MouseButton::EnumType
		{
			switch(GET_XBUTTON_WPARAM(wParam)) {
			case XBUTTON1: return MouseButton::X1;
			case XBUTTON2: return MouseButton::X2;
			default: return MouseButton::Unknown;
			}
		};

	switch(msg)
	{
#if 0  // window resize looks more nice with backgroud erasing, so we are not ignoring the message;
       // The message is sent on window show, resize and move, in general. So, it is not a performance issue.
		case WM_ERASEBKGND:
			return 1;  // returning non-zero means that background should be considered erased
#endif

		// paint the window message
		// (we render the window content here)
		case WM_PAINT: {

			// render all windows in framePendingWindows list
			// (if a window keeps its window area invalidated,
			// WM_PAINT might be constantly received by this single window only,
			// starving all other windows => we render all windows in framePendingWindows list)
			VulkanWindow* window = reinterpret_cast<VulkanWindow*>(GetWindowLongPtr(hwnd, 0));
			bool windowProcessed = false;
			for(size_t i=0; i<win32::framePendingWindows.size(); ) {

				VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(win32::framePendingWindows[i]);
				if(w == window)
					windowProcessed = true;

				// render frame
				w->_win32.framePendingState = FramePendingState::TentativePending;
				w->renderFrame();

				// was frame scheduled again?
				// (it might be rescheduled again in renderFrame())
				if(w->_win32.framePendingState == FramePendingState::TentativePending) {

					// validate window area
					if(!ValidateRect(HWND(w->_win32.hwnd), NULL))
						thrownException = make_exception_ptr(runtime_error("ValidateRect(): The function failed."));

					// update state to no-frame-pending
					w->_win32.framePendingState = FramePendingState::NotPending;
					if(win32::framePendingWindows.size() == 1) {
						win32::framePendingWindows.clear();  // all iterators are invalidated
						break;
					}
					else {
						win32::framePendingWindows[i] = win32::framePendingWindows.back();
						win32::framePendingWindows.pop_back();  // end() iterator is invalidated
						continue;
					}
				}
				i++;

			}

			// if window was not in framePendingWindows, process it
			if(!windowProcessed) {

				// validate window area
				if(!ValidateRect(hwnd, NULL))
					thrownException = make_exception_ptr(runtime_error("ValidateRect(): The function failed."));

				// render frame
				// (_framePendingState is No, but scheduleFrame() might be called inside renderFrame())
				window->renderFrame();

			}

			return 0;
		}

		// mouse move
		case WM_MOUSEMOVE: {
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_win32.titleBarLeftButtonDownMsgOnHold) {
				// workaround for WM_NCLBUTTONDOWN window freeze
				w->_win32.titleBarLeftButtonDownMsgOnHold = false;
				DefWindowProcW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, w->_win32.titleBarLeftButtonDownPos);
				return DefWindowProcW(hwnd, msg, wParam, lParam);
			}
			handleModifiers(w, wParam);
			handleMouseMove(w, float(GET_X_LPARAM(lParam)), float(GET_Y_LPARAM(lParam)));
			return 0;
		}

		// mouse buttons
		case WM_LBUTTONDOWN: return handleMouseButton(hwnd, MouseButton::Left,   ButtonState::Pressed,  wParam, lParam);
		case WM_LBUTTONUP:   return handleMouseButton(hwnd, MouseButton::Left,   ButtonState::Released, wParam, lParam);
		case WM_RBUTTONDOWN: return handleMouseButton(hwnd, MouseButton::Right,  ButtonState::Pressed,  wParam, lParam);
		case WM_RBUTTONUP:   return handleMouseButton(hwnd, MouseButton::Right,  ButtonState::Released, wParam, lParam);
		case WM_MBUTTONDOWN: return handleMouseButton(hwnd, MouseButton::Middle, ButtonState::Pressed,  wParam, lParam);
		case WM_MBUTTONUP:   return handleMouseButton(hwnd, MouseButton::Middle, ButtonState::Released, wParam, lParam);
		case WM_XBUTTONDOWN: return handleMouseButton(hwnd, getMouseXButton(wParam), ButtonState::Pressed,  wParam, lParam);
		case WM_XBUTTONUP:   return handleMouseButton(hwnd, getMouseXButton(wParam), ButtonState::Released, wParam, lParam);

		// left mouse button down on non-client window area message
		// (application freeze for several hundred of milliseconds is workarounded here)
		case WM_NCLBUTTONDOWN: {
			// This is a workaround for window freeze for several hundred of milliseconds
			// if you left-click on its title bar. The clicking on the title bar
			// makes execution of DefWindowProc to not return for several hundreds of milliseconds,
			// making the application frozen for this time period.
			// We workaround the problem by delaying WM_NCLBUTTONDOWN message until further related mouse event.
			// Processing both messages at once avoids the freeze.
			if(wParam == HTCAPTION) {
				VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
				w->_win32.titleBarLeftButtonDownMsgOnHold = true;
				w->_win32.titleBarLeftButtonDownPos = lParam;
				return 0;
			}
			else
				return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// workaround for WM_NCLBUTTONDOWN window freeze
		case WM_NCMOUSEMOVE: {

			// if WM_NCLBUTTONDOWN is not on hold, just process the current message
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(!w->_win32.titleBarLeftButtonDownMsgOnHold)
				return DefWindowProcW(hwnd, msg, wParam, lParam);

			// skip WM_NCMOUSEMOVE with the same coordinates
			if(w->_win32.titleBarLeftButtonDownPos == lParam)
				return 0;

			// process WM_NCLBUTTONDOWN and WM_NCMOUSEMOVE
			w->_win32.titleBarLeftButtonDownMsgOnHold = false;
			DefWindowProcW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, w->_win32.titleBarLeftButtonDownPos);
			return DefWindowProcW(hwnd, WM_NCMOUSEMOVE, wParam, lParam);

		}

		// workaround for WM_NCLBUTTONDOWN window freeze
		case WM_NCLBUTTONUP: {
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_win32.titleBarLeftButtonDownMsgOnHold) {
				w->_win32.titleBarLeftButtonDownMsgOnHold = false;
				DefWindowProcW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, w->_win32.titleBarLeftButtonDownPos);
			}
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// vertical and horizontal mouse wheel
		// (amount of wheel rotation since the last wheel message)
		case WM_MOUSEWHEEL: {
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			handleModifiers(w, wParam);
			POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if(ScreenToClient(hwnd, &p) == 0)
				thrownException = make_exception_ptr(runtime_error("ScreenToClient(): The function failed."));
			handleMouseMove(w, float(p.x), float(p.y));
			if(w->_mouseWheelCallback)
				w->_mouseWheelCallback(*w, 0, float(GET_WHEEL_DELTA_WPARAM(wParam)) / 120.f, w->_mouseState);
			return 0;
		}
		case WM_MOUSEHWHEEL: {
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			handleModifiers(w, wParam);
			POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if(ScreenToClient(hwnd, &p) == 0)
				thrownException = make_exception_ptr(runtime_error("ScreenToClient(): The function failed."));
			handleMouseMove(w, float(p.x), float(p.y));
			if(w->_mouseWheelCallback)
				w->_mouseWheelCallback(*w, float(GET_WHEEL_DELTA_WPARAM(wParam)) / 120.f, 0, w->_mouseState);
			return 0;
		}

		// keyboard events
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:  // LeftAlt, RightAlt and F10 come through WM_SYSKEYDOWN
		{
			// skip auto-repeat messages
			if(lParam & (1 << 30))
				return 0;

			// handle Alt-F4
			if((lParam & 0xff0000) == (LPARAM(62) << 16) && GetKeyState(VK_MENU) < 0)
				return DefWindowProcW(hwnd, msg, wParam, lParam);

			// callback
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_keyCallback)
			{
				// scan code
				unsigned nativeScanCode = (lParam >> 16) & 0x1ff;
				ScanCode scanCode = translateScanCode(nativeScanCode);
				if(scanCode == ScanCode::Unknown)
					scanCode = getScanCodeOfSpecialKey(wParam);

				// key code
				KeyCode keyCode;
				if(nativeScanCode < 128)
					keyCode = win32::keyConversionTable[nativeScanCode];
				else
				{
					UINT nativeKeyCode = wParam & 0xff;
					UINT wch16 = (MapVirtualKeyW(nativeKeyCode, MAPVK_VK_TO_CHAR) & 0xffff);
					keyCode = wchar16ToKeyCode(wch16);
				}

				// callback
				w->_keyCallback(*w, KeyState::Pressed, scanCode, keyCode);
			}
			return 0;
		}
		case WM_KEYUP:
		case WM_SYSKEYUP:  // LeftAlt, RightAlt and F10 come through WM_SYSKEYDOWN
		{
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_keyCallback)
			{
				// scan code
				unsigned nativeScanCode = (lParam >> 16) & 0x1ff;
				ScanCode scanCode = translateScanCode(nativeScanCode);
				if(scanCode == ScanCode::Unknown)
					scanCode = getScanCodeOfSpecialKey(wParam);

				// key code
				KeyCode key;
				if(nativeScanCode < 128)
					key = win32::keyConversionTable[nativeScanCode];
				else
				{
					UINT nativeKeyCode = wParam & 0xff;
					UINT wch16 = (MapVirtualKeyW(nativeKeyCode, MAPVK_VK_TO_CHAR) & 0xffff);
					key = wchar16ToKeyCode(wch16);
				}

				// callback
				w->_keyCallback(*w, KeyState::Released, scanCode, key);
			}
			return 0;
		}

		case WM_INPUTLANGCHANGE: {
			initKeyConversionTable();
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// window resize message
		// (we schedule swapchain resize here)
		case WM_SIZE: {
			cout << "WM_SIZE message (" << LOWORD(lParam) << "x" << HIWORD(lParam) << ")" << endl;
			if(LOWORD(lParam) != 0 && HIWORD(lParam) != 0) {
				VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
				w->scheduleResize();
			}
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// window show and hide message
		// (we set _visible variable here)
		case WM_SHOWWINDOW: {
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			w->_win32.visible = wParam==TRUE;
			if(w->_win32.visible == false) {

				// store frame pending state
				w->_win32.hiddenWindowFramePending = w->_win32.framePendingState == FramePendingState::Pending;

				// cancel WM_NCLBUTTONDOWN hold operation, if active
				w->_win32.titleBarLeftButtonDownMsgOnHold = false;

				// cancel frame pending state
				if(w->_win32.framePendingState == FramePendingState::Pending) {
					w->_win32.framePendingState = FramePendingState::NotPending;
					removeFromFramePendingWindows(w);
				}

			}
			else {

				// restore frame pending state
				if(w->_win32.hiddenWindowFramePending) {
					w->_win32.framePendingState = FramePendingState::Pending;
					win32::framePendingWindows.push_back(w);
				}

			}
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// close window message
		// (we call _closeCallback here if registered,
		// otherwise we hide the window and schedule main loop exit)
		case WM_CLOSE: {
			cout << "WM_CLOSE message" << endl;
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_closeCallback)
				w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
			else {
				w->hide();
				VulkanWindow::exitMainLoop();
			}
			return 0;
		}

		// destroy window message
		// (we make sure that the window is not in framePendingWindows list)
		case WM_DESTROY: {

			cout << "WM_DESTROY message" << endl;

			// remove frame pending state
			VulkanWindowPrivate* w = reinterpret_cast<VulkanWindowPrivate*>(GetWindowLongPtr(hwnd, 0));
			if(w->_win32.framePendingState != FramePendingState::NotPending) {
				w->_win32.framePendingState = FramePendingState::NotPending;
				removeFromFramePendingWindows(w);
			}

			return 0;
		}

		// all other messages are handled by standard DefWindowProc()
		default:
			return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
}
catch(...) {
	if(!thrownException)
		thrownException = std::current_exception();
	return 0;
}


void VulkanWindow::exitMainLoop()
{
	PostQuitMessage(0);
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_win32.framePendingState == FramePendingState::Pending)
		return;

	if(_win32.framePendingState == FramePendingState::NotPending) {

		// invalidate window content (this will cause WM_PAINT message to be sent)
		if(!InvalidateRect(HWND(_win32.hwnd), NULL, FALSE))
			throw runtime_error("InvalidateRect(): The function failed.");

		win32::framePendingWindows.push_back(this);
	}

	_win32.framePendingState = FramePendingState::Pending;
}


#elif defined(USE_PLATFORM_XLIB)


bool VulkanWindow::isVisible() const
{
	return _xlib.window && _xlib.visible;
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	if(_xlib.visible)
		return;

	// show window
	_xlib.visible = true;
	_xlib.iconVisible = true;
	_xlib.fullyObscured = false;
	XMapWindow(xlib::display, _xlib.window);
	if(_xlib.minimized)
		XIconifyWindow(xlib::display, _xlib.window, XDefaultScreen(xlib::display));
	else
		scheduleFrame();
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(!_xlib.visible && !_xlib.iconVisible)
		return;

	_xlib.visible = false;
	_xlib.iconVisible = false;
	_xlib.fullyObscured = true;

	// update _minimized
	updateMinimized();

	// unmap window and hide taskbar icon
	XWithdrawWindow(xlib::display, _xlib.window, XDefaultScreen(xlib::display));

	// delete all Expose events and cancel any pending frames
	XEvent tmp;
	while(XCheckTypedWindowEvent(xlib::display, _xlib.window, Expose, &tmp) == True);
	_xlib.framePending = false;
}


void VulkanWindow::updateMinimized()
{
	// read the new value of WM_STATE property
	unsigned char* data;
	Atom actualType;
	int actualFormat;
	unsigned long itemsRead;
	unsigned long bytesAfter;
	if(XGetWindowProperty(
			xlib::display,  // display
			_xlib.window,  // window
			xlib::wmStateProperty,  // property
			0, 1,  // long_offset, long_length
			False,  // delete
			xlib::wmStateProperty, // req_type
			&actualType,  // actual_type_return
			&actualFormat,  // actual_format_return
			&itemsRead,  // nitems_return
			&bytesAfter,  // bytes_after_return
			&data  // prop_return
		) == Success && itemsRead > 0)
	{
		cout << "New WM_STATE: " << *reinterpret_cast<unsigned*>(data) << endl;
		_xlib.minimized = *reinterpret_cast<unsigned*>(data) == 3;
		XFree(data);
	}
	else
		cout << "WM_STATE reading failed" << endl;
}


void VulkanWindow::mainLoop()
{
	// mouse functions
	auto handleModifiers =
		[](VulkanWindow* w, unsigned int state)
		{
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Ctrl,  state & ControlMask);
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Shift, state & ShiftMask);
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Alt,   state & (Mod1Mask|Mod5Mask));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Meta,  state & Mod4Mask);
		};
	auto handleMouseMove =
		[](VulkanWindow* w, float newX, float newY)
		{
			if(w->_mouseState.posX != newX ||
				w->_mouseState.posY != newY)
			{
				w->_mouseState.relX = newX - w->_mouseState.posX;
				w->_mouseState.relY = newY - w->_mouseState.posY;
				w->_mouseState.posX = newX;
				w->_mouseState.posY = newY;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}
		};
	auto getMouseButton =
		[](unsigned int button) -> MouseButton::EnumType
		{
			switch(button) {
			case Button1: return MouseButton::Left;
			case Button2: return MouseButton::Middle;
			case Button3: return MouseButton::Right;
			case 8: return MouseButton::X1;
			case 9: return MouseButton::X2;
			default: return MouseButton::Unknown;
			}
		};

	// run Xlib event loop
	XEvent e;
	xlib::running = true;
	while(xlib::running) {

		// get event
		XNextEvent(xlib::display, &e);

		// get VulkanWindow
		// (we use std::map because per-window data using XGetWindowProperty() would require X-server roundtrip)
		auto it = xlib::vulkanWindowMap.find(e.xany.window);
		if(it == xlib::vulkanWindowMap.end())
			continue;
		VulkanWindow* w = it->second;

		// expose event
		if(e.type == Expose)
		{
			// remove all other Expose events
			XEvent tmp;
			while(XCheckTypedWindowEvent(xlib::display, w->_xlib.window, Expose, &tmp) == True);

			// perform rendering
			w->_xlib.framePending = false;
			w->renderFrame();
			continue;
		}

		// configure event
		if(e.type == ConfigureNotify) {
			if(e.xconfigure.width != int(w->_surfaceExtent.width) || e.xconfigure.height != int(w->_surfaceExtent.height)) {
				cout << "Configure event " << e.xconfigure.width << "x" << e.xconfigure.height << endl;
				w->scheduleResize();
			}
			continue;
		}

		// mouse events
		if(e.type == MotionNotify) {
			handleModifiers(w, e.xmotion.state);
			handleMouseMove(w, float(e.xmotion.x), float(e.xmotion.y));
			continue;
		}
		if(e.type == ButtonPress) {
			cout << "state: " << e.xbutton.state << endl;
			handleModifiers(w, e.xbutton.state);
			handleMouseMove(w, float(e.xbutton.x), float(e.xbutton.y));
			if(e.xbutton.button < Button4 || e.xbutton.button > 7) {
				MouseButton::EnumType button = getMouseButton(e.xbutton.button);
				w->_mouseState.buttons.set(button, true);
				if(w->_mouseButtonCallback)
					w->_mouseButtonCallback(*w, button, ButtonState::Pressed, w->_mouseState);
			}
			else {
				float wheelX, wheelY;
				if(e.xbutton.button <= Button5) {
					wheelX = 0.f;
					wheelY = (e.xbutton.button == Button5) ? -1.f : 1.f;
				}
				else {
					wheelX = (e.xbutton.button == 6) ? -1.f : 1.f;
					wheelY = 0.f;
				}
				if(w->_mouseWheelCallback)
					w->_mouseWheelCallback(*w, wheelX, wheelY, w->_mouseState);
			}
			continue;
		}
		if(e.type == ButtonRelease) {
			handleModifiers(w, e.xbutton.state);
			handleMouseMove(w, float(e.xbutton.x), float(e.xbutton.y));
			if(e.xbutton.button < Button4 || e.xbutton.button > 7) {
				MouseButton::EnumType button = getMouseButton(e.xbutton.button);
				w->_mouseState.buttons.set(button, false);
				if(w->_mouseButtonCallback)
					w->_mouseButtonCallback(*w, button, ButtonState::Released, w->_mouseState);
			}
			continue;
		}

		// keyboard events
		if(e.type == KeyPress)
		{
			// callback
			if(w->_keyCallback)
			{
				// get scan code
				ScanCode scanCode = ScanCode(e.xkey.keycode - 8);

				// get utf32 character representing the keyboard key
				KeySym keySym;
				e.xkey.state &= ~(ShiftMask | LockMask | ControlMask |  // ignore shift state, Caps Lock and Ctrl
				                  Mod1Mask |  // ignore Alt
				                  Mod2Mask |  // ignore Num Lock
				                  Mod3Mask |  // ignore Scroll Lock
				                  Mod4Mask |  // ignore WinKey  );
				                  Mod5Mask);  // ignore unknown modifier
				XLookupString(&e.xkey, nullptr, 0, &keySym, nullptr);
				uint32_t codePoint = xkb_keysym_to_utf32(keySym);

				// callback
				w->_keyCallback(*w, KeyState::Pressed, scanCode, KeyCode(codePoint));
			}
			continue;
		}
		if(e.type == KeyRelease)
		{
			// skip auto-repeat key events
			if(XEventsQueued(xlib::display, QueuedAfterReading)) {
				XEvent nextEvent;
				XPeekEvent(xlib::display, &nextEvent);
				if(nextEvent.type == KeyPress && nextEvent.xkey.time == e.xkey.time &&
				   nextEvent.xkey.keycode == e.xkey.keycode)
				{
					XNextEvent(xlib::display, &nextEvent);
					continue;
				}
			}

			// callback
			if(w->_keyCallback)
			{
				// get scan code
				ScanCode scanCode = ScanCode(e.xkey.keycode - 8);

				// get utf32 character representing the keyboard key
				KeySym keySym;
				e.xkey.state &= ~(ShiftMask | LockMask | ControlMask |  // ignore shift state, Caps Lock and Ctrl
				                  Mod1Mask |  // ignore Alt
				                  Mod2Mask |  // ignore Num Lock
				                  Mod3Mask |  // ignore Scroll Lock
				                  Mod4Mask |  // ignore WinKey  );
				                  Mod5Mask);  // ignore unknown modifier
				XLookupString(&e.xkey, nullptr, 0, &keySym, nullptr);
				uint32_t codePoint = xkb_keysym_to_utf32(keySym);

				// callback
				w->_keyCallback(*w, KeyState::Released, scanCode, KeyCode(codePoint));
			}
			continue;
		}

		// map, unmap, obscured, unobscured
		if(e.type == MapNotify)
		{
			cout<<"MapNotify"<<endl;
			if(w->_xlib.visible)
				continue;
			w->_xlib.visible = true;
			w->_xlib.fullyObscured = false;
			w->scheduleFrame();
			continue;
		}
		if(e.type == UnmapNotify)
		{
			cout<<"UnmapNotify"<<endl;
			if(w->_xlib.visible == false)
				continue;
			w->_xlib.visible = false;
			w->_xlib.fullyObscured = true;

			XEvent tmp;
			while(XCheckTypedWindowEvent(xlib::display, w->_xlib.window, Expose, &tmp) == True);
			w->_xlib.framePending = false;
			continue;
		}
		if(e.type == VisibilityNotify) {
			if(e.xvisibility.state != VisibilityFullyObscured)
			{
				cout << "Window not fully obscured" << endl;
				w->_xlib.fullyObscured = false;
				w->scheduleFrame();
				continue;
			}
			else {
				cout << "Window fully obscured" << endl;
				w->_xlib.fullyObscured = true;
				XEvent tmp;
				while(XCheckTypedWindowEvent(xlib::display, w->_xlib.window, Expose, &tmp) == True);
				w->_xlib.framePending = false;
				continue;
			}
		}

		// minimize state
		if(e.type == PropertyNotify) {
			if(e.xproperty.atom == xlib::wmStateProperty && e.xproperty.state == PropertyNewValue)
				w->updateMinimized();
			continue;
		}

		// handle window close
		if(e.type==ClientMessage && ulong(e.xclient.data.l[0])==xlib::wmDeleteMessage) {
			if(w->_closeCallback)
				w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
			else {
				w->hide();
				VulkanWindow::exitMainLoop();
			}
			continue;
		}
	}
}


void VulkanWindow::exitMainLoop()
{
	xlib::running = false;
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_xlib.framePending || !_xlib.visible || _xlib.fullyObscured)
		return;

	_xlib.framePending = true;

	XSendEvent(
		xlib::display,  // display
		_xlib.window,  // w
		False,  // propagate
		ExposureMask,  // event_mask
		(XEvent*)(&(const XExposeEvent&)  // event_send
			XExposeEvent{
				Expose,  // type
				0,  // serial
				True,  // send_event
				xlib::display,  // display
				_xlib.window,  // window
				0, 0,  // x, y
				0, 0,  // width, height
				0  // count
			})
	);
}


#elif defined(USE_PLATFORM_WAYLAND)


bool VulkanWindow::isVisible() const
{
	return _wayland.wlSurface && (_wayland.xdgSurface || _wayland.libdecorFrame);
}


void VulkanWindow::show()
{
	show([](VulkanWindow&){}, [](VulkanWindow&){});
}


void VulkanWindow::show(void (*xdgConfigFunc)(VulkanWindow&), void (*libdecorConfigFunc)(VulkanWindow&))
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	// check for already shown window
	if(_wayland.xdgSurface || _wayland.libdecorFrame)
		return;

	if(wayland::libdecorContext)
	{

		// create libdecor decorations
		_wayland.libdecorFrame = wayland::funcs.libdecor_decorate(wayland::libdecorContext, _wayland.wlSurface, &libdecorFrameInterface, this);
		if(_wayland.libdecorFrame == nullptr)
			throw runtime_error("VulkanWindow::show(): libdecor_decorate() failed.");
		wayland::funcs.libdecor_frame_set_title(_wayland.libdecorFrame, _title.c_str());
		libdecorConfigFunc(*this);
		wayland::funcs.libdecor_frame_map(_wayland.libdecorFrame);

	}
	else
	{

		// create xdg surface
		_wayland.xdgSurface = xdg_wm_base_get_xdg_surface(wayland::xdgWmBase, _wayland.wlSurface);
		if(_wayland.xdgSurface == nullptr)
			throw runtime_error("xdg_wm_base_get_xdg_surface() failed.");
		if(xdg_surface_add_listener(_wayland.xdgSurface, &xdgSurfaceListener, this))
			throw runtime_error("xdg_surface_add_listener() failed.");

		// create xdg toplevel
		_wayland.xdgTopLevel = xdg_surface_get_toplevel(_wayland.xdgSurface);
		if(_wayland.xdgTopLevel == nullptr)
			throw runtime_error("xdg_surface_get_toplevel() failed.");
		if(wayland::zxdgDecorationManagerV1) {
			_wayland.decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wayland::zxdgDecorationManagerV1, _wayland.xdgTopLevel);
			zxdg_toplevel_decoration_v1_set_mode(_wayland.decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
		xdg_toplevel_set_title(_wayland.xdgTopLevel, _title.c_str());
		if(xdg_toplevel_add_listener(_wayland.xdgTopLevel, &xdgToplevelListener, this))
			throw runtime_error("xdg_toplevel_add_listener() failed.");
		xdgConfigFunc(*this);
		wl_surface_commit(_wayland.wlSurface);

	}

	// send callback
	wl_callback* callback = wl_display_sync(wayland::display);
	if(wl_callback_add_listener(callback, &syncListener, this))
		throw runtime_error("wl_callback_add_listener() failed.");
	_wayland.numSyncEventsOnTheFly++;
	wl_display_flush(wayland::display);

	_wayland.forcedFrame = true;
	_resizePending = true;
}


void VulkanWindowPrivate::xdgToplevelListenerConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states)
{
	cout << "toplevel configure (width=" << width << ", height=" << height << ")" << endl;

	// update window state
	// (following for loop is equivalent to wl_array_for_each(s, states) used in C)
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	bool fullscreen = false;
	bool maximized = false;
	uint32_t* p;
	for(p = static_cast<uint32_t*>(states->data);
			reinterpret_cast<char*>(p) < static_cast<char*>(states->data) + states->size;
			p++)
	{
		switch(*p) {
		case XDG_TOPLEVEL_STATE_MAXIMIZED: maximized = true; break;
		case XDG_TOPLEVEL_STATE_FULLSCREEN: fullscreen = true; break;
		}
	}
	if(fullscreen)
		w->_wayland.windowState = WindowState::FullScreen;
	else if(maximized)
		w->_wayland.windowState = WindowState::Maximized;
	else
		w->_wayland.windowState = WindowState::Normal;

	// if width or height of the window changed,
	// schedule swapchain resize and force new frame rendering
	// (width and height of zero means that the compositor does not know the window dimension)
	if(uint32_t(width) != w->_surfaceExtent.width && width != 0) {
		w->_surfaceExtent.width = width;
		if(uint32_t(height) != w->_surfaceExtent.height && height != 0)
			w->_surfaceExtent.height = height;
		w->scheduleResize();
	}
	else if(uint32_t(height) != w->_surfaceExtent.height && height != 0) {
		w->_surfaceExtent.height = height;
		w->scheduleResize();
	}
}


void VulkanWindowPrivate::xdgSurfaceListenerConfigure(void* data, xdg_surface* xdgSurface, uint32_t serial)
{
	cout << "surface configure" << endl;
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	xdg_surface_ack_configure(xdgSurface, serial);
	wl_surface_commit(w->_wayland.wlSurface);

	// we need to explicitly generate the first frame
	// otherwise the window is not shown
	// and no frame callbacks will be delivered through _frameListener
	if(w->_wayland.forcedFrame) {
		w->_wayland.forcedFrame = false;
		w->renderFrame();
	}
}


void VulkanWindowPrivate::libdecorFrameConfigure(libdecor_frame* frame, libdecor_configuration* config, void* data)
{
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);

	// update window state
	libdecor_window_state s;
	if(wayland::funcs.libdecor_configuration_get_window_state(config, &s)) {
		if(s & LIBDECOR_WINDOW_STATE_FULLSCREEN)
			w->_wayland.windowState = WindowState::FullScreen;
		else if(s & LIBDECOR_WINDOW_STATE_MAXIMIZED)
			w->_wayland.windowState = WindowState::Maximized;
		else
			w->_wayland.windowState = WindowState::Normal;
	}
	else
		throw runtime_error("libdecor_configuration_get_window_state() failed.");

	// if width or height of the window changed,
	// schedule swapchain resize and force new frame rendering
	int width, height;
	if(wayland::funcs.libdecor_configuration_get_content_size(config, frame, &width, &height))
	{
		if(uint32_t(width) != w->_surfaceExtent.width && width != 0) {
			w->_surfaceExtent.width = width;
			if(uint32_t(height) != w->_surfaceExtent.height && height != 0)
				w->_surfaceExtent.height = height;
			w->scheduleResize();
		}
		else if(uint32_t(height) != w->_surfaceExtent.height && height != 0) {
			w->_surfaceExtent.height = height;
			w->scheduleResize();
		}
	}

	cout << "libdecor configure: " << w->_surfaceExtent.width <<"x" << w->_surfaceExtent.height << endl;

	// set new window state
	libdecor_state* state = wayland::funcs.libdecor_state_new(w->_surfaceExtent.width, w->_surfaceExtent.height);
	wayland::funcs.libdecor_frame_commit(frame, state, config);
	wayland::funcs.libdecor_state_free(state);

	// we need to explicitly generate the first frame
	// otherwise the window is not shown
	// and no frame callbacks will be delivered through _frameListener
	if(w->_wayland.forcedFrame) {
		w->_wayland.forcedFrame = false;
		w->renderFrame();
	}
}


void VulkanWindowPrivate::libdecorFrameCommit(libdecor_frame* frame, void* data)
{
	cout << "libdecor commit" << endl;
	wl_surface_commit(static_cast<VulkanWindowPrivate*>(data)->_wayland.wlSurface);
}


void VulkanWindowPrivate::libdecorError(libdecor* context, libdecor_error error, const char* message)
{
	throw runtime_error("VulkanWindow: libdecor error " + to_string(error) + ": \"" + message + "\"");
}


void VulkanWindowPrivate::libdecorFrameDismissPopup(libdecor_frame* frame, const char* seatName, void* data)
{
}


void VulkanWindowPrivate::xdgToplevelListenerClose(void* data, xdg_toplevel* xdgTopLevel)
{
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	if(w->_closeCallback)
		w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
	else {
		w->hide();
		VulkanWindow::exitMainLoop();
	}

	// update window state
	if(w->_wayland.xdgTopLevel == nullptr)
		w->_wayland.windowState = WindowState::Hidden;
}


void VulkanWindowPrivate::libdecorFrameClose(libdecor_frame* frame, void* data)
{
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	if(w->_closeCallback)
		w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
	else {
		w->hide();
		VulkanWindow::exitMainLoop();
	}

	// update window state
	if(w->_wayland.libdecorFrame == nullptr)
		w->_wayland.windowState = WindowState::Hidden;
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// destroy xdg toplevel
	// and associated objects
	_wayland.forcedFrame = false;
	if(_wayland.scheduledFrameCallback) {
		wl_callback_destroy(_wayland.scheduledFrameCallback);
		_wayland.scheduledFrameCallback = nullptr;
	}
	if(_wayland.libdecorFrame) {
		wayland::funcs.libdecor_frame_unref(_wayland.libdecorFrame);
		_wayland.libdecorFrame = nullptr;
		wl_surface_attach(_wayland.wlSurface, NULL, 0, 0);
		wl_surface_commit(_wayland.wlSurface);
	}
	if(_wayland.decoration) {
		zxdg_toplevel_decoration_v1_destroy(_wayland.decoration);
		_wayland.decoration = nullptr;
	}
	if(_wayland.xdgTopLevel) {
		xdg_toplevel_destroy(_wayland.xdgTopLevel);
		_wayland.xdgTopLevel = nullptr;
	}
	if(_wayland.xdgSurface) {
		xdg_surface_destroy(_wayland.xdgSurface);
		_wayland.xdgSurface = nullptr;
		wl_surface_attach(_wayland.wlSurface, nullptr, 0, 0);
		wl_surface_commit(_wayland.wlSurface);
	}
	_wayland.windowState = WindowState::Hidden;
}


void VulkanWindow::mainLoop()
{
	// flush outgoing buffers
	cout << "Entering main loop." << endl;
	if(wl_display_flush(wayland::display) == -1)
		throw runtime_error("wl_display_flush() failed.");

	// main loop
	wayland::running = true;
	while(wayland::running) {

		// dispatch libdecor events
		if(wayland::libdecorContext)
			wayland::funcs.libdecor_dispatch(wayland::libdecorContext, -1);

		// dispatch Wayland events
		if(wl_display_dispatch(wayland::display) == -1)  // it blocks if there are no events
			throw runtime_error("wl_display_dispatch() failed.");

		// flush outgoing buffers
		if(wl_display_flush(wayland::display) == -1)
			throw runtime_error("wl_display_flush() failed.");

	}
	cout << "Main loop left." << endl;
}


void VulkanWindow::exitMainLoop()
{
	wayland::running = false;
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_wayland.scheduledFrameCallback)
		return;

	cout << "s" << flush;
	_wayland.scheduledFrameCallback = wl_surface_frame(_wayland.wlSurface);
	if(wl_callback_add_listener(_wayland.scheduledFrameCallback, &frameListener, this))
		throw runtime_error("wl_callback_add_listener() failed.");
	wl_surface_commit(_wayland.wlSurface);
}


void VulkanWindowPrivate::frameListenerDone(void *data, wl_callback* cb, uint32_t time)
{
	cout << "cb" << flush;
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	w->_wayland.scheduledFrameCallback = nullptr;
	w->renderFrame();
}


void VulkanWindowPrivate::seatListenerCapabilities(void* data, wl_seat* seat, uint32_t capabilities)
{
	if(capabilities & WL_SEAT_CAPABILITY_POINTER) {
		if(wayland::pointer == nullptr) {
			wayland::pointer = wl_seat_get_pointer(seat);
			if(wl_pointer_add_listener(wayland::pointer, &pointerListener, nullptr))
				throw runtime_error("wl_pointer_add_listener() failed.");
		}
	}
	else
		if(wayland::pointer != nullptr) {
			wl_pointer_release(wayland::pointer);
			wayland::pointer = nullptr;
		}
	if(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		if(wayland::keyboard == nullptr) {
			wayland::keyboard = wl_seat_get_keyboard(seat);
			if(wl_keyboard_add_listener(wayland::keyboard, &keyboardListener, nullptr))
				throw runtime_error("wl_keyboard_add_listener() failed.");
		}
	}
	else
		if(wayland::keyboard != nullptr) {
			wl_keyboard_release(wayland::keyboard);
			wayland::keyboard = nullptr;
		}
}


void VulkanWindowPrivate::pointerListenerEnter(void* data, wl_pointer* pointer, uint32_t serial,
                                               wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	// ignore foreign surfaces
	// (this is necessary on some compositors, to name some: gnome-shell on Ubuntu 24.04 + Fedora 41)
	if(wl_proxy_get_tag(reinterpret_cast<wl_proxy*>(surface)) != &wayland::vulkanWindowTag)
		return;

	// set cursor
	wl_pointer_set_cursor(pointer, serial, wayland::cursorSurface, wayland::cursorHotspotX, wayland::cursorHotspotY);

	// get window pointer
	wayland::windowUnderPointer = static_cast<VulkanWindowPrivate*>(wl_surface_get_user_data(surface));
	assert(wayland::windowUnderPointer && "wl_surface userData does not contain pointer to VulkanWindow.");

	// update mouse state
	float x = float(wl_fixed_to_double(surface_x));
	float y = float(wl_fixed_to_double(surface_y));
	if(wayland::windowUnderPointer->_mouseState.posX != x ||
	   wayland::windowUnderPointer->_mouseState.posY != y)
	{
		wayland::windowUnderPointer->_mouseState.relX = 0;
		wayland::windowUnderPointer->_mouseState.relY = 0;
		wayland::windowUnderPointer->_mouseState.posX = x;
		wayland::windowUnderPointer->_mouseState.posY = y;
		if(wayland::windowUnderPointer->_mouseMoveCallback)
			wayland::windowUnderPointer->_mouseMoveCallback(*wayland::windowUnderPointer, wayland::windowUnderPointer->_mouseState);
	}
}


void VulkanWindowPrivate::pointerListenerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface)
{
	wayland::windowUnderPointer = nullptr;
}


void VulkanWindowPrivate::pointerListenerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	// handle unknown window
	if(wayland::windowUnderPointer == nullptr)
		return;

	float x = float(wl_fixed_to_double(surface_x));
	float y = float(wl_fixed_to_double(surface_y));
	if(wayland::windowUnderPointer->_mouseState.posX != x ||
	   wayland::windowUnderPointer->_mouseState.posY != y)
	{
		wayland::windowUnderPointer->_mouseState.relX = x - wayland::windowUnderPointer->_mouseState.posX;
		wayland::windowUnderPointer->_mouseState.relY = y - wayland::windowUnderPointer->_mouseState.posY;
		wayland::windowUnderPointer->_mouseState.posX = x;
		wayland::windowUnderPointer->_mouseState.posY = y;
		if(wayland::windowUnderPointer->_mouseMoveCallback) {
			wayland::windowUnderPointer->_mouseState.modifiers = wayland::modifiers;
			wayland::windowUnderPointer->_mouseMoveCallback(*wayland::windowUnderPointer, wayland::windowUnderPointer->_mouseState);
		}
	}
}


void VulkanWindowPrivate::pointerListenerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	// handle unknown window
	if(wayland::windowUnderPointer == nullptr)
		return;

	MouseButton::EnumType index;
	switch(button) {
	// button codes taken from linux/input-event-codes.h
	case 0x110: index = MouseButton::Left; break;
	case 0x111: index = MouseButton::Right; break;
	case 0x112: index = MouseButton::Middle; break;
	case 0x113: index = MouseButton::X1; break;
	case 0x114: index = MouseButton::X2; break;
	default: index = MouseButton::Unknown;
	}
	wayland::windowUnderPointer->_mouseState.buttons.set(index, state == WL_POINTER_BUTTON_STATE_PRESSED);
	if(wayland::windowUnderPointer->_mouseButtonCallback) {
		wayland::windowUnderPointer->_mouseState.modifiers = wayland::modifiers;
		ButtonState buttonState =
			(state == WL_POINTER_BUTTON_STATE_PRESSED) ? ButtonState::Pressed : ButtonState::Released;
		wayland::windowUnderPointer->_mouseButtonCallback(
			*wayland::windowUnderPointer, index, buttonState, wayland::windowUnderPointer->_mouseState);
	}
}


void VulkanWindowPrivate::pointerListenerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	// handle unknown window
	if(wayland::windowUnderPointer == nullptr)
		return;

	float v = float(wl_fixed_to_double(value)) * 8 / 120.f;
	float wheelX, wheelY;
	if(axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
		wheelX = 0;
		wheelY = -v;
	}
	else if(axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		wheelX = v;
		wheelY = 0;
	}
	if(wayland::windowUnderPointer->_mouseWheelCallback) {
		wayland::windowUnderPointer->_mouseState.modifiers = wayland::modifiers;
		wayland::windowUnderPointer->_mouseWheelCallback(
			*wayland::windowUnderPointer, wheelX, wheelY, wayland::windowUnderPointer->_mouseState);
	}
}


void VulkanWindowPrivate::keyboardListenerKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	if(format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
		return;

	// map memory
	char* m = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
	if(m == MAP_FAILED)
		throw runtime_error("VulkanWindow::init(): Failed to map memory in keymap event.");

	// create keymap
	struct xkb_keymap* keymap = xkb_keymap_new_from_string(wayland::xkbContext,
		m, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	int r1 = munmap(m, size);
	int r2 = close(fd);

	// handle errors
	if(keymap == nullptr)
		throw runtime_error("VulkanWindow::init(): Failed to create keymap in keymap event.");
	if(r1 != 0) {
		xkb_keymap_unref(keymap);
		throw runtime_error("VulkanWindow::init(): Failed to unmap memory in keymap event.");
	}
	if(r2 != 0) {
		xkb_keymap_unref(keymap);
		throw runtime_error("VulkanWindow::init(): Failed to close file descriptor in keymap event.");
	}

	// unref old xkb_state
	if(wayland::xkbState)
		xkb_state_unref(wayland::xkbState);

	// create new xkb_state
	wayland::xkbState = xkb_state_new(keymap);
	xkb_keymap_unref(keymap);
	if(wayland::xkbState == nullptr)
		throw runtime_error("VulkanWindow::create(): Cannot create XKB state object in keymap event.");
}


void VulkanWindowPrivate::keyboardListenerEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys)
{
	wayland::windowWithKbFocus = static_cast<VulkanWindowPrivate*>(wl_surface_get_user_data(surface));
	assert(wayland::windowWithKbFocus && "wl_surface userData does not contain pointer to VulkanWindow.");

#if 0 // this seems not needed for our simple key down and key up callbacks
	// iterate keys array;
	// the keys array contains currently pressed keys
	// (following for loop is equivalent to wl_array_for_each(key, keys) used in C)
	uint32_t* p;
	for(p = static_cast<uint32_t*>(keys->data);
			reinterpret_cast<char*>(p) < static_cast<char*>(keys->data) + keys->size;
			p++)
	{
		uint32_t scanCode = *p;
		xkb_state_key_get_utf32(_xkbState, scanCode + 8);
	}
#endif
}


void VulkanWindowPrivate::keyboardListenerLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface)
{
	wayland::windowWithKbFocus = nullptr;
}


void VulkanWindowPrivate::keyboardListenerKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t scanCode, uint32_t state)
{
	// get code point
	uint32_t codePoint = xkb_state_key_get_utf32(wayland::xkbState, scanCode + 8);

	// callback
	if(wayland::windowWithKbFocus->_keyCallback) {
		wayland::windowWithKbFocus->_keyCallback(
			*wayland::windowWithKbFocus,
			state==WL_KEYBOARD_KEY_STATE_PRESSED ? KeyState::Pressed : KeyState::Released,
			ScanCode(scanCode),
			KeyCode(codePoint)
		);
	}
}


void VulkanWindowPrivate::keyboardListenerModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed,
                                                    uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
#if 0 // xkb_state_update_mask() disturbs keys that we pass to keyCallbacks
      // because we do not want them to be affected by any key modifiers
	xkb_state_update_mask(_xkbState, mods_depressed, mods_latched, mods_locked, 0, 0, group);
#endif

	// update modifiers global state
	wayland::modifiers.set(VulkanWindow::Modifier::Ctrl,  mods_depressed & 0x04);
	wayland::modifiers.set(VulkanWindow::Modifier::Shift, mods_depressed & 0x01);
	wayland::modifiers.set(VulkanWindow::Modifier::Alt,   mods_depressed & 0x88);
	wayland::modifiers.set(VulkanWindow::Modifier::Meta,  mods_depressed & 0x40);
}


#elif defined(USE_PLATFORM_SDL3)


const vector<const char*>& VulkanWindow::requiredExtensions()
{
	// SDL must be initialized to call this function
	assert(sdl::initialized && "VulkanWindow::init() must be called before calling VulkanWindow::requiredExtensions().");

	// get required instance extensions
	Uint32 count;
	const char* const* p = SDL_Vulkan_GetInstanceExtensions(&count);
	if(p == nullptr)
		throw runtime_error("VulkanWindow: SDL_Vulkan_GetInstanceExtensions() function failed.");
	sdl::requiredInstanceExtensions.clear();
	sdl::requiredInstanceExtensions.reserve(count);
	for(Uint32 i=0; i<count; i++)
		sdl::requiredInstanceExtensions.emplace_back(p[i]);
	return sdl::requiredInstanceExtensions;
}


vector<const char*>& VulkanWindow::appendRequiredExtensions(vector<const char*>& v)  { auto& l=requiredExtensions(); v.insert(v.end(), l.begin(), l.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(requiredExtensions().size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return requiredExtensions().data(); }


bool VulkanWindow::isVisible() const
{
	return _sdl.window && _sdl.visible;
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	// do nothing on already shown window
	if(_sdl.visible)
		return;

	// show window
	if(SDL_ShowWindow(_sdl.window))
		_sdl.visible = true;
	else
		throw runtime_error(string("VulkanWindow: SDL_ShowWindow() function failed. Error details: ") + SDL_GetError());
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// do nothing on already hidden window
	if(_sdl.visible == false)
		return;

	// hide window
	// and set _visible flag immediately
	if(SDL_HideWindow(_sdl.window))
		_sdl.visible = false;
	else
		throw runtime_error(string("VulkanWindow: SDL_HideWindow() function failed. Error details: ") + SDL_GetError());
}


void VulkanWindow::mainLoop()
{
	// mouse functions
	auto handleModifiers =
		[](VulkanWindow* w) -> void
		{
			SDL_Keymod m = SDL_GetModState();
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Ctrl,  m & (SDL_KMOD_LCTRL |SDL_KMOD_RCTRL));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Shift, m & (SDL_KMOD_LSHIFT|SDL_KMOD_RSHIFT));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Alt,   m & (SDL_KMOD_LALT  |SDL_KMOD_RALT));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Meta,  m & (SDL_KMOD_LGUI  |SDL_KMOD_RGUI));
		};
	auto handleMouseMove =
		[](VulkanWindow* w, float newX, float newY) -> void
		{
			if(w->_mouseState.posX != newX ||
			   w->_mouseState.posY != newY)
			{
				w->_mouseState.relX = newX - w->_mouseState.posX;
				w->_mouseState.relY = newY - w->_mouseState.posY;
				w->_mouseState.posX = newX;
				w->_mouseState.posY = newY;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}
		};
	auto handleMouseButton =
		[](VulkanWindow* w, SDL_Event& event, ButtonState buttonState) -> void
		{
			// button
			MouseButton::EnumType mouseButton;
			switch(event.button.button) {
			case SDL_BUTTON_LEFT:   mouseButton = MouseButton::Left; break;
			case SDL_BUTTON_RIGHT:  mouseButton = MouseButton::Right; break;
			case SDL_BUTTON_MIDDLE: mouseButton = MouseButton::Middle; break;
			case SDL_BUTTON_X1:     mouseButton = MouseButton::X1; break;
			case SDL_BUTTON_X2:     mouseButton = MouseButton::X2; break;
			default: mouseButton = MouseButton::Unknown;
			}

			// callback with new button state
			w->_mouseState.buttons.set(mouseButton, buttonState==ButtonState::Pressed);
			if(w->_mouseButtonCallback)
				w->_mouseButtonCallback(*w, mouseButton, buttonState, w->_mouseState);
		};

	// main loop
	SDL_Event event;
	sdl::running = true;
	do {

		// get event
		// (wait for one if no events are in the queue yet)
		if(!SDL_WaitEvent(&event))
			throw runtime_error(string("VulkanWindow: SDL_WaitEvent() function failed. Error details: ") + SDL_GetError());

		// convert SDL_WindowID to VulkanWindow*
		auto getWindow =
			[](SDL_WindowID windowID) -> VulkanWindow* {
				SDL_Window* w = SDL_GetWindowFromID(windowID);
				SDL_PropertiesID props = SDL_GetWindowProperties(w);
				if(props == 0)
					throw runtime_error(string("VulkanWindow: SDL_GetWindowProperties() function failed. Error details: ") + SDL_GetError());
				void* p = SDL_GetPointerProperty(props, sdl::windowPointerName, nullptr);
				if(p == nullptr)
					throw runtime_error(string("VulkanWindow: The property holding VulkanWindow pointer not set for the SDL_Window."));
				return reinterpret_cast<VulkanWindow*>(p);
			};

		// handle event
		// (Make sure that all event types (event.type) handled here, such as SDL_WINDOWEVENT,
		// are removed from the queue in VulkanWindow::destroy().
		// Otherwise we might receive events of already non-existing window.)
		switch(event.type) {

		case SDL_EVENT_WINDOW_EXPOSED: {
			VulkanWindow* w = getWindow(event.window.windowID);
			w->_sdl.framePending = false;
			if(w->_sdl.visible && !w->_sdl.minimized)
				w->renderFrame();
			break;
		}

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
			cout << "Size changed event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			w->scheduleResize();
			break;
		}

		case SDL_EVENT_WINDOW_SHOWN: {
			cout << "Shown event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			w->_sdl.visible = true;
			w->_sdl.minimized = false;
			if(w->_sdl.hiddenWindowFramePending) {
				w->_sdl.hiddenWindowFramePending = false;
				w->scheduleFrame();
			}
			break;
		}

		case SDL_EVENT_WINDOW_HIDDEN: {
			cout << "Hidden event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			w->_sdl.visible = false;
			if(w->_sdl.framePending) {
				w->_sdl.hiddenWindowFramePending = true;
				w->_sdl.framePending = false;
			}
			break;
		}

		case SDL_EVENT_WINDOW_MINIMIZED: {
			cout << "Minimized event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			w->_sdl.minimized = true;
			if(w->_sdl.framePending) {
				w->_sdl.hiddenWindowFramePending = true;
				w->_sdl.framePending = false;
			}
			break;
		}

		case SDL_EVENT_WINDOW_RESTORED: {
			cout << "Restored event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			w->_sdl.minimized = false;
			if(w->_sdl.hiddenWindowFramePending) {
				w->_sdl.hiddenWindowFramePending = false;
				w->scheduleFrame();
			}
			break;
		}

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
			cout << "Close event" << endl;
			VulkanWindow* w = getWindow(event.window.windowID);
			if(w->_closeCallback)
				w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
			else {
				w->hide();
				VulkanWindow::exitMainLoop();
			}
			break;
		}

		case SDL_EVENT_MOUSE_MOTION: {
			VulkanWindow* w = getWindow(event.motion.windowID);
			handleModifiers(w);
			handleMouseMove(w, event.motion.x, event.motion.y);
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			VulkanWindow* w = getWindow(event.button.windowID);
			handleModifiers(w);
			handleMouseMove(w, event.button.x, event.button.y);
			handleMouseButton(w, event, ButtonState::Pressed);
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			VulkanWindow* w = getWindow(event.button.windowID);
			handleModifiers(w);
			handleMouseMove(w, event.button.x, event.button.y);
			handleMouseButton(w, event, ButtonState::Released);
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL:
		{
			VulkanWindow* w = getWindow(event.button.windowID);

			if(w->_mouseWheelCallback)
			{
				handleModifiers(w);

				// handle wheel rotation
				// (value is relative to last wheel event)
				w->_mouseWheelCallback(*w, event.wheel.x, event.wheel.y, w->_mouseState);
			}
			break;
		}

		case SDL_EVENT_KEY_DOWN: {
			VulkanWindow* w = getWindow(event.key.windowID);
			if(w->_keyCallback && event.key.repeat == 0)
			{
				ScanCode scanCode = translateScanCode(event.key.scancode);
				KeyCode keyCode = KeyCode((event.key.key & SDLK_SCANCODE_MASK) ? 0 : event.key.key);
				w->_keyCallback(*w, KeyState::Pressed, scanCode, keyCode);
			}
			break;
		}
		case SDL_EVENT_KEY_UP: {
			VulkanWindow* w = getWindow(event.key.windowID);
			if(w->_keyCallback && event.key.repeat == 0)
			{
				ScanCode scanCode = translateScanCode(event.key.scancode);
				KeyCode keyCode = KeyCode((event.key.key & SDLK_SCANCODE_MASK) ? 0 : event.key.key);
				w->_keyCallback(*w, KeyState::Released, scanCode, keyCode);
			}
			break;
		}

		case SDL_EVENT_QUIT:  // SDL_EVENT_QUIT is generated on variety of reasons,
		                      // including SIGINT and SIGTERM, or pressing Command-Q on Mac OS X.
		                      // By default, it would also be generated by SDL on the last window close.
		                      // This would interfere with VulkanWindow expected behaviour to exit main loop after the call of exitMainLoop().
		                      // So, we disabled it by SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE hint that is available since SDL 2.0.22.
			return;
		}

	} while(sdl::running);
}


void VulkanWindow::exitMainLoop()
{
	sdl::running = false;
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_sdl.framePending)
		return;

	// handle invisible and minimized window
	if(_sdl.visible==false || _sdl.minimized) {
		_sdl.hiddenWindowFramePending = true;
		return;
	}

	_sdl.framePending = true;

	SDL_Event e;
	e.window.type = SDL_EVENT_WINDOW_EXPOSED;
	e.window.timestamp = SDL_GetTicksNS();
	e.window.windowID = SDL_GetWindowID(_sdl.window);
	e.window.data1 = 0;
	e.window.data2 = 0;
	SDL_PushEvent(&e);  // the call might fail, but we ignore the error value
}


#elif defined(USE_PLATFORM_SDL2)


const vector<const char*>& VulkanWindow::requiredExtensions()
{
	// SDL must be initialized to call this function
	assert(sdl::initialized && "VulkanWindow::init() must be called before calling VulkanWindow::requiredExtensions().");

	// cache the result in static local variable
	// so extension list is constructed only once
	static const vector<const char*> l =
		[]() {

			// create temporary Vulkan window
			unique_ptr<SDL_Window,void(*)(SDL_Window*)> tmpWindow(
				SDL_CreateWindow("Temporary", 0,0, 1,1, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN),
				[](SDL_Window* w){ SDL_DestroyWindow(w); }
			);
			if(tmpWindow.get() == nullptr)
				throw runtime_error(string("VulkanWindow: SDL_CreateWindow() function failed. Error details: ") + SDL_GetError());

			// get number of required instance extensions
			unsigned int count;
			if(!SDL_Vulkan_GetInstanceExtensions(tmpWindow.get(), &count, nullptr))
				throw runtime_error("VulkanWindow: SDL_Vulkan_GetInstanceExtensions() function failed.");

			// get required instance extensions
			vector<const char*> l;
			l.resize(count);
			if(!SDL_Vulkan_GetInstanceExtensions(tmpWindow.get(), &count, l.data()))
				throw runtime_error("VulkanWindow: SDL_Vulkan_GetInstanceExtensions() function failed.");
			l.resize(count);
			return l;

		}();

	return l;
}

vector<const char*>& VulkanWindow::appendRequiredExtensions(vector<const char*>& v)  { auto& l=requiredExtensions(); v.insert(v.end(), l.begin(), l.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(requiredExtensions().size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return requiredExtensions().data(); }


bool VulkanWindow::isVisible() const
{
	return _sdl.window && _sdl.visible;
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	// show window
	// and set _visible flag immediately
	SDL_ShowWindow(_sdl.window);
	_sdl.visible = true;
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// hide window
	// and set _visible flag immediately
	SDL_HideWindow(_sdl.window);
	_sdl.visible = false;
}


void VulkanWindow::mainLoop()
{
	// mouse functions
	auto handleModifiers =
		[](VulkanWindow* w) -> void
		{
			SDL_Keymod m = SDL_GetModState();
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Ctrl,  m & (KMOD_LCTRL|KMOD_RCTRL));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Shift, m & (KMOD_LSHIFT|KMOD_RSHIFT));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Alt,   m & (KMOD_LALT|KMOD_RALT));
			w->_mouseState.modifiers.set(VulkanWindow::Modifier::Meta,  m & (KMOD_LGUI|KMOD_RGUI));
		};
	auto handleMouseMove =
		[](VulkanWindow* w, float newX, float newY) -> void
		{
			if(w->_mouseState.posX != newX ||
			   w->_mouseState.posY != newY)
			{
				w->_mouseState.relX = newX - w->_mouseState.posX;
				w->_mouseState.relY = newY - w->_mouseState.posY;
				w->_mouseState.posX = newX;
				w->_mouseState.posY = newY;
				if(w->_mouseMoveCallback)
					w->_mouseMoveCallback(*w, w->_mouseState);
			}
		};
	auto handleMouseButton =
		[](VulkanWindow* w, SDL_Event& event, ButtonState buttonState) -> void
		{
			// button
			MouseButton::EnumType mouseButton;
			switch(event.button.button) {
			case SDL_BUTTON_LEFT:   mouseButton = MouseButton::Left; break;
			case SDL_BUTTON_RIGHT:  mouseButton = MouseButton::Right; break;
			case SDL_BUTTON_MIDDLE: mouseButton = MouseButton::Middle; break;
			case SDL_BUTTON_X1:     mouseButton = MouseButton::X1; break;
			case SDL_BUTTON_X2:     mouseButton = MouseButton::X2; break;
			default: mouseButton = MouseButton::Unknown;
			}

			// callback with new button state
			w->_mouseState.buttons.set(mouseButton, buttonState==ButtonState::Pressed);
			if(w->_mouseButtonCallback)
				w->_mouseButtonCallback(*w, mouseButton, buttonState, w->_mouseState);
		};

	// main loop
	SDL_Event event;
	sdl::running = true;
	do {

		// get event
		// (wait for one if no events are in the queue yet)
		if(SDL_WaitEvent(&event) == 0)
			throw runtime_error(string("VulkanWindow: SDL_WaitEvent() function failed. Error details: ") + SDL_GetError());

		// handle event
		// (Make sure that all event types (event.type) handled here, such as SDL_WINDOWEVENT,
		// are removed from the queue in VulkanWindow::destroy().
		// Otherwise we might receive events of already non-existing window.)
		switch(event.type) {
		case SDL_WINDOWEVENT:
			switch(event.window.event) {

			case SDL_WINDOWEVENT_EXPOSED: {
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->_sdl.framePending = false;
				if(w->_sdl.visible && !w->_sdl.minimized)
					w->renderFrame();
				break;
			}

			case SDL_WINDOWEVENT_SIZE_CHANGED: {
				cout << "Size changed event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->scheduleResize();
				break;
			}

			case SDL_WINDOWEVENT_SHOWN: {
				cout << "Shown event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->_sdl.visible = true;
				w->_sdl.minimized = false;
				if(w->_sdl.hiddenWindowFramePending) {
					w->_sdl.hiddenWindowFramePending = false;
					w->scheduleFrame();
				}
				break;
			}

			case SDL_WINDOWEVENT_HIDDEN: {
				cout << "Hidden event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->_sdl.visible = false;
				if(w->_sdl.framePending) {
					w->_sdl.hiddenWindowFramePending = true;
					w->_sdl.framePending = false;
				}
				break;
			}

			case SDL_WINDOWEVENT_MINIMIZED: {
				cout << "Minimized event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->_sdl.minimized = true;
				if(w->_sdl.framePending) {
					w->_sdl.hiddenWindowFramePending = true;
					w->_sdl.framePending = false;
				}
				break;
			}

			case SDL_WINDOWEVENT_RESTORED: {
				cout << "Restored event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				w->_sdl.minimized = false;
				if(w->_sdl.hiddenWindowFramePending) {
					w->_sdl.hiddenWindowFramePending = false;
					w->scheduleFrame();
				}
				break;
			}

			case SDL_WINDOWEVENT_CLOSE: {
				cout << "Close event" << endl;
				VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
					SDL_GetWindowData(SDL_GetWindowFromID(event.window.windowID), sdl::windowPointerName));
				if(w->_closeCallback)
					w->_closeCallback(*w);  // VulkanWindow object might be already destroyed when returning from the callback
				else {
					w->hide();
					VulkanWindow::exitMainLoop();
				}
				break;
			}

			}
			break;

		case SDL_MOUSEMOTION: {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.motion.windowID), sdl::windowPointerName));
			handleModifiers(w);
			handleMouseMove(w, float(event.motion.x), float(event.motion.y));
			break;
		}
		case SDL_MOUSEBUTTONDOWN: {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.button.windowID), sdl::windowPointerName));
			handleModifiers(w);
			handleMouseMove(w, float(event.button.x), float(event.button.y));
			handleMouseButton(w, event, ButtonState::Pressed);
			break;
		}
		case SDL_MOUSEBUTTONUP: {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.button.windowID), sdl::windowPointerName));
			handleModifiers(w);
			handleMouseMove(w, float(event.button.x), float(event.button.y));
			handleMouseButton(w, event, ButtonState::Released);
			break;
		}
		case SDL_MOUSEWHEEL:
		{
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.button.windowID), sdl::windowPointerName));

			if(w->_mouseWheelCallback)
			{
				handleModifiers(w);

				// handle wheel rotation
				// (value is relative to last wheel event)
			#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION >= 18
				float wheelX = event.wheel.preciseX;
				float wheelY = event.wheel.preciseY;
			#else
				float wheelX = float(event.wheel.x);
				float wheelY = float(event.wheel.y);
			#endif
				w->_mouseWheelCallback(*w, wheelX, wheelY, w->_mouseState);
			}
			break;
		}

		case SDL_KEYDOWN: {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.key.windowID), sdl::windowPointerName));
			if(w->_keyCallback && event.key.repeat == 0)
			{
				ScanCode scanCode = translateScanCode(event.key.keysym.scancode);
				KeyCode keyCode = KeyCode((event.key.keysym.sym & SDLK_SCANCODE_MASK) ? 0 : event.key.keysym.sym);
				w->_keyCallback(*w, KeyState::Pressed, scanCode, keyCode);
			}
			break;
		}
		case SDL_KEYUP: {
			VulkanWindow* w = reinterpret_cast<VulkanWindow*>(
				SDL_GetWindowData(SDL_GetWindowFromID(event.key.windowID), sdl::windowPointerName));
			if(w->_keyCallback && event.key.repeat == 0)
			{
				ScanCode scanCode = translateScanCode(event.key.keysym.scancode);
				KeyCode keyCode = KeyCode((event.key.keysym.sym & SDLK_SCANCODE_MASK) ? 0 : event.key.keysym.sym);
				w->_keyCallback(*w, KeyState::Released, scanCode, keyCode);
			}
			break;
		}

		case SDL_QUIT:  // SDL_QUIT is generated on variety of reasons, including SIGINT and SIGTERM, or pressing Command-Q on Mac OS X.
		                // By default, it would also be generated by SDL on the last window close.
		                // This would interfere with VulkanWindow expected behaviour to exit main loop after the call of exitMainLoop().
		                // So, we disabled it by SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE hint that is available since SDL 2.0.22.
			return;
		}

	} while(sdl::running);
}


void VulkanWindow::exitMainLoop()
{
	sdl::running = false;
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_sdl.framePending)
		return;

	// handle invisible and minimized window
	if(_sdl.visible==false || _sdl.minimized) {
		_sdl.hiddenWindowFramePending = true;
		return;
	}

	_sdl.framePending = true;

	SDL_Event e;
	e.window.type = SDL_WINDOWEVENT;
	e.window.timestamp = SDL_GetTicks();
	e.window.windowID = SDL_GetWindowID(_sdl.window);
	e.window.event = SDL_WINDOWEVENT_EXPOSED;
	e.window.data1 = 0;
	e.window.data2 = 0;
	SDL_PushEvent(&e);  // the call might fail, but we ignore the error value
}


#elif defined(USE_PLATFORM_GLFW)


const vector<const char*>& VulkanWindow::requiredExtensions()
{
	// cache the result in static local variable
	// so extension list is constructed only once
	static const vector<const char*> l =
		[]() {

			// get required extensions
			uint32_t count;
			const char** a = glfwGetRequiredInstanceExtensions(&count);
			if(a == nullptr) {
				const char* errorString;
				int errorCode = glfwGetError(&errorString);
				throw runtime_error(string("VulkanWindow: glfwGetRequiredInstanceExtensions() function failed. Error code: ") +
				                    to_string(errorCode) + ". Error string: " + errorString);
			}
			
			// fill std::vector
			vector<const char*> l;
			l.reserve(count);
			for(uint32_t i=0; i<count; i++)
				l.emplace_back(a[i]);
			return l;
		}();

	return l;
}


vector<const char*>& VulkanWindow::appendRequiredExtensions(vector<const char*>& v)  { auto& l=requiredExtensions(); v.insert(v.end(), l.begin(), l.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(requiredExtensions().size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return requiredExtensions().data(); }


bool VulkanWindow::isVisible() const
{
	return _glfw.window && _glfw.visible;
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::mainLoop().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::mainLoop() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::mainLoop().");

	// show window
	_glfw.visible = true;
	glfwShowWindow(_glfw.window);
	checkError("glfwShowWindow");
	scheduleFrame();
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// hide window
	_glfw.visible = false;
	glfwHideWindow(_glfw.window);
	checkError("glfwHideWindow");

	// cancel pending frame, if any, on window hide
	if(_glfw.framePendingState != FramePendingState::NotPending) {
		_glfw.framePendingState = FramePendingState::NotPending;
		for(size_t i=0; i<glfw::framePendingWindows.size(); i++)
			if(glfw::framePendingWindows[i] == this) {
				glfw::framePendingWindows[i] = glfw::framePendingWindows.back();
				glfw::framePendingWindows.pop_back();
				break;
			}
	}
}


void VulkanWindow::mainLoop()
{
	// main loop
	glfw::running = true;
	do {

		if(glfw::framePendingWindows.empty())
		{
			glfwWaitEvents();
			checkError("glfwWaitEvents");
		}
		else
		{
			glfwPollEvents();
			checkError("glfwPollEvents");
		}

		// render all windows with _framePendingState set to Pending
		for(size_t i=0; i<glfw::framePendingWindows.size(); ) {

			// render frame
			VulkanWindow* w = glfw::framePendingWindows[i];
			w->_glfw.framePendingState = FramePendingState::TentativePending;
			w->renderFrame();

			// was frame scheduled again?
			// (it might be rescheduled again in renderFrame())
			if(w->_glfw.framePendingState == FramePendingState::TentativePending) {

				// update state to no-frame-pending
				w->_glfw.framePendingState = FramePendingState::NotPending;
				if(glfw::framePendingWindows.size() == 1) {
					glfw::framePendingWindows.clear();  // all iterators are invalidated
					break;
				}
				else {
					glfw::framePendingWindows[i] = glfw::framePendingWindows.back();
					glfw::framePendingWindows.pop_back();  // end() iterator is invalidated
					continue;
				}
			}
			i++;

		}

	} while(glfw::running);
}


void VulkanWindow::exitMainLoop()
{
	glfw::running = false;
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	if(_glfw.framePendingState == FramePendingState::Pending)
		return;

	if(_glfw.framePendingState == FramePendingState::NotPending)
		glfw::framePendingWindows.push_back(this);

	_glfw.framePendingState = FramePendingState::Pending;
}


#elif defined(USE_PLATFORM_QT)


const vector<const char*>& VulkanWindow::requiredExtensions()
{
	static vector<const char*> l =
		[]() {
			QString platform = QGuiApplication::platformName();
			if(platform == "wayland")
				return vector<const char*>{ "VK_KHR_surface", "VK_KHR_wayland_surface" };
			else if(platform == "windows")
				return vector<const char*>{ "VK_KHR_surface", "VK_KHR_win32_surface" };
			else if(platform == "xcb")
				return vector<const char*>{ "VK_KHR_surface", "VK_KHR_xcb_surface" };
			else if(platform == "")
				throw runtime_error("VulkanWindow::requiredExtensions(): QGuiApplication not initialized or unknown Qt platform.");
			else
				throw runtime_error("VulkanWindow::requiredExtensions(): Unknown Qt platform \"" + platform.toStdString() + "\".");
		}();

	return l;
}

vector<const char*>& VulkanWindow::appendRequiredExtensions(vector<const char*>& v)  { auto& l=requiredExtensions(); v.insert(v.end(), l.begin(), l.end()); return v; }
uint32_t VulkanWindow::requiredExtensionCount()  { return uint32_t(requiredExtensions().size()); }
const char* const* VulkanWindow::requiredExtensionNames()  { return requiredExtensions().data(); }


bool VulkanWindow::isVisible() const
{
	return _qt.window && _qt.window->isVisible();
}


void VulkanWindow::show()
{
	// asserts for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");
	assert(_resizeCallback && "Resize callback must be set before VulkanWindow::show() call. Please, call VulkanWindow::setResizeCallback() before VulkanWindow::show().");
	assert(_frameCallback && "Frame callback need to be set before VulkanWindow::show() call. Please, call VulkanWindow::setFrameCallback() before VulkanWindow::show().");

	// show window
	_qt.window->setVisible(true);
}


void VulkanWindow::hide()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// hide window
	_qt.window->setVisible(false);
}


void VulkanWindow::mainLoop()
{
	// call exec()
	thrownException = nullptr;
	QGuiApplication::exec();
	if(thrownException)  // rethrow the exception that we caught in QtRenderingWindow::event()
		rethrow_exception(thrownException);
}


void VulkanWindow::exitMainLoop()
{
	QGuiApplication::exit();
}


bool QtRenderingWindow::event(QEvent* event)
{
	try {

		// mouse functions
		auto getMouseButton =
			[](QMouseEvent* e) -> VulkanWindow::MouseButton::EnumType {
				switch(e->button()) {
				case Qt::LeftButton: return VulkanWindow::MouseButton::Left;
				case Qt::RightButton: return VulkanWindow::MouseButton::Right;
				case Qt::MiddleButton: return VulkanWindow::MouseButton::Middle;
				case Qt::XButton1: return VulkanWindow::MouseButton::X1;
				case Qt::XButton2: return VulkanWindow::MouseButton::X2;
				default: return VulkanWindow::MouseButton::Unknown;
				}
			};
		auto handleModifiers =
			[](VulkanWindow* vulkanWindow, QInputEvent* e)
			{
				Qt::KeyboardModifiers m = e->modifiers();
				vulkanWindow->_mouseState.modifiers.set(VulkanWindow::Modifier::Ctrl,  m & Qt::ControlModifier);
				vulkanWindow->_mouseState.modifiers.set(VulkanWindow::Modifier::Shift, m & Qt::ShiftModifier);
				vulkanWindow->_mouseState.modifiers.set(VulkanWindow::Modifier::Alt,   m & Qt::AltModifier);
				vulkanWindow->_mouseState.modifiers.set(VulkanWindow::Modifier::Meta,  m & Qt::MetaModifier);
			};
		auto handleMouseMove =
			[](VulkanWindow* vulkanWindow, float newX, float newY)
			{
				if(vulkanWindow->_mouseState.posX != newX ||
				   vulkanWindow->_mouseState.posY != newY)
				{
					vulkanWindow->_mouseState.relX = newX - vulkanWindow->_mouseState.posX;
					vulkanWindow->_mouseState.relY = newY - vulkanWindow->_mouseState.posY;
					vulkanWindow->_mouseState.posX = newX;
					vulkanWindow->_mouseState.posY = newY;
					if(vulkanWindow->_mouseMoveCallback)
						vulkanWindow->_mouseMoveCallback(*vulkanWindow, vulkanWindow->_mouseState);
				}
			};
		auto handleMouseButton =
			[](VulkanWindow* vulkanWindow, float x, float y, VulkanWindow::MouseButton::EnumType mouseButton, VulkanWindow::ButtonState buttonState) -> bool
			{
				// handle mouse move, if any
				if(vulkanWindow->_mouseState.posX != x ||
				   vulkanWindow->_mouseState.posY != y)
				{
					vulkanWindow->_mouseState.relX = x - vulkanWindow->_mouseState.posX;
					vulkanWindow->_mouseState.relY = y - vulkanWindow->_mouseState.posY;
					vulkanWindow->_mouseState.posX = x;
					vulkanWindow->_mouseState.posY = y;
					if(vulkanWindow->_mouseMoveCallback)
						vulkanWindow->_mouseMoveCallback(*vulkanWindow, vulkanWindow->_mouseState);
				}

				// callback with new button state
				vulkanWindow->_mouseState.buttons.set(mouseButton, buttonState==VulkanWindow::ButtonState::Pressed);
				if(vulkanWindow->_mouseButtonCallback)
					vulkanWindow->_mouseButtonCallback(*vulkanWindow, mouseButton, buttonState, vulkanWindow->_mouseState);
				return true;

			};
		auto convertQtKeyToUtf32 =
			[](int qtKey) -> uint32_t
			{
				switch(qtKey) {
				case Qt::Key_Escape: return 0x1b;
				case Qt::Key_Tab: return '\t';
				case Qt::Key_Backspace: return 0x08;
				case Qt::Key_Return: return '\n';
				case Qt::Key_Enter: return '\n';
				case Qt::Key_Space: return ' ';
				default: return 0;
				}
			};

		// handle verious events
		switch(event->type()) {

		case QEvent::Type::Timer:
			cout<<"t";
			killTimer(timer);
			timer = 0;
			if(isExposed())
				vulkanWindow->renderFrame();
			return true;

		case QEvent::Type::Expose: {
			cout<<"e";
			bool r = QWindow::event(event);
			if(isExposed())
				vulkanWindow->scheduleFrame();
			return r;
		}

		case QEvent::Type::UpdateRequest:
			if(isExposed())
				vulkanWindow->scheduleFrame();
			return true;

		case QEvent::Type::Resize: {
			// schedule resize
			// (we do not call vulkanWindow->scheduleResize() directly
			// because Resize event might be delivered already inside VulkanWindow::create() function,
			// resulting in assert error of invalid usage of scheduleFrame(); instead, we schedule frame manually)
			vulkanWindow->_resizePending = true;
			scheduleFrameTimer();
			return QWindow::event(event);
		}

		// handle mouse events
		case QEvent::Type::MouseMove: {
			QMouseEvent* e = static_cast<QMouseEvent*>(event);
			handleModifiers(vulkanWindow, e);
		#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			QPointF p = e->position() * devicePixelRatio();
		#else
			QPointF p = e->localPos() * devicePixelRatio();
		#endif
			handleMouseMove(vulkanWindow, p.x(), p.y());
			return true;
		}
		case QEvent::Type::MouseButtonPress: {
			QMouseEvent* e = static_cast<QMouseEvent*>(event);
		#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			QPointF p = e->position() * devicePixelRatio();
		#else
			QPointF p = e->localPos() * devicePixelRatio();
		#endif
			handleModifiers(vulkanWindow, e);
			return handleMouseButton(vulkanWindow, p.x(), p.y(), getMouseButton(e), VulkanWindow::ButtonState::Pressed);
		}
		case QEvent::Type::MouseButtonRelease: {
			QMouseEvent* e = static_cast<QMouseEvent*>(event);
		#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			QPointF p = e->position() * devicePixelRatio();
		#else
			QPointF p = e->localPos() * devicePixelRatio();
		#endif
			handleModifiers(vulkanWindow, e);
			return handleMouseButton(vulkanWindow, p.x(), p.y(), getMouseButton(e), VulkanWindow::ButtonState::Released);
		}
		case QEvent::Type::Wheel: {

			QWheelEvent* e = static_cast<QWheelEvent*>(event);
			handleModifiers(vulkanWindow, e);

			// handle mouse move, if any
			QPointF p = e->position() * devicePixelRatio();
			handleMouseMove(vulkanWindow, p.x(), p.y());

			// handle wheel rotation
			// (value is relative since last wheel event)
			if(vulkanWindow->_mouseWheelCallback) {
				p = QPointF(e->angleDelta()) / 120.f;
				vulkanWindow->_mouseWheelCallback(*vulkanWindow, -p.x(), p.y(), vulkanWindow->_mouseState);
			}
			return true;

		}

		// handle key events
		case QEvent::Type::KeyPress: {
			if(vulkanWindow->_keyCallback) {
				QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
				if(!keyEvent->isAutoRepeat()) {

					// scan code
				#ifdef _WIN32
					VulkanWindow::ScanCode scanCode = translateScanCode(keyEvent->nativeScanCode());
				#else
					VulkanWindow::ScanCode scanCode = VulkanWindow::ScanCode(keyEvent->nativeScanCode() - 8);
				#endif

					// convert to lower case
					int32_t k = keyEvent->key();
					if(k >= Qt::Key_A && k <= Qt::Key_Z)
						k += 32;
					else {
						u32string s =  keyEvent->text().toStdU32String();
						k = (s.empty()) ? 0 : int32_t(s.front());
					}

					// callback
					vulkanWindow->_keyCallback(*vulkanWindow, VulkanWindow::KeyState::Pressed, scanCode, VulkanWindow::KeyCode(k));
				}
			}
			return true;
		}
		case QEvent::Type::KeyRelease: {
			if(vulkanWindow->_keyCallback) {
				QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
				if(!keyEvent->isAutoRepeat()) {

					// scan code
				#ifdef _WIN32
					VulkanWindow::ScanCode scanCode = translateScanCode(keyEvent->nativeScanCode());
				#else
					VulkanWindow::ScanCode scanCode = VulkanWindow::ScanCode(keyEvent->nativeScanCode() - 8);
				#endif

					// convert to lower case
					int32_t k = keyEvent->key();
					if(k >= Qt::Key_A && k <= Qt::Key_Z)
						k += 32;
					else {
						u32string s =  keyEvent->text().toStdU32String();
						k = (s.empty()) ? 0 : int32_t(s.front());
					}

					// callback
					vulkanWindow->_keyCallback(*vulkanWindow, VulkanWindow::KeyState::Released, scanCode, VulkanWindow::KeyCode(k));
				}
			}
			return true;
		}

		// hide window on close
		// (we must not really close it as Vulkan surface would be destroyed
		// and this would make a problem as swapchain still exists and Vulkan
		// requires the swapchain to be destroyed first)
		case QEvent::Type::Close:
			if(vulkanWindow->_closeCallback)
				vulkanWindow->_closeCallback(*vulkanWindow);  // VulkanWindow object might be already destroyed when returning from the callback
			else {
				vulkanWindow->hide();
				VulkanWindow::exitMainLoop();
			}
			return true;

		default:
			return QWindow::event(event);
		}
	}
	catch(...) {
		VulkanWindow::thrownException = std::current_exception();
		QGuiApplication::exit();
		return true;
	}
}


void QtRenderingWindow::scheduleFrameTimer()
{
	// start zero timeout timer
	if(timer == 0) {
		timer = startTimer(0);
		if(timer == 0)
			throw runtime_error("VulkanWindow::scheduleNextFrame(): Cannot allocate timer.");
	}
}


void VulkanWindow::scheduleFrame()
{
	// assert for valid usage
	assert(_surface && "VulkanWindow::_surface is null, indicating invalid VulkanWindow object. Call VulkanWindow::create() to initialize it.");

	// start zero timeout timer
	static_cast<QtRenderingWindow*>(_qt.window)->scheduleFrameTimer();
}


#endif



#if defined(USE_PLATFORM_WIN32)

void VulkanWindow::updateTitle()
{
	wstring s = utf8toWString(_title.c_str());
	if(!SetWindowTextW(HWND(_win32.hwnd), s.c_str()))
		throw runtime_error("VulkanWindow::updateTitle(): Failed to set window title.");
}

#elif defined(USE_PLATFORM_XLIB)

void VulkanWindow::updateTitle()
{
	XStoreName(xlib::display, _xlib.window, _title.c_str());
	XChangeProperty(
		xlib::display,
		_xlib.window,
		xlib::netWmName,  // property
		xlib::utf8String,  // type
		8,  // format
		PropModeReplace,  // mode
		reinterpret_cast<const unsigned char*>(_title.c_str()),  // data
		_title.size()  // nelements
	);
}

#elif defined(USE_PLATFORM_WAYLAND)

void VulkanWindow::updateTitle()
{
	// if libdecor is used, use libdecor_frame_set_title()
	// otherwise set it using xdg_toplevel_set_title()
	if(_wayland.libdecorFrame)
		wayland::funcs.libdecor_frame_set_title(_wayland.libdecorFrame, _title.c_str());
	if(_wayland.xdgTopLevel)
		xdg_toplevel_set_title(_wayland.xdgTopLevel, _title.c_str());
}

#elif defined(USE_PLATFORM_SDL3)

void VulkanWindow::updateTitle()
{
	if(!SDL_SetWindowTitle(_sdl.window, _title.c_str()))
		throw runtime_error("VulkanWindow::updateTitle(): Failed to set window title.");
}

#elif defined(USE_PLATFORM_SDL2)

void VulkanWindow::updateTitle()
{
	SDL_SetWindowTitle(_sdl.window, _title.c_str());
}

#elif defined(USE_PLATFORM_GLFW)

void VulkanWindow::updateTitle()
{
	glfwSetWindowTitle(_glfw.window, _title.c_str());
}

#elif defined(USE_PLATFORM_QT)

void VulkanWindow::updateTitle()
{
	_qt.window->setTitle(_title.c_str()); // this treats _title as utf8 string
}

#endif



#if defined(USE_PLATFORM_WIN32)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_win32.hwnd || !_win32.visible)
		return WindowState::Hidden;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	if(!GetWindowPlacement(HWND(_win32.hwnd), &wp))
		throw runtime_error("VulkanWindow::windowState(): The function GetWindowPlacement() failed.");

	switch(wp.showCmd) {
	case SW_HIDE:             return WindowState::Hidden;
	case SW_MINIMIZE:
	case SW_SHOWMINIMIZED:
	case SW_SHOWMINNOACTIVE:
	case SW_FORCEMINIMIZE:    return WindowState::Minimized;
	case SW_SHOW:
	case SW_SHOWNORMAL:
	case SW_SHOWNA:
	case SW_SHOWNOACTIVATE:
	case SW_SHOWDEFAULT:
	case SW_RESTORE:          return WindowState::Normal;
	case SW_SHOWMAXIMIZED:    return WindowState::Maximized;
	default:                  return WindowState::Normal;  // unknown value => exception might be thrown here;
	                                                       // let's rather consider it some new Windows value and try as less harm as possible
	}
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	switch(windowState) {
	case WindowState::Hidden:     hide(); break;
	case WindowState::Minimized:  ShowWindow(HWND(_win32.hwnd), SW_SHOWMINIMIZED); _win32.visible = true; break;
	case WindowState::Normal:     ShowWindow(HWND(_win32.hwnd), SW_SHOWNORMAL); break;
	case WindowState::Maximized:  ShowWindow(HWND(_win32.hwnd), SW_SHOWMAXIMIZED); _win32.visible = true; break;
	case WindowState::FullScreen: break;
	}
}

#elif defined(USE_PLATFORM_XLIB)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_xlib.window)
		return WindowState::Hidden;

	return WindowState::Normal;
}

void VulkanWindow::setWindowState(WindowState windowState)
{
}

#elif defined(USE_PLATFORM_WAYLAND)

void VulkanWindowPrivate::syncListenerDone(void *data, wl_callback* cb, uint32_t time)
{
	VulkanWindowPrivate* w = static_cast<VulkanWindowPrivate*>(data);
	w->_wayland.numSyncEventsOnTheFly--;
}

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_wayland.wlSurface)
		return WindowState::Hidden;

	// make sure all window state changes were processed by Wayland server
	waitAllSyncEvents(const_cast<unsigned&>(_wayland.numSyncEventsOnTheFly));

	return _wayland.windowState;
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	assert(_wayland.wlSurface && "VulkanWindow::setWindowState() called on VulkanWindow without created wlSurface. Call VulkanWindow::create() first.");

	// change window state
	switch(windowState) {
	case WindowState::Hidden:
		hide();
		break;
	case WindowState::Minimized:
		if(!isVisible())

			// show the window with appropriate settings
			show(
				[](VulkanWindow& w) { xdg_toplevel_set_minimized(w._wayland.xdgTopLevel); },
				[](VulkanWindow& w) { wayland::funcs.libdecor_frame_set_minimized(w._wayland.libdecorFrame); }
			);

		else

			// set window state
			if(_wayland.libdecorFrame)
				wayland::funcs.libdecor_frame_set_minimized(_wayland.libdecorFrame);
			else
				xdg_toplevel_set_minimized(_wayland.xdgTopLevel);

		break;
	case WindowState::Normal:
		if(!isVisible())

			// show the window with appropriate settings
			show(
				[](VulkanWindow& w) { xdg_toplevel_unset_maximized(w._wayland.xdgTopLevel); xdg_toplevel_unset_fullscreen(w._wayland.xdgTopLevel); },
				[](VulkanWindow& w) { wayland::funcs.libdecor_frame_unset_maximized(w._wayland.libdecorFrame); wayland::funcs.libdecor_frame_unset_fullscreen(w._wayland.libdecorFrame); }
			);

		else {

			// set window state
			if(_wayland.libdecorFrame) {
				wayland::funcs.libdecor_frame_unset_maximized(_wayland.libdecorFrame);
				wayland::funcs.libdecor_frame_unset_fullscreen(_wayland.libdecorFrame);
			}
			else {
				xdg_toplevel_unset_maximized(_wayland.xdgTopLevel);
				xdg_toplevel_unset_fullscreen(_wayland.xdgTopLevel);
			}

			// send callback
			wl_callback* callback = wl_display_sync(wayland::display);
			if(wl_callback_add_listener(callback, &syncListener, this))
				throw runtime_error("wl_callback_add_listener() failed.");
			_wayland.numSyncEventsOnTheFly++;
			wl_display_flush(wayland::display);

		}
		break;
	case WindowState::Maximized:
		if(!isVisible())

			// show the window with appropriate settings
			show(
				[](VulkanWindow& w) { xdg_toplevel_set_maximized(w._wayland.xdgTopLevel); xdg_toplevel_unset_fullscreen(w._wayland.xdgTopLevel); },
				[](VulkanWindow& w) { wayland::funcs.libdecor_frame_set_maximized(w._wayland.libdecorFrame); wayland::funcs.libdecor_frame_unset_fullscreen(w._wayland.libdecorFrame); }
			);

		else {

			// send callback
			if(_wayland.libdecorFrame) {
				wayland::funcs.libdecor_frame_set_maximized(_wayland.libdecorFrame);
				wayland::funcs.libdecor_frame_unset_fullscreen(_wayland.libdecorFrame);
			}
			else {
				xdg_toplevel_set_maximized(_wayland.xdgTopLevel);
				xdg_toplevel_unset_fullscreen(_wayland.xdgTopLevel);
			}

			// send callback
			wl_callback* callback = wl_display_sync(wayland::display);
			if(wl_callback_add_listener(callback, &syncListener, this))
				throw runtime_error("wl_callback_add_listener() failed.");
			_wayland.numSyncEventsOnTheFly++;
			wl_display_flush(wayland::display);

		}
		break;
	case WindowState::FullScreen:
		if(!isVisible())

			// show the window with appropriate settings
			show(
				[](VulkanWindow& w) { xdg_toplevel_set_fullscreen(w._wayland.xdgTopLevel, nullptr); },
				[](VulkanWindow& w) { wayland::funcs.libdecor_frame_set_fullscreen(w._wayland.libdecorFrame, nullptr); }
			);

		else {

			// send callback
			if(_wayland.libdecorFrame)
				wayland::funcs.libdecor_frame_set_fullscreen(_wayland.libdecorFrame, nullptr);
			else
				xdg_toplevel_set_fullscreen(_wayland.xdgTopLevel, nullptr);

			// send callback
			wl_callback* callback = wl_display_sync(wayland::display);
			if(wl_callback_add_listener(callback, &syncListener, this))
				throw runtime_error("wl_callback_add_listener() failed.");
			_wayland.numSyncEventsOnTheFly++;
			wl_display_flush(wayland::display);

		}
		break;
	default:
		throw runtime_error("VulkanWindow::setWindowState(): Invalid WindowState value passed as parameter.");
	}
	if(wl_display_roundtrip(wayland::display) == -1)
		throw runtime_error("wl_display_roundtrip() failed.");
}

#elif defined(USE_PLATFORM_SDL3)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_sdl.window)
		return WindowState::Hidden;

	SDL_WindowFlags f = SDL_GetWindowFlags(_sdl.window);
	if(f & SDL_WINDOW_HIDDEN)
		return WindowState::Hidden;
	if(f & SDL_WINDOW_MINIMIZED)
		return WindowState::Minimized;
	if(f & SDL_WINDOW_FULLSCREEN)
		return WindowState::FullScreen;
	if(f & SDL_WINDOW_MAXIMIZED)
		return WindowState::Maximized;
	else
		return WindowState::Normal;
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	// leave fullscreen mode if needed
	SDL_WindowFlags f = SDL_GetWindowFlags(_sdl.window);
	if(f & SDL_WINDOW_FULLSCREEN) {
		if(windowState == WindowState::FullScreen)
			return;
		else
			SDL_SetWindowFullscreen(_sdl.window, false);
	}

	// change window state
	switch(windowState) {
	case WindowState::Hidden:
		hide();
		break;
	case WindowState::Minimized:
		if(!SDL_MinimizeWindow(_sdl.window))
			throw runtime_error("VulkanWindow::setWindowState(): Failed to minimize window.");
		show();
		break;
	case WindowState::Normal:
		if(!SDL_RestoreWindow(_sdl.window))
			throw runtime_error("VulkanWindow::setWindowState(): Failed to restore window.");
		show();
		break;
	case WindowState::Maximized:
		if(!SDL_MaximizeWindow(_sdl.window))
			throw runtime_error("VulkanWindow::setWindowState(): Failed to maximize window.");
		show();
		break;
	case WindowState::FullScreen:
		if(!SDL_SetWindowFullscreen(_sdl.window, true))
			throw runtime_error("VulkanWindow::setWindowState(): Failed to make window fullscreen.");
		show();
		break;
	default:
		throw runtime_error("VulkanWindow::setWindowState(): Invalid WindowState value passed as parameter.");
	}
}

#elif defined(USE_PLATFORM_SDL2)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_sdl.window)
		return WindowState::Hidden;

	Uint32 f = SDL_GetWindowFlags(_sdl.window);
	if(f & SDL_WINDOW_HIDDEN)
		return WindowState::Hidden;
	if(f & SDL_WINDOW_MINIMIZED)
		return WindowState::Minimized;
	if(f & SDL_WINDOW_FULLSCREEN)
		return WindowState::FullScreen;
	if(f & SDL_WINDOW_MAXIMIZED)
		return WindowState::Maximized;
	else
		return WindowState::Normal;
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	// leave fullscreen mode if needed
	Uint32 f = SDL_GetWindowFlags(_sdl.window);
	if(f & SDL_WINDOW_FULLSCREEN) {
		if(windowState == WindowState::FullScreen)
			return;
		else
			SDL_SetWindowFullscreen(_sdl.window, 0);  // leave full screen mode
	}

	// change window state
	switch(windowState) {
	case WindowState::Hidden:     hide(); break;
	case WindowState::Minimized:  show(); SDL_MinimizeWindow(_sdl.window); break;
	case WindowState::Normal:     SDL_RestoreWindow(_sdl.window); show(); break;
	case WindowState::Maximized:  SDL_MaximizeWindow(_sdl.window); show(); break;
	case WindowState::FullScreen: if(SDL_SetWindowFullscreen(_sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
		                              throw runtime_error("VulkanWindow::setWindowState(): Failed to make window fullscreen.");
		                          show();
		                          break;
	default: throw runtime_error("VulkanWindow::setWindowState(): Invalid WindowState value passed as parameter.");
	}
}

#elif defined(USE_PLATFORM_GLFW)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_glfw.window)
		return WindowState::Hidden;

	if(!glfwGetWindowAttrib(_glfw.window, GLFW_VISIBLE))
		return WindowState::Hidden;
	if(glfwGetWindowAttrib(_glfw.window, GLFW_ICONIFIED))
		return WindowState::Minimized;
	if(glfwGetWindowMonitor(_glfw.window) != nullptr)
		return WindowState::FullScreen;
	if(glfwGetWindowAttrib(_glfw.window, GLFW_MAXIMIZED))
		return WindowState::Maximized;
	else
		return WindowState::Normal;
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	// leave fullscreen mode if needed
	if(glfwGetWindowMonitor(_glfw.window) != nullptr) {
		if(windowState == WindowState::FullScreen)
			return;
		else {
			if(windowState == WindowState::Hidden || windowState == WindowState::Normal ||
			   windowState == WindowState::Maximized)
				glfwSetWindowMonitor(_glfw.window, nullptr, _glfw.savedPosX, _glfw.savedPosY, _glfw.savedWidth, _glfw.savedHeight, 0);
			if(windowState == WindowState::Maximized)
				glfwRestoreWindow(_glfw.window);  // this seems necessary to transition correctly from full screen to maximized state (seen on glfw 3.3.8 on 2025-02-14)
		}
	}

	// change window state
	switch(windowState) {
	case WindowState::Hidden:     hide(); break;
	case WindowState::Minimized:  glfwIconifyWindow(_glfw.window); show(); break;
	case WindowState::Normal:     glfwRestoreWindow(_glfw.window); show(); break;
	case WindowState::Maximized:  glfwMaximizeWindow(_glfw.window); show(); break;
	case WindowState::FullScreen: {
			GLFWmonitor* m = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(m);
			glfwSetWindowMonitor(_glfw.window, m, 0, 0, mode->width, mode->height, mode->refreshRate);
			show();
			break;
		}
	default: throw runtime_error("VulkanWindow::setWindowState(): Invalid WindowState value passed as parameter.");
	}
}

#elif defined(USE_PLATFORM_QT)

VulkanWindow::WindowState VulkanWindow::windowState() const
{
	if(!_qt.window || !_qt.window->isVisible())
		return WindowState::Hidden;

	switch(_qt.window->windowState()) {
	case Qt::WindowMinimized:   return WindowState::Minimized;
	case Qt::WindowNoState:     return WindowState::Normal;
	case Qt::WindowMaximized:   return WindowState::Maximized;
	case Qt::WindowFullScreen:  return WindowState::FullScreen;
	default: throw runtime_error("VulkanWindow::windowState(): Unknown WindowState value.");
	}
}

void VulkanWindow::setWindowState(WindowState windowState)
{
	switch(windowState) {
	case WindowState::Hidden:     hide(); break;
	case WindowState::Minimized:  _qt.window->showMinimized(); break;
	case WindowState::Normal:     _qt.window->showNormal(); break;
	case WindowState::Maximized:  _qt.window->showMaximized(); break;
	case WindowState::FullScreen: _qt.window->showFullScreen(); break;
	default: throw runtime_error("VulkanWindow::setWindowState(): Invalid WindowState value passed as parameter.");
	}
}

#endif



constexpr VulkanWindow::KeyCode VulkanWindow::fromUtf8(const char* s)
{
	// decode single character
	char ch1 = *s;
	if(ch1 < 0x80)
		return KeyCode(ch1);

	// decode 2 character sequence
	if(ch1 < 0xe0) {
		if(ch1 < 0xc0)
			return KeyCode(0xfffd);  // return "replacement character" indicating the error
		s++;
		char ch2 = *s;
		return KeyCode((ch1&0x1f)<<6 | (ch2&0x3f));
	}

	// decode 3 character sequence
	if(ch1 < 0xf0) {
		s++;
		char ch2 = *s;
		s++;
		char ch3 = *s;
		return KeyCode((ch1&0x0f)<<12 | (ch2&0x3f)<<6 | (ch3&0x3f));
	}

	// decode 4 character sequence
	if(ch1 < 0xf8) {
		s++;
		char ch2 = *s;
		s++;
		char ch3 = *s;
		s++;
		char ch4 = *s;
		return KeyCode((ch1&0x07)<<18 | (ch2&0x3f)<<12 | (ch3&0x3f)<<6 | (ch4&0x3f));
	}

	// more than 4 character sequences are forbidden
	return KeyCode(0xfffd);  // return "replacement character" indicating the error
}


array<char, 5> VulkanWindow::toCharArray(VulkanWindow::KeyCode keyCode)
{
	// write 1 byte sequence
	array<char, 5> r;
	uint32_t k = uint32_t(keyCode);
	if(k < 128) {
		*reinterpret_cast<uint32_t*>(r.data()) = k;
		r[4] = 0;
		return r;
	}

	// write 2 byte sequence
	if(k < 2048) {
		*reinterpret_cast<uint32_t*>(r.data()) = 0xc0 | ((k&0x07c0)>>6) | 0x8000 | ((k&0x3f)<<8);
		r[4] = 0;
		return r;
	}

	// write 3 byte sequence
	if(k < 65536) {
		*reinterpret_cast<uint32_t*>(r.data()) = 0xe0 | ((k&0xf000)>>12) | 0x8000 | ((k&0x0fc0)<<2) |
		                                         0x800000 | ((k&0x3f)<<16);
		r[4] = 0;
		return r;
	}

	// write 4 byte sequence
	if(k < 2097152) {
		*reinterpret_cast<uint32_t*>(r.data()) = 0xf0 | ((k&0x1c0000)>>18) | 0x8000 | ((k&0x03f000)>>4) |
		                                         0x800000 | ((k&0x0fc0)<<10) | 0x80000000 | ((k&0x3f)<<24);
		r[4] = 0;
		return r;
	}

	// more than 4 character sequences are forbidden
	// (return "replacement character" (value 0xfffd) indicating the error)
	*reinterpret_cast<uint32_t*>(r.data()) = 0xe0 | (0xf000>>12) | 0x8000 | (0x0fc0<<2) |
	                                         0x800000 | 0x3d<<16;
	r[4] = 0;
	return r;
}


string VulkanWindow::toString(KeyCode keyCode)
{
	string s;
	s.reserve(4);  // this shall allocate at least 4 chars and one null byte, e.g. at least 5 bytes
	s.assign(toCharArray(keyCode).data());
	return s;
}



static int niftyCounter = 0;


VulkanWindowInitAndFinalizer::VulkanWindowInitAndFinalizer()
{
	niftyCounter++;
}


VulkanWindowInitAndFinalizer::~VulkanWindowInitAndFinalizer()
{
	if(--niftyCounter == 0)
		VulkanWindow::finalize();
}
