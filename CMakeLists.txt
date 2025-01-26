cmake_minimum_required(VERSION 3.16.0)

project(VulkanWindow)

set(PROJECTS
	examples/helloWindow
	examples/mandelbrot
	tests/multiWindowTests
	tests/inputTest
)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/;${CMAKE_MODULE_PATH}")

foreach(prj ${PROJECTS})
	add_subdirectory(${prj})
endforeach()
