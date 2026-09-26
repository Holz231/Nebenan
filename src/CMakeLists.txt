set(NEBENAN_SOURCE_FILES
	core.c
	core.h
	fracture.c
	fracture.h
	hull_builder.c
	hull_builder.h
	impact.c
	destructible.c
	load.c
	poly.c
	poly.h
	scheduler.c
	scheduler.h
	world.c
	world.h
)

set(NEBENAN_INCLUDE_FILES
	../include/nebenan/base.h
	../include/nebenan/nebenan.h
	../include/nebenan/types.h
)

add_library(nebenan ${NEBENAN_SOURCE_FILES} ${NEBENAN_INCLUDE_FILES})
add_library(nebenan::nebenan ALIAS nebenan)

target_include_directories(nebenan
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)
target_link_libraries(nebenan PUBLIC box3d::box3d PRIVATE Threads::Threads)

set_target_properties(nebenan PROPERTIES
	C_STANDARD 17
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS YES
	VERSION ${PROJECT_VERSION}
)

if(NEBENAN_WARNINGS_AS_ERRORS)
	set_target_properties(nebenan PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

if(MSVC)
	target_compile_options(nebenan PRIVATE /W4 /wd4201)
	# Fracture patterns must not depend on floating point contraction.
	target_compile_options(nebenan PRIVATE /fp:precise)
else()
	target_compile_options(nebenan PRIVATE -Wall -Wextra -pedantic -Wmissing-prototypes -ffp-contract=off)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
	target_link_libraries(nebenan PUBLIC m)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "src" FILES ${NEBENAN_SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../include" PREFIX "include" FILES ${NEBENAN_INCLUDE_FILES})
