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
* QT6
* QT5

## Compile

Prerequisities:
* Windows: Microsoft Visual C++ 2022 (earlier versions not tested)
* Linux: gcc, cmake, vulkan development files and tools,
  additional libraries depending on selected back-end (Xlib, Wayland, SDL3,...).
  This usually translates to the following required packages on Ubuntu Linux distribution:
  * build-essential
  * cmake (or cmake-curses-gui)
  * libvulkan-dev and glslang-tools
  * libx11-dev (if native xlib support is desired)
  * libwayland-dev and wayland-protocols (if native wayland support is desired)
  * pkg-config (optional - helps cmake to find wayland-protocols path)
