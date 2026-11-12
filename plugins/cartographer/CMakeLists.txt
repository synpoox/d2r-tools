# Cartographer: opens a generated map from a map item in Harrogath (D2RLoader plugin).
#
# Build:  tools\build.ps1 -Plugin cartographer -Test (from the repo root)
# Output: build/cartographer/Release/cartographer.dll -> copy it to <game>/d2rloader/plugins/
#         with the companion folder d2rl-cartographer/ (the plugin's own strings) beside it.
cmake_minimum_required(VERSION 3.29)
project(cartographer LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CARTOGRAPHER_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK" CACHE PATH "PluginSDK checkout")
option(CARTOGRAPHER_TESTS "Build the host unit tests" OFF)

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${CARTOGRAPHER_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

# One build rule for every Cartographer library (the sibling PRs for stats, events, outdoor and maze rebase onto this):
#  - Every library is STATIC and linked into cartographer.dll, so dist\cartographer\ ships only the plugin and its
#    companion folder. Never add a SHARED library: build.ps1 and release.ps1 collect every Release DLL as a plugin.
#  - Every library gets the plugin's MSVC runtime (static, /MT) and /EHsc; a mix fails to link (LNK2038). Create a
#    library with cartographer_library(<name> <sources...>), not add_library, and link it into the plugin target.
function(cartographer_library name)
	add_library(${name} STATIC ${ARGN})
	if(MSVC)
		target_compile_options(${name} PRIVATE /W4 /permissive- /EHsc /utf-8)
		set_property(TARGET ${name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
	else()
		target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
	endif()
endfunction()

# The table loader: pure C++, no engine headers. The plugin and tests/tables_tests.cpp both use it.
cartographer_library(cartographer_core src/core/tables.cpp)
target_include_directories(cartographer_core PUBLIC src)

# The layout generator (phase 2): pure C++, no engine headers.
cartographer_library(cartographer_layout src/layout/layout.cpp src/layout/layout.h src/layout/rng.h)
target_include_directories(cartographer_layout PUBLIC src/layout)

# The maze steering core (phase 3a): pure C++, no engine headers. Built on the layout generator.
cartographer_library(cartographer_maze src/maze/maze.cpp src/maze/maze.h)
target_include_directories(cartographer_maze PUBLIC src/maze)
target_link_libraries(cartographer_maze PUBLIC cartographer_layout)

add_library(cartographer SHARED
	src/plugin.cpp
	src/engine/portal.cpp
	src/log.cpp
	src/cartographer.rc
)
target_compile_features(cartographer PRIVATE cxx_std_20)
target_link_libraries(cartographer PRIVATE cartographer_core cartographer_layout cartographer_maze D2RLPlugin::D2RLPlugin user32)
set_target_properties(cartographer PROPERTIES OUTPUT_NAME "cartographer")
if(MSVC)
	target_compile_options(cartographer PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET cartographer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

if(CARTOGRAPHER_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
