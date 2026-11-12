# Cartographer: opens a generated map from the map's own item in Harrogath (D2RLoader plugin).
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

# One build rule for every Cartographer library (the sibling PRs for stats, events, outdoor and maze rebase onto this):
#  - Every library is STATIC and linked into cartographer.dll, so dist\cartographer\ ships only the plugin. Never add a SHARED library: build.ps1 and release.ps1 collect every Release DLL as a plugin.
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

# Cores: every directory src/<core>/ except core/ (the table loader above) and engine/ (plugin-only code) is a pure
# C++ library cartographer_<core> built from its *.cpp, with src/<core> as its public include path. A new core needs no
# edit here: add the directory. Dependencies on other cores go in src/<core>/deps.cmake as
# `set(CARTOGRAPHER_CORE_DEPS layout)` (names without the cartographer_ prefix). Tests: tests/<core>_tests.cpp.
file(GLOB cartographer_core_dirs LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
list(SORT cartographer_core_dirs)
set(CARTOGRAPHER_CORES "")
foreach(core IN LISTS cartographer_core_dirs)
	if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/${core} OR core STREQUAL "core" OR core STREQUAL "engine")
		continue()
	endif()
	file(GLOB core_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${core}/*.cpp)
	cartographer_library(cartographer_${core} ${core_sources})
	target_include_directories(cartographer_${core} PUBLIC src/${core})
	set(CARTOGRAPHER_CORE_DEPS "")
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${core}/deps.cmake)
		include(${CMAKE_CURRENT_SOURCE_DIR}/src/${core}/deps.cmake)
	endif()
	foreach(dep IN LISTS CARTOGRAPHER_CORE_DEPS)
		target_link_libraries(cartographer_${core} PUBLIC cartographer_${dep})
	endforeach()
	list(APPEND CARTOGRAPHER_CORES cartographer_${core})
endforeach()

# core is the one place that names cores by hand (see #36 for the discovery above); a new core it reads needs a line here.
# The table loader also reads shapes.txt, mapstats.txt and mapevents.txt through these cores.
target_link_libraries(cartographer_core PUBLIC cartographer_layout cartographer_stats cartographer_events)

add_library(cartographer SHARED
	src/plugin.cpp
	src/engine/portal.cpp
	src/engine/stat_apply.cpp
	src/engine/item_stats.cpp
	src/engine/event_apply.cpp
	src/engine/event_6b.cpp
	src/engine/map_sync.cpp
	src/engine/scan.cpp
	src/engine/maze_hooks.cpp
	src/engine/outdoor_hooks.cpp
	src/engine/map_names.cpp
	src/engine/level_row.cpp
	src/engine/game_memory.cpp
	src/log.cpp
	src/cartographer.rc
)
target_compile_features(cartographer PRIVATE cxx_std_20)
target_link_libraries(cartographer PRIVATE cartographer_core ${CARTOGRAPHER_CORES} D2RLPlugin::D2RLPlugin user32)
set_target_properties(cartographer PROPERTIES OUTPUT_NAME "cartographer")
if(MSVC)
	target_compile_options(cartographer PRIVATE /W4 /permissive- /EHsc /utf-8)
	set_property(TARGET cartographer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()

if(CARTOGRAPHER_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
