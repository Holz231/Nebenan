cmake_minimum_required(VERSION 3.22)

project(nebenan
	VERSION 0.1.0
	DESCRIPTION "Polygonal real-time destruction on top of Box3D"
	LANGUAGES C CXX
)

option(NEBENAN_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(NEBENAN_BENCHMARK "Build the benchmark" ${PROJECT_IS_TOP_LEVEL})
option(NEBENAN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

option(NEBENAN_DEMO "Build the desktop demo" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Box3D links the static C runtime on MSVC and all targets have to agree. As a bonus the demo runs
# without the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Box3D ships in extern/box3d at a fixed commit, so every build uses the same physics code and needs no
# download. A project that embeds Nebenan and has its own box3d::box3d target keeps that one.
if(NOT TARGET box3d::box3d)
	if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
		add_subdirectory(extern/box3d SYSTEM)
	else()
		add_subdirectory(extern/box3d)
	endif()
endif()

add_subdirectory(src)

if(NEBENAN_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

if(NEBENAN_BENCHMARK)
	add_subdirectory(benchmark)
endif()

if(NEBENAN_DEMO)
	add_subdirectory(demo)

	# Start the demo by default in Visual Studio
	if(MSVC AND TARGET nebenan_demo)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT nebenan_demo)
	endif()
endif()
