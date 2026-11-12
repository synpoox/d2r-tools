# Cartographer: opens a generated map from a map item in Harrogath (D2RLoader plugin).
#
# Build:  tools\build.ps1 -Plugin cartographer -Test (from the repo root)
# Output: build/cartographer/Release/cartographer.dll -> copy it to <game>/d2rloader/plugins/
cmake_minimum_required(VERSION 3.29)
project(cartographer LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CARTOGRAPHER_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK" CACHE PATH "PluginSDK checkout")
option(CARTOGRAPHER_TESTS "Build the host unit tests" OFF)

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${CARTOGRAPHER_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

# The table loader: pure C++, no engine headers. The plugin and tests/tables_tests.cpp both use it.
add_library(cartographer_core STATIC src/core/tables.cpp)
target_include_directories(cartographer_core PUBLIC src)
if(MSVC)
	target_compile_options(cartographer_core PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET cartographer_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
	target_compile_options(cartographer_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# The layout generator (phase 2): pure C++, no engine headers.
add_library(cartographer_layout SHARED src/layout/layout.cpp src/layout/layout.h src/layout/rng.h)
target_include_directories(cartographer_layout PUBLIC src/layout)
set_target_properties(cartographer_layout PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(MSVC)
	target_compile_options(cartographer_layout PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(cartographer_layout PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(cartographer SHARED
	src/plugin.cpp
	src/engine/portal.cpp
	src/log.cpp
	src/cartographer.rc
)
target_compile_features(cartographer PRIVATE cxx_std_20)
target_link_libraries(cartographer PRIVATE cartographer_core D2RLPlugin::D2RLPlugin user32)
set_target_properties(cartographer PROPERTIES OUTPUT_NAME "cartographer")
if(MSVC)
	target_compile_options(cartographer PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET cartographer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

if(CARTOGRAPHER_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
