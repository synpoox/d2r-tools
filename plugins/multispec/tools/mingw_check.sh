#!/bin/sh
# Compiles and links the plugin with mingw-w64 on Linux: a check that the Windows code builds (types, headers, link),
# not the release build (that is build.ps1 with MSVC). Needs x86_64-w64-mingw32-g++ and the PluginSDK.
#   tools/mingw_check.sh [PluginSDK dir]
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
sdk="${1:-$root/../d2rAI-toolkit/third_party/PluginSDK}"
out="${TMPDIR:-/tmp}/multispec-mingw"
mkdir -p "$out"
cxx=x86_64-w64-mingw32-g++
x86_64-w64-mingw32-windres -I "$sdk/include" "$root/src/multispec.rc" -O coff -o "$out/multispec_rc.o"
$cxx -std=c++20 -O1 -Wall -Wextra -Werror -Wno-cast-function-type -isystem "$sdk/include" -shared -static \
	-DMULTISPEC_VERSION='"1.0.1"' -DMULTISPEC_MOD_NAME='""' \
	"$root"/src/plugin.cpp "$root"/src/d2r.cpp "$root"/src/spec_model.cpp "$root"/src/swap.cpp \
	"$root"/src/spec_engine.cpp "$root"/src/spec_store.cpp "$root"/src/log.cpp "$root"/src/off_switch.cpp \
	"$root"/src/env_guard.cpp "$out/multispec_rc.o" -lshell32 -lole32 -luuid -o "$out/multispec.dll"
x86_64-w64-mingw32-objdump -p "$out/multispec.dll" | grep -E "D2RLoader(GetPluginInfo|LoadPlugin|UnloadPlugin)"
echo "mingw check ok: $out/multispec.dll"
