# SPDX-FileCopyrightText: 2025-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

project(ColorSpace)

set(APP_SOURCES
	main.cpp
)

set(APP_INCLUDES
)

set(APP_SHADERS
	ChromacityDiagram-CIE1931.vert
	ChromacityDiagram-CIE2006.vert
	ColorSpace.vert
	InterpolatedColor.frag
)

# dependencies
find_package(Vulkan REQUIRED)

# executable
add_executable(${PROJECT_NAME} ${APP_SOURCES} ${APP_INCLUDES})
Vulkan_add_shaders(${PROJECT_NAME} "${APP_SHADERS}")

# VulkanWindow
include("../../src/VulkanWindow.cmake")
VulkanWindowConfigure(${PROJECT_NAME})

# target
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

# target
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
