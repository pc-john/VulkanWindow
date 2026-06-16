# SPDX-FileCopyrightText: 2025-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

set(APP_NAME ColorSpace)

project(${APP_NAME})

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
set(libs Vulkan::Vulkan)

# executable
add_shaders("${APP_SHADERS}" APP_SHADER_DEPS)
add_executable(${APP_NAME} ${APP_SOURCES} ${APP_INCLUDES} ${APP_SHADER_DEPS})

# target
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${APP_NAME} ${libs})

# VulkanWindow
include(${CMAKE_SOURCE_DIR}/CMakeModules/VulkanWindow.cmake)
VulkanWindowConfigure(${APP_NAME}
	${CMAKE_SOURCE_DIR}/src/VulkanWindow.h ${CMAKE_SOURCE_DIR}/src/VulkanWindow.cpp)
