# One test per core: tests/<core>_tests.cpp becomes test <core>_tests, linked to cartographer_<core> and doctest.
# A new core's tests need no edit here. doctest is vendored in third_party/doctest.
file(GLOB cartographer_core_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_tests.cpp)
list(SORT cartographer_core_tests)
foreach(test_source IN LISTS cartographer_core_tests)
	get_filename_component(test_name ${test_source} NAME_WE)
	string(REGEX REPLACE "_tests$" "" core ${test_name})
	if(NOT TARGET cartographer_${core})
		continue()  # tables_tests is declared below, against cartographer_core
	endif()
	add_executable(${test_name} ${test_source})
	target_link_libraries(${test_name} PRIVATE cartographer_${core})
	target_include_directories(${test_name} PRIVATE ../third_party/doctest)
	if(MSVC)
		target_compile_options(${test_name} PRIVATE /W4 /permissive- /EHsc /utf-8)
		set_property(TARGET ${test_name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # match the static library
	else()
		target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
	endif()
	add_test(NAME ${test_name} COMMAND ${test_name})
	target_compile_definitions(${test_name} PRIVATE CARTOGRAPHER_EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples/first-map")
endforeach()

# Catalogue tool tests (Python). Kept separate so it survives PR #42's CMake restructure.
# CARTOGRAPHER_NO_DATA=1: CI has no game data.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	add_test(NAME catalogue_tool_tests
		COMMAND ${Python3_EXECUTABLE} -I -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/../tools)
	set_tests_properties(catalogue_tool_tests PROPERTIES ENVIRONMENT "CARTOGRAPHER_NO_DATA=1")
else()
	message(STATUS "Python not found: skipping catalogue tool tests")
endif()

# Host tests for the table loader, the gates and the signature scanner.
add_executable(tables_tests tables_tests.cpp ../src/core/tables.cpp)
target_include_directories(tables_tests PRIVATE ../src ../third_party/doctest)
target_compile_definitions(tables_tests PRIVATE CARTOGRAPHER_EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples/first-map")
if(MSVC)
	target_compile_options(tables_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
	target_compile_options(tables_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME tables_tests COMMAND tables_tests)
