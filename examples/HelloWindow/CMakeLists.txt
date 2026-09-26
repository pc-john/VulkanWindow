# SPDX-FileCopyrightText: 2022-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

set(APP_NAME HelloWindow)

project(${APP_NAME})

set(APP_SOURCES
	main.cpp
)

set(APP_INCLUDES
)

set(APP_SHADERS
)

# dependencies
find_package(Vulkan REQUIRED)

# executable
add_executable(${APP_NAME} ${APP_SOURCES} ${APP_INCLUDES})
Vulkan_add_shaders(${APP_NAME} "${APP_SHADERS}")

# VulkanWindow
include("../../src/VulkanWindow.cmake")
VulkanWindowConfigure(${APP_NAME})

# target
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 17)

# target
target_link_libraries(${APP_NAME} Vulkan::Vulkan)
