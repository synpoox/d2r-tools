# Cartographer: Open check (2026-11-12)

Live in game: the new `Shape` `Open` (PR #325, review item: the many-link maze build at Large), slot 11, Hell, single
player, test save `cartos` (Hell unlocked, no sidecar). The plugin was built from the tree of `89172bf` (`tools\build.ps1
-Plugin cartographer -Test`, SHA-256 `19CE6EDD...B48BF`); the log line `cartographer: 0.1.0 loaded (mod D2RToolsTest,
D2R 3.3.0, image 0x6ab3782c)` was seen in each game. The slot had no standing Cartographer tables; `maps.txt` and
`cartographer.txt` were written for this check (rows `OpenCryptLarge`, `OpenCryptSmall`, `OpenJungleLarge`, items
t20 to t22, Level 90, `Properties` 0, `EventChance` 0), the Maze level's `LevelType` was set to 4 for Crypt. The
character used `debug_mode` and `cheat god 1` first. Maps were opened by `AutoCheck`; the portal was entered by hand.
Logs and screenshots are kept outside the repo.

| Check | Result |
|---|---|
| 1. Crypt Large Open (12 x 12 rooms, 144 rooms) | PASS in 2 games (two seeds): the maze built with no crash, arrival in the planned entrance cell, the boss spawned in the boss room (superunique), Save and Exit worked. `AutoCheck` pass=5 fail=0 in game 2. |
| 2. Jungle Large Open (48 x 48 grid, 2304 rooms) | PASS: opened through level 140, arrival "in the entrance cell" (room 1162 of 2304), no crash. `AutoCheck` pass=3, fail=1, n/a=1: boss n/a (no Boss in the row) and stat-changes FAIL only because the row had `Properties` 0, so nothing was drawn. |
| 3. Walls and openings | Crypt: rooms with walls, pillars and openings seen in screenshots (the Crypt is very dark). Jungle: the character walked only on open ground near the entrance; the whole field was not walked and no room-to-room walk was done. |
| 4. Stability | 0 `[FAULT]`. One `[ERROR]`-class line in game 1, see below. |

Unexplained: once, in game 1 (Crypt Large), `BC_VERIFY: ptDrlgLevel->ptRoomFirst` (D2Common Drlg.cpp:882,
D2RCore.dll+0x360BC4) was logged right after two `cheat goto` calls with tile coordinates instead of subtile
coordinates (the character did not move). The game went on. It did not repeat in later games, including the Jungle
game. The goto calls are the likely cause; this was not proven. Also in game 1 the maze was built twice (on opening
and on entering); both builds finished.

Not tested: Cave Open (needs the Maze level at LevelType 3 and its own staging), the Small maps of Jungle and Crypt
Small beyond building, Snow refusal live, walking a Large map end to end, the Open boss on Jungle (no Boss row), a
second map in one game, multiplayer.
