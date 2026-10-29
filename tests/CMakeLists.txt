# Host tests for the plugin's decision logic (no game, no Windows, no SDK).
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(quick_npc_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

add_executable(logic_tests logic_tests.cpp ../plugin/src/quick_npc_logic.cpp)
target_include_directories(logic_tests PRIVATE ../plugin/src)
if(MSVC)
	target_compile_options(logic_tests PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(logic_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME logic_tests COMMAND logic_tests)
