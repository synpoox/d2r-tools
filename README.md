# d2r-tools

Monorepo for my D2RLoader plugins for Diablo II: Resurrected.

## Plugins

<!--PLUGINS-->

## Layout

```
plugins/<name>/     one plugin each: CMakeLists.txt, src/, tests/, README.md, CHANGELOG.md
third_party/PluginSDK/   D2RLoader PluginSDK submodule, pinned once for every plugin
tools/build.ps1     build one or all plugins
docs/               RELEASING.md
.github/workflows/  ci (build matrix), release (tag-driven)
```

## Build

Windows x64, Visual Studio 2022 (C++ workload), CMake 3.29+.

```powershell
git clone --recurse-submodules https://github.com/synpoox/d2r-tools
cd d2r-tools
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin better-auras   # or omit -Plugin for all
```

DLLs land in `dist\<plugin>\`. Each plugin also builds standalone from its own directory (`plugins\<name>\build.ps1`).

## Releases

Tags are plugin-scoped (`better-auras/v0.2.0`); see [docs/RELEASING.md](docs/RELEASING.md).

Native offsets and signatures are specific to a D2R build; check before reuse.
