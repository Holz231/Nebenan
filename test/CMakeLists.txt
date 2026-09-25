set(NEBENAN_TEST_FILES
	main.c
	test_macros.h
	test_poly.c
	test_world.c
)

add_executable(nebenan_test ${NEBENAN_TEST_FILES})

# The tests reach into internal headers to check the geometry kernel directly
target_include_directories(nebenan_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(nebenan_test PRIVATE nebenan)

set_target_properties(nebenan_test PROPERTIES
	C_STANDARD 17
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS YES
)

if(NOT MSVC)
	target_compile_options(nebenan_test PRIVATE -ffp-contract=off)
endif()

add_test(NAME nebenan_test COMMAND nebenan_test)
