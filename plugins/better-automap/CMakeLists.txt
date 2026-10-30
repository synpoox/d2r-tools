# Better Automap: D2RLoader plugin (D2R 3.3.0 build 93847, D2RLoader 1.3.1, PluginSDK 0.3.0).
#
# Built by GitHub Actions (.github/workflows/build.yml):
#   cmake -S . -B build -A x64
#   cmake --build build --config Release
# Output: build/Release/better-automap.dll -> <game>\d2rloader\plugins\ (game-wide)
cmake_minimum_required(VERSION 3.29)
project(better_automap LANGUAGES CXX RC)

set(BETTER_AUTOMAP_VERSION "0.1.0" CACHE STRING "PluginInfo::version")
set(BETTER_AUTOMAP_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PluginSDK" CACHE PATH "D2RLoader PluginSDK directory")

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${BETTER_AUTOMAP_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(better_automap SHARED
	src/plugin.cpp
	src/plugin.rc
	src/d2r.cpp
	src/config.cpp
	src/log.cpp
	src/off_switch.cpp
	src/minimap_side.cpp
	src/overlay_toggle.cpp
	src/overlay_zoom.cpp
)
target_link_libraries(better_automap PRIVATE D2RLPlugin::D2RLPlugin)
target_compile_definitions(better_automap PRIVATE BETTER_AUTOMAP_VERSION="${BETTER_AUTOMAP_VERSION}")
set_target_properties(better_automap PROPERTIES OUTPUT_NAME "better-automap")
# Default settings: D2RLoader writes <game>\d2rloader\config\better-automap.toml from this copy when it is missing.
d2rlplugin_embed_config(TARGET better_automap FILE "${CMAKE_CURRENT_SOURCE_DIR}/better-automap.toml")
if(MSVC)
	target_compile_options(better_automap PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc /utf-8>)
	set_property(TARGET better_automap PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()
