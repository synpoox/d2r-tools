# Better XP Bar

A D2RLoader plugin for **Diablo II: Resurrected 3.3** that shows an XP panel in the HUD, in the spirit of Project
Diablo II's XP bar:

```
Level 84
XP 2,061,214,567  41.27%
Session +38,250,112  +27.05%
XP/h 21.4M  15.12%  (avg 18.9M)
Time 01:12:31
Next level 23m 40s
```

1. Character level
2. Total XP, and % of the current level (the same formula as the stock XP bar)
3. Session XP, flat and in levels (+27.05 % = about a quarter level)
4. XP per hour over the last 10 minutes (and the session average), flat and in % of a level per hour
5. Session time (in-game time only)
6. Time to the next level at the current rate

Definitions: [docs/metrics.md](docs/metrics.md). Tested in game on D2R 3.3 build 93847 with D2RLoader 1.3.1; see [TESTING.md](TESTING.md).

## Supported game

| | |
|---|---|
| Game | D2R **3.3.0**, Battle.net build 93847 / Steam 94995 (same program) |
| Loader | D2RLoader **1.3.1** (plugin ABI 4, PluginSDK 0.3.0) |
| Image check | PE timestamp `0x6ab3782c` plus 9 byte checks of the code it uses |

On any other game or loader image the plugin loads, logs `not the expected game image`, and does nothing else. It
installs no hooks and writes no game memory: it reads what the stock XP bar reads and draws with D2RLoader's overlay.
Client-side only: it is a `Client` plugin (not recorded in character plugin history, no multiplayer match needed).

## Install

1. Get `better-xp-bar.dll`: GitHub Actions > the latest green `build` run > Artifacts > `better-xp-bar` (a zip).
2. Copy it to `<game>\d2rloader\plugins\` (all mods) or `<game>\mods\<Mod>\d2rloader\plugins\` (one mod).
3. Start the game through D2RLoader. The display shows when you enter a game.

Uninstall: delete the DLL (and `better-xp-bar.toml` from the config folder if you like).

## Hotkeys and commands

| Action | Default | Rebind |
|---|---|---|
| Toggle the display | Ctrl+X | Options > Controls > "Better XP Bar" |
| Reset the session | Ctrl+R | same |

D2RLoader stores a key the first time an action is registered (`<game>\d2rloader\config\input-bindings.toml`);
rebind in the Controls menu.

Console (D2RLoader's console): `better-xp-bar` (status), `better-xp-bar show`, `better-xp-bar reset`,
`better-xp-bar toggle`, `better-xp-bar reload` (re-read the config).

## Session rules

- A session is **one game**: it starts when your character is ready in a game and ends when you leave to the menu
  or lobby. Set `scope = "character"` to keep one session per character across games.
- The reset hotkey starts a new session at once.
- XP/hour is net: an XP loss from death lowers it. "Next level" shows `--` while the rate is zero or negative, and at
  the maximum level. Rates show `--` for the first 60 seconds.

## Configuration

D2RLoader creates `better-xp-bar.toml` in the `d2rloader\config\` folder of the scope the DLL is in (the game's or the
mod's) from the defaults built into the DLL ([plugin/better-xp-bar.toml](plugin/better-xp-bar.toml)):

| Key | Default | Meaning |
|---|---|---|
| `[display] visible` | `true` | shown when a game starts |
| `anchor` | `"bottom-center"` | `top-left`, `top-center`, `top-right`, `bottom-left`, `bottom-center`, `bottom-right` |
| `offset_x`, `offset_y` | `0`, `-160` | pixels from the anchor |
| `text_size` | `0` | 0 = D2RLoader's default |
| `background` | `0.55` | background opacity 0..1 |
| `text_color` | `[1.0, 0.86, 0.55]` | r, g, b 0..1 |
| `lines` | all six | any of `"level"`, `"xp"`, `"session"`, `"rate"`, `"time"`, `"eta"`, in order |
| `[session] scope` | `"game"` | or `"character"` |
| `window_seconds` | `600` | rolling window for the live XP/hour (60..86400) |
| `eta_from` | `"window"` | or `"session"` (time to level from the session average) |
| `[log] xp_changes` | `false` | log XP changes (sampled) |

## Logs and troubleshooting

- Plugin log: the `logs` folder next to D2RLoader's (`better-xp-bar:` lines): version, image check, one line per
  part, config, every game join/leave with a session summary, level-ups, XP losses.
- Off-switch: `better-xp-bar-off.txt` next to the DLL with any of `xp overlay hotkeys envguard` leaves those parts
  out.

## Build

CI (the monorepo's `.github/workflows/ci.yml`) builds the DLL on `windows-latest`. By hand on Windows (CMake 3.29+, Visual
Studio with C++):

```bash
git clone --recursive https://github.com/synpoox/d2r-tools
cd d2r-tools
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin better-xp-bar   # or by hand from plugins/better-xp-bar:
```

```bash
cmake -S plugins/better-xp-bar -B build -A x64 -DBXB_SDK_DIR=third_party/PluginSDK
```

```bash
cmake --build build --config Release
```

Output: `build/plugin/Release/better-xp-bar.dll`. The metrics core and its tests build on any OS:

```bash
cmake -S . -B build-tests -DBXB_BUILD_PLUGIN=OFF && cmake --build build-tests && ctest --test-dir build-tests
```

## Layout

| Path | What |
|---|---|
| `core/` | metrics, formatting, config parsing: pure C++20, no game or Windows code |
| `tests/` | unit tests for `core/` |
| `plugin/` | the D2RLoader DLL; `src/d2r.h` holds every game address with its evidence |
| [`docs/reverse-engineering/better-xp-bar*.md`](../../docs/reverse-engineering/better-xp-bar.md) (repo root) | dump inventory and reverse-engineering findings (addresses, evidence, confidence) |
| `tools/re/` | the small static-analysis scripts used for the findings |

This repository contains no game files. Diablo II: Resurrected is (c) Blizzard Entertainment. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
