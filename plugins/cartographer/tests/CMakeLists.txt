# Host tests for the layout generator: no game, no SDK. doctest is vendored in third_party/doctest.
add_executable(layout_tests layout_tests.cpp ../src/layout/layout.cpp)
target_include_directories(layout_tests PRIVATE ../src/layout ../third_party/doctest)
if(MSVC)
	target_compile_options(layout_tests PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(layout_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME layout_tests COMMAND layout_tests)

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
if(MSVC)
	target_compile_options(tables_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
	target_compile_options(tables_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME tables_tests COMMAND tables_tests)
