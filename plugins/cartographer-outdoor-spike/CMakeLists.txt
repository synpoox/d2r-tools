# THROWAWAY spike, never merged. Build: tools\build.ps1 -Plugin cartographer-outdoor-spike
cmake_minimum_required(VERSION 3.29)
project(cartographer_outdoor_spike LANGUAGES CXX RC)

set(D2RL_SDK_DIR "" CACHE PATH "D2RLoader PluginSDK directory")
if(NOT D2RL_SDK_DIR)
	message(FATAL_ERROR "Pass -DD2RL_SDK_DIR=<PluginSDK checkout>")
endif()

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${D2RL_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(cartographer_outdoor_spike SHARED
	src/plugin.cpp
	src/log.cpp
	src/spike.rc
)
target_compile_features(cartographer_outdoor_spike PRIVATE cxx_std_20)
target_link_libraries(cartographer_outdoor_spike PRIVATE D2RLPlugin::D2RLPlugin user32)
set_target_properties(cartographer_outdoor_spike PROPERTIES OUTPUT_NAME "cartographer-outdoor-spike")
d2rlplugin_embed_config(TARGET cartographer_outdoor_spike FILE cartographer-outdoor-spike.toml)
if(MSVC)
	target_compile_options(cartographer_outdoor_spike PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET cartographer_outdoor_spike PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
