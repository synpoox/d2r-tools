# Pure C++ metrics: no game, no Windows. Used by the plugin and by the unit tests.
add_library(bxb_core STATIC
	src/exp_table.cpp
	src/xp_session.cpp
	src/format.cpp
)
target_include_directories(bxb_core PUBLIC include)
target_compile_features(bxb_core PUBLIC cxx_std_20)
if(MSVC)
	target_compile_options(bxb_core PRIVATE /W4 /permissive- /utf-8)
	set_property(TARGET bxb_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
	target_compile_options(bxb_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
