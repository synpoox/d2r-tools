# MultiSpec: D2RLoader plugin (D2R 3.3 build 93847, D2RLoader 1.3.1, PluginSDK 0.3.0).
#
# Build:  powershell -ExecutionPolicy Bypass -File build.ps1            (or by hand:)
#         cmake -S . -B build -A x64 -DMULTISPEC_SDK_DIR=<PluginSDK>
#         cmake --build build --config Release
# Output: build/Release/multispec.dll -> <game>/mods/<Mod>/d2rloader/plugins/
#
# MULTISPEC_SDK_DIR defaults to the toolkit next to this repo (../d2rAI-toolkit/third_party/PluginSDK).
# -DMULTISPEC_TESTS=ON also builds multispec_tests (the portable core's unit tests).
cmake_minimum_required(VERSION 3.29)
project(multispec LANGUAGES CXX RC)

set(MULTISPEC_VERSION "1.0.0" CACHE STRING "PluginInfo::version (shared plugins must match in multiplayer)")
set(MULTISPEC_MOD_NAME "" CACHE STRING "Mod folder the plugin ships in (mods/<name>); the environment guard warns when another runs")
set(MULTISPEC_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK" CACHE PATH "D2RLoader PluginSDK directory")
option(MULTISPEC_TESTS "Build the unit tests of the portable core" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${MULTISPEC_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(multispec SHARED
	src/plugin.cpp
	src/d2r.cpp
	src/spec_model.cpp
	src/swap.cpp
	src/spec_engine.cpp
	src/spec_store.cpp
	src/log.cpp
	src/off_switch.cpp
	src/env_guard.cpp
	src/spec_panel.cpp
	src/spec_panel_layout.cpp
	src/multispec.rc
)
target_link_libraries(multispec PRIVATE D2RLPlugin::D2RLPlugin shell32 ole32)
target_compile_definitions(multispec PRIVATE
	MULTISPEC_VERSION="${MULTISPEC_VERSION}"
	MULTISPEC_MOD_NAME="${MULTISPEC_MOD_NAME}"
)
set_target_properties(multispec PROPERTIES PREFIX "")  # multispec.dll with every generator
d2rlplugin_embed_config(TARGET multispec FILE multispec.toml)
if(MSVC)
	target_compile_options(multispec PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET multispec PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime needed
endif()

if(MULTISPEC_TESTS)
	add_executable(multispec_tests tests/test_spec.cpp src/spec_model.cpp src/swap.cpp src/spec_panel_layout.cpp)
	if(MSVC)
		set_property(TARGET multispec_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
	endif()
endif()
