# Quick NPC

A D2RLoader plugin for Diablo II: Resurrected. **Hold the Quick NPC key and left-click a vendor** to go straight to
its trade screen, skipping the Talk / Trade / Cancel menu.

The game still does all the real work. It walks you to the NPC, checks range, and opens the dialog. The plugin then
presses the menu's own "Trade" option for you, which is the same function a mouse click on "Trade" runs. Without the
key, or on an NPC that doesn't trade, nothing changes.

- Built from [d2rAI-toolkit](https://github.com/Nugwa628/d2rAI-toolkit)'s `plugin-template` (build guard, off-switch
  file, environment guard, log, status command).
- Plugin only: no data tables, strings or other mod files change.
- **Targets one game image: D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (PE stamp `0x6ab3782c`).** The Steam 3.3
  build 94995 runs the same image. On any other image the plugin loads, logs why, and changes nothing.

## Status

| What | State |
|---|---|
| Reverse engineering on the 93847 dump | done: [docs/reverse-engineering.md](docs/reverse-engineering.md); `tools/verify_dump.py` passes |
| Decision logic | 212 host checks pass (`tests/`) |
| Plugin build | MinGW-w64 cross compile and link pass. The MSVC build (`plugin/build.ps1`) runs in CI on Windows |
| **In-game tests** | **not run yet**: see [TESTING.md](TESTING.md) |

## Use

1. Install (below), start the game through D2RLoader with your mod, and load a character.
2. Options > Controls > **Quick NPC**: the action "Quick NPC: hold, then left-click a vendor to trade". The default
   key is **Y**. If Y is already taken, the action has no key and you pick one. Pick a key that is not used for
   something else (the Controls menu clears conflicts).
3. In town, hold the key and left-click a vendor. You walk there and the trade screen opens.

Console command (type it in the D2R console):

| Command | Does |
|---|---|
| `quick-npc` | Version, image check, key, live counters, other plugins |
| `quick-npc off` / `quick-npc on` | Switch the feature off or on until the game restarts |

To leave the feature out at start-up, put the word `quicknpc` in `quick-npc-off.txt` next to the DLL. That's the
toolkit's diagnosis switch; delete the file for normal play.

## What counts as a vendor

An NPC whose dialog offers **Trade** in D2R's own NPC menu table, read live from the game. That covers every
Trade and Trade/Repair NPC. Quest and story NPCs (Talk only), mercenary, travel and gamble-only menus, monsters,
objects and players are left alone: the click works as it always does.

## Build

The toolkit's process, on Windows with Visual Studio (C++ workload) and CMake 3.29 or newer:

```powershell
git clone --recursive <this repo>
powershell -ExecutionPolicy Bypass -File plugin\build.ps1
# -> plugin\build\Release\quick-npc.dll
```

Options: `-Mod <ModFolder>` turns on the environment guard's mod check. `-Name` renames the DLL, plugin id and console
command.

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

Close the game, then copy `quick-npc.dll` to:

```text
<game>\mods\<YourMod>\d2rloader\plugins\
```

The plugin is mod-scoped (`ModScopedOnly`), as the template is: it loads only from that mod's folder while that mod
runs. It is a `Client` plugin, so it isn't recorded in the character's plugin history and isn't part of the
multiplayer version match. Restart the game after replacing the DLL.

Logs: `<game>\mods\<YourMod>\d2rloader\logs\quick-npc.log`, with a load line (version, D2R version, image stamp) and one
line per part (`envguard ...`, `quicknpc on: hooks 0x101910 + 0x1141d0, 48 NPC menu rows, key vk 0x59`).

## Limitations

- Build 93847 / image `0x6ab3782c` only. Another game or D2RLoader version needs the addresses re-found
  (`docs/reverse-engineering.md`, then `tools/verify_dump.py`).
- Not yet tested in game (see TESTING.md).
- Gamble-only NPCs aren't treated as vendors, and Gamble isn't pressed for NPCs that have both Trade and Gamble.
  customnpcs.txt NPCs get only Talk in D2R's menu table, so they are not covered.
- If an NPC first plays a greeting or quest speech, the trade screen opens after the speech, when the menu appears.
  A click that doesn't reach its NPC within 60 seconds expires.
- Controller "interact" input is not covered (it doesn't use the left-click path).
- The default key Y is a guess at a free key. D2R's default bindings weren't checked in game.
