# Quick NPC

A D2RLoader plugin for Diablo II: Resurrected. **Hold Left Ctrl and left-click a vendor** to go straight to its trade
screen, skipping the Talk / Trade / Cancel menu. The key is configurable in a TOML file.

The game still does all the real work. It walks (or, with Ctrl, runs) you to the NPC, checks range, and opens the
dialog. The plugin then presses the menu's own "Trade" option for you, which is the same function a mouse click on
"Trade" runs. Without the key, or on an NPC that doesn't trade, nothing changes.

- Built from [d2rAI-toolkit](https://github.com/Nugwa628/d2rAI-toolkit)'s `plugin-template` (build guard, off-switch
  file, environment guard, log, status command).
- A **global** plugin: install it once and it works with every mod and with the unmodded game.
- Plugin only: no data tables, strings or other mod files change.
- **Targets one game image: D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (PE stamp `0x6ab3782c`).** The Steam 3.3
  build 94995 runs the same image. On any other image the plugin loads, logs why, and changes nothing.

## Status

| What | State |
|---|---|
| Reverse engineering on the 93847 dump | done: [docs/reverse-engineering.md](docs/reverse-engineering.md); `tools/verify_dump.py` passes |
| Decision logic and settings parser | 272 host checks pass (`tests/`) |
| Plugin build | MinGW-w64 cross build and the MSVC build (`plugin/build.ps1`) pass in CI |
| In-game tests | step 0 (loads, hooks on, image match) confirmed in game on 0.1.0; the rest not run yet: see [TESTING.md](TESTING.md) |

## Use

1. Install (below), start the game through D2RLoader, and load a character.
2. In town, hold **Left Ctrl** and left-click a vendor. You run there and the trade screen opens.

Ctrl is also D2R's default **Run** key, so the game makes you run to the vendor instead of walking. That's the only
difference from a normal click.

Console command (type it in the D2R console):

| Command | Does |
|---|---|
| `quick-npc` | Version, image check, hotkey, config and log paths, live counters, other plugins |
| `quick-npc off` / `quick-npc on` | Switch the feature off or on (until the game restarts or `reload`) |
| `quick-npc reload` | Re-read `quick-npc.toml` |

To leave the feature out at start-up, put the word `quicknpc` in `quick-npc-off.txt` next to the DLL. That's the
toolkit's diagnosis switch; delete the file for normal play.

## Settings: `quick-npc.toml`

D2RLoader writes the default file to `<game>\d2rloader\config\quick-npc.toml` the first time the plugin loads. It
never overwrites your copy after that. Edit it, then restart the game or type `quick-npc reload`.

```toml
[quick-npc]
enabled = true            # false: start switched off
hotkey = "LeftCtrl"       # hold this, then left-click a vendor
timeout_seconds = 60      # how long a click waits for the vendor's dialog (5 to 600)
```

`hotkey` names: `LeftCtrl`, `RightCtrl`, `Ctrl` (either), `LeftAlt`, `RightAlt`, `Alt`, `A`-`Z`, `0`-`9`, `F1`-`F24`,
`Numpad0`-`Numpad9`, `Space`, `Tab`, `CapsLock`, `Backquote`, `Mouse3` (middle button), `Mouse4`, `Mouse5` (side
buttons), or a Windows virtual-key code such as `"0xA2"`. Case, spaces, `-` and `_` don't matter.

- **Shift doesn't work** with D2R's default controls. Shift is Stand Still, and Stand Still stops NPC interaction.
  The plugin logs a warning if you choose it.
- **Alt** is D2R's Show Items key, so a click on an item label picks the item up instead of reaching the NPC.
- A letter or digit still does whatever D2R binds to it. Pick one that is unbound in Options > Controls.

A bad value is reported in the log with its line number, and the default is kept.

## What counts as a vendor

An NPC whose dialog offers **Trade** in D2R's own NPC menu table, read live from the game. That covers every
Trade and Trade/Repair NPC. Quest and story NPCs (Talk only), mercenary, travel and gamble-only menus, monsters,
objects and players are left alone: the click works as it always does.

## Build

The toolkit's process, on Windows with Visual Studio (C++ workload) and CMake 3.29 or newer:

```powershell
git clone --recursive <this repo>
powershell -ExecutionPolicy Bypass -File plugin\build.ps1
# -> plugin\build\Release\quick-npc.dll  (the default quick-npc.toml is embedded in it)
```

`-Name` renames the DLL, plugin id, console command and config file.

Host tests (any OS, any C++20 compiler):

```bash
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
```

Check a dump of a game image (after a game or loader update):

```bash
python tools/verify_dump.py <dump.exe> --table
```

`--table` needs `pip install unicorn`.

## Install

Close the game, then copy `quick-npc.dll` to D2RLoader's game-wide plugin folder:

```text
<game>\d2rloader\plugins\
```

It loads with every mod and with the unmodded game. If you installed 0.1.0 into a mod's
`mods\<Mod>\d2rloader\plugins\` folder, delete that copy: two copies would both hook the game, and the second one's
hooks are refused (its status says why). It is a `Client` plugin, so it isn't recorded in the character's plugin
history and isn't part of the multiplayer version match. Restart the game after replacing the DLL.

The log has a load line (version, D2R version, image stamp), the settings in use, and one line per part, for example
`quicknpc on: hooks 0x101910 + 0x1141d0, 48 NPC menu rows, hotkey LeftCtrl`. The `quick-npc` console command prints
where D2RLoader keeps the log and the config.

## Limitations

- Build 93847 / image `0x6ab3782c` only. Another game or D2RLoader version needs the addresses re-found
  (`docs/reverse-engineering.md`, then `tools/verify_dump.py`).
- Only step 0 of TESTING.md has been confirmed in game.
- Gamble-only NPCs aren't treated as vendors, and Gamble isn't pressed for NPCs that have both Trade and Gamble.
  customnpcs.txt NPCs get only Talk in D2R's menu table, so they are not covered.
- If an NPC first plays a greeting or quest speech, the trade screen opens after the speech, when the menu appears.
  A click that doesn't reach its NPC within `timeout_seconds` expires.
- Controller "interact" input is not covered (it doesn't use the left-click path).
- The hotkey is read from the keyboard state when you click (`GetAsyncKeyState`), not through D2R's Controls menu, so
  it doesn't appear there and D2R doesn't warn about clashes.
