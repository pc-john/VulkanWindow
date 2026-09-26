# SPDX-FileCopyrightText: 2022-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

project(HelloWindow)

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
add_executable(${PROJECT_NAME} ${APP_SOURCES} ${APP_INCLUDES})
Vulkan_add_shaders(${PROJECT_NAME} "${APP_SHADERS}")

# VulkanWindow
include("../../src/VulkanWindow.cmake")
VulkanWindowConfigure(${PROJECT_NAME})

# target
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

# target
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
