# Better XP Bar: D2RLoader plugin for D2R 3.3.0 build 93847 (image 0x6ab3782c), PluginSDK 0.3.0.
#
# Two parts:
#   core/    pure C++20 metrics (no game, no Windows): builds and tests on any platform.
#   plugin/  the D2RLoader DLL (Windows x64 only; MSVC or clang-cl, CMake 3.29+ for the SDK).
#
# Windows:  cmake -S . -B build -A x64 && cmake --build build --config Release   -> build/Release/better-xp-bar.dll
# Any OS:   cmake -S . -B build-tests -DBXB_BUILD_PLUGIN=OFF && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.20)
project(better_xp_bar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BXB_BUILD_TESTS "Build the core unit tests" ON)
if(WIN32)
	option(BXB_BUILD_PLUGIN "Build the D2RLoader plugin DLL" ON)
else()
	option(BXB_BUILD_PLUGIN "Build the D2RLoader plugin DLL" OFF)
endif()

set(BXB_PLUGIN_NAME "better-xp-bar" CACHE STRING "Plugin id, DLL name, console command and <name>-off.txt prefix")
set(BXB_PLUGIN_VERSION "1.0.0" CACHE STRING "PluginInfo::version")
set(BXB_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK" CACHE PATH "D2RLoader PluginSDK directory")

add_subdirectory(core)

if(BXB_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(BXB_BUILD_PLUGIN)
	cmake_minimum_required(VERSION 3.29)  # the PluginSDK's own minimum
	add_subdirectory(plugin)
endif()
