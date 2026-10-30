# Better Auras: a D2RLoader plugin (D2R 3.3 build 93847, D2RLoader 1.3.1, PluginSDK 0.3.0), scaffolded from
# d2rAI-toolkit's plugin-template.
#
# Build:  powershell -ExecutionPolicy Bypass -File build.ps1            (or by hand:)
#         cmake -S . -B build -A x64
#         cmake --build build --config Release
# Output: build/Release/better-auras.dll -> copy it to <game>/d2rloader/plugins/ (a game-wide plugin). On its first
# start D2RLoader writes better-auras.toml (embedded below) to <game>/d2rloader/config/.
cmake_minimum_required(VERSION 3.29)
project(better_auras LANGUAGES CXX RC)

set(MODKIT_PLUGIN_NAME "better-auras" CACHE STRING "Plugin id, DLL name, console command and <name>-off.txt prefix")
set(MODKIT_PLUGIN_VERSION "0.1.0" CACHE STRING "PluginInfo::version (shared plugins must match in multiplayer)")
set(MODKIT_MOD_NAME "" CACHE STRING "Mod folder the plugin ships in (mods/<name>); \"\" = any mod")
set(MODKIT_SOUND_ROWS "0" CACHE STRING "Rows of the mod's sounds.txt (0 = no new sounds)")
set(MODKIT_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PluginSDK" CACHE PATH "D2RLoader PluginSDK directory")

if(NOT EXISTS "${MODKIT_SDK_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "PluginSDK not found at ${MODKIT_SDK_DIR}: run `git submodule update --init`")
endif()

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${MODKIT_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

# better-auras.toml becomes the defaults D2RLoader writes when the config is missing (EnsureConfig).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/better-auras.toml")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/better-auras.toml" BETTER_AURAS_DEFAULT_TOML)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h"
	"// Generated from better-auras.toml by CMakeLists.txt: do not edit.\n#pragma once\n"
	"inline constexpr const char* DefaultConfig = R\"BA_TOML(${BETTER_AURAS_DEFAULT_TOML})BA_TOML\";\n")

add_library(better_auras SHARED
	src/plugin.cpp
	src/d2r.cpp
	src/log.cpp
	src/off_switch.cpp
	src/env_guard.cpp
	src/sound_guard.cpp
	src/item_type_check.cpp
	src/config.cpp
	src/aura_api.cpp
	src/auras.cpp
	src/plugin.rc  # ABI manifest resource: D2RLoader rejects a plugin DLL without it
)
target_link_libraries(better_auras PRIVATE D2RLPlugin::D2RLPlugin)
target_include_directories(better_auras PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(better_auras PRIVATE
	MODKIT_PLUGIN_NAME="${MODKIT_PLUGIN_NAME}"
	MODKIT_PLUGIN_VERSION="${MODKIT_PLUGIN_VERSION}"
	MODKIT_MOD_NAME="${MODKIT_MOD_NAME}"
	MODKIT_SOUND_ROWS=${MODKIT_SOUND_ROWS}
)
set_target_properties(better_auras PROPERTIES OUTPUT_NAME "${MODKIT_PLUGIN_NAME}")
if(MSVC)
	target_compile_options(better_auras PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc /utf-8>)
	set_property(TARGET better_auras PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()
