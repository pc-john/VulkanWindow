# SPDX-FileCopyrightText: 2022-2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
#
# SPDX-License-Identifier: MIT-0

macro(VulkanWindowConfigure target vulkanWindowHeaderFile vulkanWindowCppFile)

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
	target_sources(${target} PRIVATE "${vulkanWindowHeaderFile}" "${vulkanWindowCppFile}")

	# append include directory to the target to find VulkanWindow header file
	get_filename_component(vulkanWindowHeaderPath "${vulkanWindowHeaderFile}" DIRECTORY)
	if(NOT "${vulkanWindowHeaderPath}" STREQUAL "${CMAKE_SOURCE_DIR}")
		target_include_directories(${target} PRIVATE "${vulkanWindowHeaderPath}")
	endif()


	# platform specific stuff
	if("${VULKAN_WINDOW_GUI}" STREQUAL "Win32")

		# configure for Win32
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_WIN32)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Xlib")

		# configure for Xlib
		find_package(X11 REQUIRED)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_XLIB)
		target_link_libraries(${target} X11 -l:libxkbcommon.so.0)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Wayland")

		# configure for Wayland
		find_package(Wayland REQUIRED)

		if(Wayland_client_FOUND AND Wayland_SCANNER AND Wayland_PROTOCOLS_DIR)

			add_custom_command(OUTPUT xdg-shell-client-protocol.h
			                   COMMAND ${Wayland_SCANNER} client-header ${Wayland_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h)
			add_custom_command(OUTPUT xdg-shell-protocol.c
			                   COMMAND ${Wayland_SCANNER} private-code  ${Wayland_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c)
			add_custom_command(OUTPUT xdg-decoration-client-protocol.h
			                   COMMAND ${Wayland_SCANNER} client-header ${Wayland_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml xdg-decoration-client-protocol.h)
			add_custom_command(OUTPUT xdg-decoration-protocol.c
			                   COMMAND ${Wayland_SCANNER} private-code  ${Wayland_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml xdg-decoration-protocol.c)

			target_sources(${target} PRIVATE xdg-shell-protocol.c xdg-decoration-protocol.c
			                                 xdg-shell-client-protocol.h xdg-decoration-client-protocol.h)
			set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_WAYLAND)
			target_link_libraries(${target} Wayland::client Wayland::cursor -lrt -l:libxkbcommon.so.0)

		else()
			message(FATAL_ERROR "Not all Wayland variables were detected properly.")
		endif()

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "SDL3")

		# configure for SDL3
		find_package(SDL3 REQUIRED)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_SDL3)
		target_link_libraries(${target} SDL3::SDL3)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "SDL2")

		# configure for SDL2
		find_package(SDL2 REQUIRED)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_SDL2)
		target_link_libraries(${target} SDL2::SDL2)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "GLFW")

		# configure for GLFW
		find_package(glfw3 3.3 REQUIRED)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_GLFW)
		target_link_libraries(${target} glfw)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Qt6")

		# configure for Qt6
		find_package(Qt6 REQUIRED COMPONENTS Core Gui)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_QT)
		target_link_libraries(${target} Qt6::Gui)

	elseif("${VULKAN_WINDOW_GUI}" STREQUAL "Qt5")

		# configure for Qt5
		# (we need at least version 5.10 because of Vulkan support)
		find_package(Qt5 5.10 REQUIRED COMPONENTS Core Gui)
		set_property(SOURCE "${vulkanWindowCppFile}" PROPERTY COMPILE_FLAGS -DVULKAN_WINDOW_QT)
		target_link_libraries(${target} Qt5::Gui)
		if(WIN32)
			# windeployqt path
			get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
			get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
			set(QT5_WINDEPLOYQT_EXECUTABLE "${_qt_bin_dir}/windeployqt.exe")
		endif()

	else()
		message(FATAL_ERROR "Invalid VULKAN_WINDOW_GUI value: ${VULKAN_WINDOW_GUI}")
	endif()


	# copy DLLs and other stuff on Wind32 (SDL3.dll, SDL2.dll, glfw3.dll, Qt stuff,...)
	if(WIN32)
		if(${VULKAN_WINDOW_GUI} STREQUAL "SDL3" AND SDL3_DLL)
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL3_DLL}" $<TARGET_FILE_DIR:${target}>)
		elseif(${VULKAN_WINDOW_GUI} STREQUAL "SDL2" AND SDL2_DLL)
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_DLL}" $<TARGET_FILE_DIR:${target}>)
		elseif(${VULKAN_WINDOW_GUI} STREQUAL "GLFW" AND glfw3_DLL)
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${glfw3_DLL}" $<TARGET_FILE_DIR:${target}>)
		elseif(${VULKAN_WINDOW_GUI} STREQUAL "Qt6")
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND Qt6::windeployqt
						--no-translations  # skip Qt translations
						--no-opengl-sw  # skip software OpenGL
						--no-system-d3d-compiler  # skip D3D stuff
						--no-svg  # skip svg support
						$<TARGET_FILE_DIR:${target}>
						COMMENT "Deploying Qt related dependencies...")
		elseif(${VULKAN_WINDOW_GUI} STREQUAL "Qt5")
			add_custom_command(TARGET ${target}
				POST_BUILD COMMAND "${QT5_WINDEPLOYQT_EXECUTABLE}"
						--no-translations  # skip Qt translations
						--no-widgets  # skip Qt widgets
						--no-opengl-sw  # skip software OpenGL
						--no-angle  # skip software OpenGL (ANGLE)
						$<TARGET_FILE_DIR:${target}>
						COMMENT "Deploying Qt related dependencies...")
		endif()
	endif()

endmacro()
