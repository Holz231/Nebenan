add_executable(nebenan_benchmark main.c)
target_link_libraries(nebenan_benchmark PRIVATE nebenan)
target_include_directories(nebenan_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_target_properties(nebenan_benchmark PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)

# Same floating point rules as the library, so runs on different platforms can be compared
if(NOT MSVC)
	target_compile_options(nebenan_benchmark PRIVATE -ffp-contract=off)
endif()
