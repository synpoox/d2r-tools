# dps-counter: the damage per second you deal to monsters, as text just above the life globe (D2RLoader plugin).
#
# Build:  tools\build.ps1 -Plugin dps-counter -Test (from the repo root)
# Output: build/dps-counter/Release/dps-counter.dll -> copy it to <game>/d2rloader/plugins/
cmake_minimum_required(VERSION 3.29)
project(dps_counter LANGUAGES CXX RC)

set(D2RL_SDK_DIR "" CACHE PATH "D2RLoader PluginSDK directory")
if(NOT D2RL_SDK_DIR)
	message(FATAL_ERROR "Pass -DD2RL_SDK_DIR=<PluginSDK checkout>")
endif()

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${D2RL_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(dps_counter SHARED
	src/plugin.cpp
	src/counter.cpp
	src/log.cpp
	src/dps-counter.rc
)
target_compile_features(dps_counter PRIVATE cxx_std_20)
target_link_libraries(dps_counter PRIVATE D2RLPlugin::D2RLPlugin user32)
set_target_properties(dps_counter PROPERTIES OUTPUT_NAME "dps-counter")
d2rlplugin_embed_config(TARGET dps_counter FILE dps-counter.toml)
if(MSVC)
	target_compile_options(dps_counter PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET dps_counter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

enable_testing()
add_executable(dps_logic_test tests/dps_logic_test.cpp)
target_compile_features(dps_logic_test PRIVATE cxx_std_20)
target_include_directories(dps_logic_test PRIVATE src)
add_test(NAME dps_logic_test COMMAND dps_logic_test)
