# Testing

## Automated (no game)

| Check | Command | Result (this commit) |
|---|---|---|
| Decision logic: vendor vs non-vendor classification, click and hotkey edge, repeated-input suppression, timeouts, feature off, unsupported build, refused or already-taken hooks, missing services | `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests` | 212 checks, 0 failed (GCC 13, `-Wall -Wextra -Wpedantic -Werror`, ASan + UBSan) |
| Every address, entry byte, code pattern and table layout against the 93847 dump | `python tools/verify_dump.py <dump> --table` | all checks passed; 48 menu rows, 16 vendor classes |
| Plugin compiles and links against PluginSDK 0.3.0 and the Windows headers | MinGW-w64 GCC 13 cross build (see CI) | links; exports `D2RLoaderGetPluginInfo`, `D2RLoaderLoadPlugin`, `D2RLoaderUnloadPlugin`; manifest resource embedded |
| MSVC build with the toolkit's `build.ps1` | CI job `plugin-msvc` (Windows runner) | see the CI run for the pushed commit |

## In game (game-copy slot, toolkit `testing/`)

**None of these have been run.** They need Windows, the game and D2RLoader 1.3.1 (a game-copy slot from the toolkit's
`testing/game_slots.py`). Each row is what to do and what should happen. Record the result and the `quick-npc` console
output after each one.

| # | Do | Expect | Result |
|---|---|---|---|
| 0 | Install, start the game, type `quick-npc` | `quicknpc on: hooks 0x101910 + 0x1141d0, 48 NPC menu rows, key vk 0x..`; image `match`; no new crash files | not run |
| 1 | Hold the key, left-click a vendor (Akara, Charsi, Gheed) in range | The trade screen opens once. Counters: vendors 1, queued 1, opened 1 | not run |
| 2 | Left-click the same vendor without the key | The normal Talk / Trade / Cancel menu. Counters unchanged | not run |
| 3 | Hold the key, left-click Cain, Kashya or Warriv | The normal menu. Counter "not vendors" +1, no trade | not run |
| 4 | Hold the key, left-click a monster outside town, a chest or waypoint, a player, and click inventory or chat | Vanilla behavior. Counters unchanged (the monster may count as "not vendors") | not run |
| 5 | Hold the key, left-click a vendor across town | You walk there (vanilla path), then the trade screen opens. Release the key while walking: it still opens | not run |
| 6 | As 5, but click the ground while walking | You go there instead. No trade screen later when you talk to the vendor | not run |
| 7 | Hold the key and keep the left button held on a vendor; also press twice quickly | One trade screen. Opened +1 per interaction | not run |
| 8 | Close the trade screen, then click the vendor again without the key | The normal menu | not run |
| 9 | `quick-npc off`, then repeat 1 | The normal menu (feature off). `quick-npc on` restores it | not run |
| 10 | Put `quicknpc` in `quick-npc-off.txt`, restart | Log `quicknpc off: left out by quick-npc-off.txt`. Vanilla behavior | not run |
| 11 | A vendor with a pending quest speech (e.g. first visit in an act) | The speech plays, then the trade screen opens | not run |
| 12 | Quit to menu, rejoin, repeat 1. Exit the game | Works after rejoin. Clean exit, no crash report in `d2rloader\crashes` | not run |
