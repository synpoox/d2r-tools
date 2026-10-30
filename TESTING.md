# Testing

## Automated (no game)

| Check | Command | Result (this commit) |
|---|---|---|
| Decision logic and settings: vendor vs non-vendor classification, click edge, repeated-input suppression, timeouts (default and configured), feature off, unsupported build, refused or already-taken hooks, missing services, `quick-npc.toml` parsing (the real default file, key names, bad values, BOM, CRLF, other tables) | `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests` | 272 checks, 0 failed (GCC 13, `-Wall -Wextra -Wpedantic -Werror`, ASan + UBSan) |
| Every address, entry byte, code pattern and table layout against the 93847 dump | `python tools/verify_dump.py <dump> --table` | all checks passed, including D2R's default Run = Ctrl and Stand Still = Shift bindings; 48 menu rows, 16 vendor classes |
| Plugin compiles and links against PluginSDK 0.3.0 and the Windows headers | MinGW-w64 GCC 13 cross build (see CI) | links; exports `D2RLoaderGetPluginInfo`, `D2RLoaderLoadPlugin`, `D2RLoaderUnloadPlugin`; manifest and default `quick-npc.toml` resources embedded |
| MSVC build with the toolkit's `build.ps1` | CI job `plugin-msvc` (Windows runner, MSVC 19.51, `/W4 /permissive-`) | 0.1.0: no warnings (CI run 2). 0.2.0: see the CI run for this commit; the DLL is the `quick-npc-dll` artifact |

## In game (game-copy slot, toolkit `testing/`)

They need Windows, the game and D2RLoader 1.3.1 (a game-copy slot from the toolkit's `testing/game_slots.py`). Each
row is what to do and what should happen. Record the result and the `quick-npc` console output after each one.
"The hotkey" is Left Ctrl unless `quick-npc.toml` says otherwise.

| # | Do | Expect | Result |
|---|---|---|---|
| 0 | Install, start the game, type `quick-npc` | `quicknpc on: hooks 0x101910 + 0x1141d0, 48 NPC menu rows, ...`; image `match`; no new crash files | **passed** on 0.1.0 (mod-scoped build, commit 2b1d69a), confirmed by the user in game. Repeat on 0.2.0 |
| 0b | 0.2.0: install in `<game>\d2rloader\plugins\` (delete any copy in a mod's plugins folder), start the game with a mod and once without one, type `quick-npc` | It loads both times. `hotkey LeftCtrl (vk 0xa2)`; `config ...\d2rloader\config\quick-npc.toml` exists with the default text; no "second copy" warning | not run |
| 1 | Hold the hotkey, left-click a vendor (Akara, Charsi, Gheed) in range | The trade screen opens once. Counters: vendors 1, queued 1, opened 1 | not run |
| 2 | Left-click the same vendor without the hotkey | The normal Talk / Trade / Cancel menu. Counters unchanged | not run |
| 3 | Hold the hotkey, left-click Cain, Kashya or Warriv | The normal menu. Counter "not vendors" +1, no trade | not run |
| 4 | Hold the hotkey, left-click a monster outside town, a chest or waypoint, a player, and click inventory or chat | Vanilla behavior (Ctrl also makes you run). Counters unchanged (the monster may count as "not vendors") | not run |
| 5 | Hold the hotkey, left-click a vendor across town | You run there (Ctrl is D2R's Run key), then the trade screen opens. Release the key while running: it still opens | not run |
| 6 | As 5, but click the ground while running | You go there instead. No trade screen later when you talk to the vendor | not run |
| 7 | Hold the hotkey and keep the left button held on a vendor; also press twice quickly | One trade screen. Opened +1 per interaction | not run |
| 8 | Close the trade screen, then click the vendor again without the hotkey | The normal menu | not run |
| 9 | `quick-npc off`, then repeat 1 | The normal menu (feature off). `quick-npc on` restores it | not run |
| 10 | Put `quicknpc` in `quick-npc-off.txt` next to the DLL, restart | Log `quicknpc off: left out by quick-npc-off.txt`. Vanilla behavior | not run |
| 11 | A vendor with a pending quest speech (e.g. first visit in an act) | The speech plays, then the trade screen opens | not run |
| 12 | Quit to menu, rejoin, repeat 1. Exit the game | Works after rejoin. Clean exit, no crash report in `d2rloader\crashes` | not run |
| 13 | Right Ctrl + left-click a vendor (default `LeftCtrl`) | The normal menu: only Left Ctrl is the hotkey | not run |
| 14 | Set `hotkey = "RightCtrl"` in `quick-npc.toml`, `quick-npc reload`, repeat 1 with Right Ctrl, then with Left Ctrl | Right Ctrl trades; Left Ctrl gives the normal menu. `quick-npc` shows `hotkey RightCtrl (vk 0xa3)` | not run |
| 15 | Set `hotkey = "Hyper"`, `quick-npc reload` | Log `config line N: hotkey "Hyper" is not a key ...`; hotkey stays LeftCtrl | not run |
| 16 | Set `enabled = false`, restart | `quick-npc` says disabled; vendor clicks are vanilla; `quick-npc on` enables it | not run |
| 17 | Set `hotkey = "LeftShift"`, reload, try 1 | Log warning about Stand Still. With D2R's default controls you stand still and no menu opens (expected) | not run |
