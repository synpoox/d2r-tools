# player-bars: health and mana bars above your character (D2RLoader plugin).
#
# Build:  tools\build.ps1 -Plugin player-bars -Test (from the repo root)
# Output: build/player-bars/Release/player-bars.dll -> copy it to <game>/d2rloader/plugins/
cmake_minimum_required(VERSION 3.29)
project(player_bars LANGUAGES CXX RC)

set(D2RL_SDK_DIR "" CACHE PATH "D2RLoader PluginSDK directory")
if(NOT D2RL_SDK_DIR)
	message(FATAL_ERROR "Pass -DD2RL_SDK_DIR=<PluginSDK checkout>")
endif()

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${D2RL_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(player_bars SHARED
	src/plugin.cpp
	src/bars.cpp
	src/log.cpp
	src/player-bars.rc
)
target_compile_features(player_bars PRIVATE cxx_std_20)
target_link_libraries(player_bars PRIVATE D2RLPlugin::D2RLPlugin user32)
set_target_properties(player_bars PROPERTIES OUTPUT_NAME "player-bars")
d2rlplugin_embed_config(TARGET player_bars FILE player-bars.toml)
if(MSVC)
	target_compile_options(player_bars PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET player_bars PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

enable_testing()
add_executable(bars_logic_test tests/bars_logic_test.cpp)
target_compile_features(bars_logic_test PRIVATE cxx_std_20)
target_include_directories(bars_logic_test PRIVATE src)
add_test(NAME bars_logic_test COMMAND bars_logic_test)
