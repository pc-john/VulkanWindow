# VulkanWindow
Simple rendering window for Vulkan applications. Its main focus is ease of use,
performance and multiplatform support.

It supports following platforms / back-ends:
* native Win32 API
* native Xlib API
* native Wayland API
* SDL3
* SDL2
* GLFW3
* Qt6
* Qt5

## Compile

Prerequisities:
* Windows: Microsoft Visual C++ 2026 or 2022 (earlier versions not tested)
* Linux: gcc, cmake, vulkan development files and tools,
  additional libraries depending on selected back-end (Xlib, Wayland, SDL3,...).
  This usually translates to the following required packages on Ubuntu Linux distributions:
  * build-essential
  * cmake (or cmake-curses-gui)
  * libvulkan-dev and glslang-tools
  * pkg-config (optional - helps cmake to find wayland-protocols path)
  Packages depending on selected back-end:
  * Win32:
    * none
  * Xlib:
    * libx11-dev
  * Wayland:
    * libwayland-dev and wayland-protocols
  * SDL3:
    * libsdl3-dev
  * SDL2:
    * libsdl2-dev
  * GLFW3:
    * libglfw3-dev
  * Qt6
    * qt6-base-dev
  * Qt5
    * qtbase5-dev
