# Quick NPC D2RLoader plugin (D2R 3.3.0 build 93847, D2RLoader 1.3.1, PluginSDK 0.3.0), from d2rAI-toolkit's
# plugin-template.
#
# Build:  powershell -ExecutionPolicy Bypass -File build.ps1            (or by hand:)
#         cmake -S . -B build -A x64
#         cmake --build build --config Release
# Output: build/Release/quick-npc.dll -> copy to <game>/mods/<Mod>/d2rloader/plugins/
#
# MODKIT_SDK_DIR points at the PluginSDK checkout (git submodule third_party/PluginSDK by default).
cmake_minimum_required(VERSION 3.29)
project(quick_npc_plugin LANGUAGES CXX RC)

set(MODKIT_PLUGIN_NAME "quick-npc" CACHE STRING "Plugin id, DLL name, console command and <name>-off.txt prefix")
set(MODKIT_PLUGIN_VERSION "0.1.0" CACHE STRING "PluginInfo::version")
set(MODKIT_MOD_NAME "" CACHE STRING "Mod folder the plugin ships in (mods/<name>); the environment guard warns when another runs")
set(MODKIT_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PluginSDK" CACHE PATH "D2RLoader PluginSDK directory")

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${MODKIT_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(quick_npc_plugin SHARED
	src/plugin.cpp
	src/plugin.rc
	src/quick_npc.cpp
	src/quick_npc_logic.cpp
	src/log.cpp
	src/off_switch.cpp
	src/env_guard.cpp
)
target_link_libraries(quick_npc_plugin PRIVATE D2RLPlugin::D2RLPlugin)
target_compile_definitions(quick_npc_plugin PRIVATE
	MODKIT_PLUGIN_NAME="${MODKIT_PLUGIN_NAME}"
	MODKIT_PLUGIN_VERSION="${MODKIT_PLUGIN_VERSION}"
	MODKIT_MOD_NAME="${MODKIT_MOD_NAME}"
)
set_target_properties(quick_npc_plugin PROPERTIES OUTPUT_NAME "${MODKIT_PLUGIN_NAME}")
if(MSVC)
	target_compile_options(quick_npc_plugin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc /utf-8>)
	set_property(TARGET quick_npc_plugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()
