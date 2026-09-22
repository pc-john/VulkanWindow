# SPDX-FileCopyrightText: 2022-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

function(VulkanWindowConfigure target)

	# set VULKAN_WINDOW_GUI if not already set or if set to "default" string
	string(TOLOWER "${VULKAN_WINDOW_GUI}" vulkanWindowGuiLowerCased)
	if(NOT VULKAN_WINDOW_GUI OR ("${vulkanWindowGuiLowerCased}" STREQUAL "default"))

		# detect recommended GUI type
		if(WIN32)
			set(gui "Win32")
		elseif(UNIX)
			find_package(Wayland)
			if(Wayland_client_FOUND AND Wayland_SCANNER AND Wayland_PROTOCOLS_DIR)
				set(gui "Wayland")
			else()
				find_package(X11)
				if(X11_FOUND)
					set(gui "Xlib")
				else()
					# default to Wayland on Linux
					set(gui "Wayland")
				endif()
			endif()
		endif()
		set(VULKAN_WINDOW_GUI ${gui} CACHE STRING "Vulkan Window platform used to implement GUI. Accepted values: default, Win32, Xlib, Wayland, SDL3, SDL2, GLFW, Qt6 and Qt5." FORCE)

	endif()

	# give error on invalid VULKAN_WINDOW_GUI
	set(guiList "Win32" "Xlib" "Wayland" "SDL3" "SDL2" "GLFW" "Qt6" "Qt5")
	if(NOT VULKAN_WINDOW_GUI IN_LIST guiList)
		message(FATAL_ERROR "VULKAN_WINDOW_GUI value is invalid. It must be set to default, Win32, Xlib, Wayland, SDL3, SDL2, GLFW, Qt6 or Qt5.")
	endif()

	# provide a list of valid values in CMake GUI
	set_property(CACHE VULKAN_WINDOW_GUI PROPERTY STRINGS ${guiList})


	# append VulkanWindow source files to the target
	target_sources(${target} PRIVATE
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.h"
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp")

	# append VulkanWindow include directory
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")

	# append VulkanWindow directory to the module search path
	set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_FUNCTION_LIST_DIR};${CMAKE_MODULE_PATH}")


	# platform specific stuff
	if("${VULKAN_WINDOW_GUI}" STREQUAL "Win32")

		# configure for Win32
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_WIN32)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Xlib")

		# configure for Xlib
		find_package(X11 REQUIRED)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_XLIB)
		target_link_libraries(${target} X11 -l:libxkbcommon.so.0)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Wayland")

		# detect paths
		find_path(Wayland_client_INCLUDE_DIR NAMES wayland-client.h)
		find_path(Wayland_cursor_INCLUDE_DIR NAMES wayland-cursor.h)
		find_library(Wayland_client_LIBRARY  NAMES wayland-client)
		find_library(Wayland_cursor_LIBRARY  NAMES wayland-cursor)
		find_program(Wayland_SCANNER         NAMES wayland-scanner)

		# Wayland protocols directory
		find_package(PkgConfig QUIET)
		pkg_check_modules(Wayland_PROTOCOLS wayland-protocols QUIET)
		if(PKG_CONFIG_FOUND AND Wayland_PROTOCOLS_FOUND)
			pkg_get_variable(Wayland_PROTOCOLS_DIR wayland-protocols pkgdatadir)
		endif()
		set(Wayland_PROTOCOLS_DIR "${Wayland_PROTOCOLS_DIR}" CACHE PATH "Wayland protocols directory.")

		# Wayland::client target
		if(Wayland_client_INCLUDE_DIR AND Wayland_client_LIBRARY)
			set(Wayland_client_FOUND TRUE)
			if(NOT TARGET Wayland::client)
				add_library(Wayland::client UNKNOWN IMPORTED)
				set_target_properties(Wayland::client PROPERTIES
					IMPORTED_LOCATION "${Wayland_client_LIBRARY}"
					INTERFACE_INCLUDE_DIRECTORIES "${Wayland_client_INCLUDE_DIR}")
			endif()
		endif()

		# Wayland::cursor target
		if(Wayland_cursor_INCLUDE_DIR AND Wayland_cursor_LIBRARY)
			set(Wayland_cursor_FOUND TRUE)
			if(NOT TARGET Wayland::cursor)
				add_library(Wayland::cursor UNKNOWN IMPORTED)
				set_target_properties(Wayland::cursor PROPERTIES
					IMPORTED_LOCATION "${Wayland_cursor_LIBRARY}"
					INTERFACE_INCLUDE_DIRECTORIES "${Wayland_cursor_INCLUDE_DIR}")
			endif()
		endif()

		if(Wayland_client_FOUND AND Wayland_SCANNER AND Wayland_PROTOCOLS_DIR)

			# Wayland protocols
			add_custom_command(OUTPUT xdg-shell-client-protocol.h
			                   COMMAND ${Wayland_SCANNER} client-header ${Wayland_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h)
			add_custom_command(OUTPUT xdg-shell-protocol.c
			                   COMMAND ${Wayland_SCANNER} private-code  ${Wayland_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c)
			add_custom_command(OUTPUT xdg-decoration-client-protocol.h
			                   COMMAND ${Wayland_SCANNER} client-header ${Wayland_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml xdg-decoration-client-protocol.h)
			add_custom_command(OUTPUT xdg-decoration-protocol.c
			                   COMMAND ${Wayland_SCANNER} private-code  ${Wayland_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml xdg-decoration-protocol.c)

			# target and sources
			target_sources(${target} PRIVATE xdg-shell-protocol.c xdg-decoration-protocol.c
			                                 xdg-shell-client-protocol.h xdg-decoration-client-protocol.h)
			set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_WAYLAND)
			target_link_libraries(${target} Wayland::client Wayland::cursor -lrt -l:libxkbcommon.so.0)

		else()
			message(FATAL_ERROR "Not all Wayland variables were detected properly.")
		endif()

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "SDL3")

		# configure for SDL3
		# (SDL3Config.cmake file is distributed with precompiled binaries on Win32
		# with all SDL3 versions)
		find_package(SDL3 CONFIG REQUIRED)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_SDL3)
		target_link_libraries(${target} SDL3::SDL3)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${target}> $<TARGET_RUNTIME_DLLS:${target}> COMMAND_EXPAND_LISTS)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "SDL2")

		# configure for SDL2
		# (SDL2Config.cmake file is distributed with precompiled binaries on Win32
		# since SDL2 version 2.24.0. For CMake 4.x and latest CMake 3.x versions
		# you might need latest SDL2 versions to be compatible with
		# cmake_minimum_required inside SDL2Config.cmake. Otherwise, you might get
		# deprecation warning or even CMake compatibility error with CMake 4.x.)
		find_package(SDL2 REQUIRED)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_SDL2)
		target_link_libraries(${target} SDL2::SDL2)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${target}> $<TARGET_RUNTIME_DLLS:${target}> COMMAND_EXPAND_LISTS)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Qt6")

		# configure for Qt6
		# (Point Qt6_DIR to lib/cmake/Qt6 in your Qt installation.)
		find_package(Qt6 REQUIRED COMPONENTS Core Gui)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_QT)
		target_link_libraries(${target} Qt6::Gui)

		# copy dependencies on Win32
		if(WIN32)
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND Qt6::windeployqt
						--no-translations  # skip Qt translations
						--no-opengl-sw  # skip software OpenGL
						--no-system-d3d-compiler  # skip D3D stuff
						--no-svg  # skip svg support
						$<TARGET_FILE_DIR:${target}>
						COMMENT "Deploying Qt related dependencies...")
		endif()

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Qt5")

		# configure for Qt5
		# (version 5.10 brings Vulkan support, latest Qt 5.15.x might be needed
		# on C++20 or in other circumstances)
		find_package(Qt5 5.10 REQUIRED COMPONENTS Core Gui)
		find_package(Vulkan)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_QT)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIR}")
		target_link_libraries(${target} Qt5::Gui)

		# copy dependencies on Win32
		if(WIN32)

			# windeployqt path
			get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
			get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
			set(QT5_WINDEPLOYQT_EXECUTABLE "${_qt_bin_dir}/windeployqt.exe")

			# deploy command
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND "${QT5_WINDEPLOYQT_EXECUTABLE}"
						--no-translations  # skip Qt translations
						--no-widgets  # skip Qt widgets
						--no-opengl-sw  # skip software OpenGL
						--no-angle  # skip software OpenGL (ANGLE)
						$<TARGET_FILE_DIR:${target}>
						COMMENT "Deploying Qt related dependencies...")

		endif()

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "GLFW")

		# find GLFW include path
		find_path(glfw3_INCLUDE_DIR GLFW/glfw3.h)

		# find GLFW library
		if(WIN32)
			find_library(glfw3_LIBRARY
				NAMES
					glfw3.lib glfw3_mt.lib glfw3dll.lib
			)
		else()
			find_library(glfw3_LIBRARY
				NAMES
					libglfw.so libglfw.so.3
			)
		endif()

		# find GLFW DLL
		# (leave it empty if you are not using glfw3dll.lib to not copy dll that is not used)
		if(WIN32)
			find_file(glfw3_DLL
				NAMES
					glfw3.dll
			)
		endif()

		# configure for GLFW
		# (No glfw3Config.cmake provided by glfw library for precompiled Win64 MSVC build
		# even for version 3.5.1. So, we go without glfw3 target.)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_GLFW)
		set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VulkanWindow.cpp" PROPERTY INCLUDE_DIRECTORIES "${glfw3_INCLUDE_DIR}")
		target_link_libraries(${target} "${glfw3_LIBRARY}")
		if(WIN32 AND glfw3_DLL)
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${glfw3_DLL}" $<TARGET_FILE_DIR:${target}>)
		endif()

	else()
		message(FATAL_ERROR "Invalid VULKAN_WINDOW_GUI value: ${VULKAN_WINDOW_GUI}")
	endif()

endfunction()
