# Minimal wiring for the pure layout generator (phase 2), so tools\build.ps1 -Plugin cartographer -Test can build and
# run its host tests. The plugin skeleton (#20) replaces this file; keep its layout library and tests lines.
cmake_minimum_required(VERSION 3.20)
project(cartographer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CARTOGRAPHER_SDK_DIR only exists so toolsuild.ps1 finds the SDK path it looks for; nothing here uses it yet.
set(CARTOGRAPHER_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK" CACHE PATH "PluginSDK checkout")
option(CARTOGRAPHER_TESTS "Build the host unit tests" OFF)

# The layout generator: pure C++, no engine headers. SHARED is a stop-gap so build.ps1 has a DLL to stage until the
# plugin skeleton (#20) replaces this file; it is not a D2RLoader plugin, so do not copy dist\cartographer into a game.
add_library(cartographer_layout SHARED src/layout/layout.cpp src/layout/layout.h src/layout/rng.h)
target_include_directories(cartographer_layout PUBLIC src/layout)
set_target_properties(cartographer_layout PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(MSVC)
	target_compile_options(cartographer_layout PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(cartographer_layout PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CARTOGRAPHER_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
