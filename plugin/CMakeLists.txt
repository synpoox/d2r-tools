# The D2RLoader plugin DLL (Windows x64). Based on d2rAI-toolkit's plugin-template/CMakeLists.txt.
enable_language(RC)

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${BXB_SDK_DIR} PluginSDK EXCLUDE_FROM_ALL)

add_library(bxb_plugin SHARED
	src/plugin.cpp
	src/log.cpp
	src/off_switch.cpp
	src/env_guard.cpp
	src/xp_reader.cpp
	src/display.cpp
	better-xp-bar.rc
)
target_link_libraries(bxb_plugin PRIVATE D2RLPlugin::D2RLPlugin bxb_core)
# D2RLoader writes <scope>/d2rloader/config/better-xp-bar.toml from this when the file is missing.
d2rlplugin_embed_config(TARGET bxb_plugin FILE "${CMAKE_CURRENT_SOURCE_DIR}/better-xp-bar.toml")
target_compile_definitions(bxb_plugin PRIVATE
	BXB_PLUGIN_NAME="${BXB_PLUGIN_NAME}"
	BXB_PLUGIN_VERSION="${BXB_PLUGIN_VERSION}"
)
set_target_properties(bxb_plugin PROPERTIES OUTPUT_NAME "${BXB_PLUGIN_NAME}" PREFIX "")
if(MSVC)
	target_compile_options(bxb_plugin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc /utf-8>)
	set_property(TARGET bxb_plugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ runtime dependency
endif()
