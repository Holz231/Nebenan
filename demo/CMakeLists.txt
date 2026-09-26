# Nebenan desktop demo: sokol (D3D11 on Windows, Metal on macOS, OpenGL on Linux) and Dear ImGui, both in
# extern/. The sokol headers come with Box3D, so the demo uses exactly the version Box3D's samples use.

if(APPLE)
	enable_language(OBJC)
endif()

if(UNIX AND NOT APPLE)
	find_package(OpenGL QUIET)
	find_package(X11 QUIET)
	find_package(Threads REQUIRED)
	if(NOT OPENGL_FOUND OR NOT X11_FOUND OR NOT X11_Xi_FOUND OR NOT X11_Xcursor_FOUND)
		message(WARNING "Skipping the Nebenan demo: install the OpenGL, X11, Xi and Xcursor development packages "
			"(for example libgl-dev libx11-dev libxi-dev libxcursor-dev).")
		return()
	endif()
endif()

set(NEBENAN_IMGUI_DIR "${PROJECT_SOURCE_DIR}/extern/imgui")
add_library(nebenan_imgui STATIC
	${NEBENAN_IMGUI_DIR}/imgui.cpp
	${NEBENAN_IMGUI_DIR}/imgui_draw.cpp
	${NEBENAN_IMGUI_DIR}/imgui_tables.cpp
	${NEBENAN_IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(nebenan_imgui SYSTEM PUBLIC ${NEBENAN_IMGUI_DIR})
set_target_properties(nebenan_imgui PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

set(NEBENAN_SOKOL_DIR "${PROJECT_SOURCE_DIR}/extern/box3d/extern/sokol")

set(NEBENAN_DEMO_FILES
	demo.cpp
	imgui_impl.cpp
	math3d.h
	renderer.cpp
	renderer.h
	screenshot.c
	sokol_impl.c
	system_info.cpp
	system_info.h
	shaders/scene.glsl
	shaders/generated/scene.glsl.h
)

add_executable(nebenan_demo ${NEBENAN_DEMO_FILES})
target_include_directories(nebenan_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(nebenan_demo SYSTEM PRIVATE ${NEBENAN_SOKOL_DIR})
target_link_libraries(nebenan_demo PRIVATE nebenan nebenan_imgui)
set_target_properties(nebenan_demo PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	C_STANDARD 17
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_source_files_properties(shaders/scene.glsl PROPERTIES HEADER_FILE_ONLY TRUE)

if(WIN32)
	target_compile_definitions(nebenan_demo PRIVATE SOKOL_D3D11 NOMINMAX)
	target_link_libraries(nebenan_demo PRIVATE d3d11 dxgi dxguid user32 gdi32 shell32 ole32)
	if(MSVC)
		# A windowed program without console, main() stays the entry point
		target_link_options(nebenan_demo PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
	elseif(MINGW)
		target_link_options(nebenan_demo PRIVATE -mwindows)
	endif()
elseif(APPLE)
	target_compile_definitions(nebenan_demo PRIVATE SOKOL_METAL)
	set_source_files_properties(sokol_impl.c PROPERTIES LANGUAGE OBJC COMPILE_OPTIONS "-fobjc-arc")
	target_link_libraries(nebenan_demo PRIVATE
		"-framework Foundation" "-framework Cocoa" "-framework QuartzCore" "-framework Metal" "-framework MetalKit")
else()
	target_compile_definitions(nebenan_demo PRIVATE SOKOL_GLCORE)
	target_link_libraries(nebenan_demo PRIVATE OpenGL::GL X11::X11 X11::Xi X11::Xcursor Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${NEBENAN_DEMO_FILES})
