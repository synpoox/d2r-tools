# d2r-tools

Tools and plugins for Diablo II: Resurrected, built on [D2RLoader](https://github.com/D2RLoader) and its [PluginSDK](https://github.com/D2RLoader/PluginSDK); each lives in its own folder and builds on its own.

## Plugins

| Plugin | Version | Description |
|---|---|---|
| [better-auras](plugins/better-auras) | Unreleased | Toggled, Path of Exile-style auras with mana/life drain and reservation. |
| [better-automap](plugins/better-automap) | Unreleased | Pick the mini map's side, open the full map over it with one key, zoom it with the mouse wheel. |
| [better-xp-bar](plugins/better-xp-bar) | Unreleased | HUD panel with level, XP, session gain and XP per hour. |
| [monster-count](plugins/monster-count) | Unreleased | Monster counter; reverse-engineering spike only, nothing hooked in game yet. |
| [multispec](plugins/multispec) | Unreleased | Two (or more) saved skill allocations per character, swapped in town without a respec token. |
| [quick-npc](plugins/quick-npc) | Unreleased | Ctrl+click an NPC to skip the dialog: trade, identify, resurrect your mercenary or travel. |
| [scrollable-waypoints](plugins/scrollable-waypoints) | 1.0.0 | Mouse wheel switches the waypoint panel's Act tabs. |

Each plugin's README has its features, config and supported game build; its `CHANGELOG.md` lists releases.

## Installation

Requires Diablo II: Resurrected with D2RLoader 1.3.1 (offline or TCP/IP; never Battle.net online play).

1. Download a plugin's `<plugin>-X.Y.Z.zip` (or the bundle with every plugin) from the latest
   [release](../../releases/latest), and extract the DLL. Unreleased plugins are only available as a `ci` run's artifact.
2. Close the game. Copy the DLL to `<game>\d2rloader\plugins\` (works with every mod), or to
   `<game>\mods\<Mod>\d2rloader\plugins\` for one mod only. Use one location per plugin, never both.
3. Start the game through D2RLoader. On first start the plugin writes its default config to
   `<game>\d2rloader\config\<plugin>.toml`; edit it and restart to change settings.

To update, replace the DLL. Plugins that change shared game state must be the same version on every player in a
multiplayer game; the plugin README says which. Native addresses belong to one D2R build; on any other build a
plugin should log that and stay off, but check its README before updating the game.

## Bug Reports

Open an issue on [GitHub Issues](../../issues). Please include:

- the plugin and its version (see the log's `<plugin>: x.y.z loaded` line),
- the game version and build, D2RLoader version, and the mod and mod version, if any,
- offline, TCP/IP host or TCP/IP client,
- steps to reproduce, what you expected and what happened,
- the plugin log from `<game>\d2rloader\logs\` (or `mods\<Mod>\d2rloader\logs\`), with personal paths and account
  names removed. Do not attach saves, game files or credentials.

## Building

Windows x64, Visual Studio 2022 (C++ workload), CMake 3.29+.

```powershell
git clone --recurse-submodules https://github.com/synpoox/d2r-tools
cd d2r-tools
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin better-auras   # omit -Plugin for all; add -Test for tests
```

DLLs land in `dist\<plugin>\`. Layout, tests and release steps: [CONTRIBUTING.md](CONTRIBUTING.md),
[docs/RELEASING.md](docs/RELEASING.md). Notes for coding agents: [AGENTS.md](AGENTS.md).

```
plugins/<name>/          one plugin each: sources, tests, README.md, CHANGELOG.md
third_party/PluginSDK/   D2RLoader PluginSDK submodule, pinned once for every plugin
tools/build.ps1          build (and test) one or all plugins
docs/                    RELEASING.md
.github/workflows/       ci (build matrix), release (suite release, run by hand)
```
