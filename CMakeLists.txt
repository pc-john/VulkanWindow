cmake_minimum_required(VERSION 3.16.0)

project(VulkanWindow)

set(PROJECTS
	examples/mandelbrot
	tests/multiWindowTests
)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/;${CMAKE_MODULE_PATH}")

foreach(prj ${PROJECTS})
	add_subdirectory(${prj})
endforeach()
