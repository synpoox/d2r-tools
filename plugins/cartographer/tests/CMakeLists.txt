# Host tests for the layout generator: no game, no SDK. doctest is vendored in third_party/doctest.
add_executable(layout_tests layout_tests.cpp ../src/layout/layout.cpp)
target_include_directories(layout_tests PRIVATE ../src/layout ../third_party/doctest)
if(MSVC)
	target_compile_options(layout_tests PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(layout_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME layout_tests COMMAND layout_tests)
