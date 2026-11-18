# potion-counter: how many potions are in each belt column, shown on the belt (D2RLoader plugin).
#
# Build:  tools\build.ps1 -Plugin potion-counter -Test (from the repo root)
# Output: build/potion-counter/Release/potion-counter.dll -> copy it to <game>/d2rloader/plugins/
cmake_minimum_required(VERSION 3.29)
project(potion_counter LANGUAGES CXX RC)

set(D2RL_SDK_DIR "" CACHE PATH "D2RLoader PluginSDK directory")
if(NOT D2RL_SDK_DIR)
	message(FATAL_ERROR "Pass -DD2RL_SDK_DIR=<PluginSDK checkout>")
endif()

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${D2RL_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(potion_counter SHARED
	src/plugin.cpp
	src/counter.cpp
	src/log.cpp
	src/potion-counter.rc
)
target_compile_features(potion_counter PRIVATE cxx_std_20)
target_link_libraries(potion_counter PRIVATE D2RLPlugin::D2RLPlugin user32)
set_target_properties(potion_counter PROPERTIES OUTPUT_NAME "potion-counter")
if(MSVC)
	target_compile_options(potion_counter PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET potion_counter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

enable_testing()
add_executable(counter_logic_test tests/counter_logic_test.cpp)
target_compile_features(counter_logic_test PRIVATE cxx_std_20)
target_include_directories(counter_logic_test PRIVATE src)
add_test(NAME counter_logic_test COMMAND counter_logic_test)
