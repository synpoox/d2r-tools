## 45. The maze builder's case for LevelType 13 (Act 2 Sewers, issue #243)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e6d85 8` and `dis 0x3eab30 200`, the jump table `0x3e76d8` read from the dump bytes); the live result is in the PR of #243.

| Fact | Evidence | Level |
|---|---|---|
| LevelType 13 is jump-table index 10 (`13 - 3`); the entry is `0x3e6d85`. | dword at `0x3e76d8 + 10 * 4` | static |
| The case calls the ring grower `0x3e9820(level, 2)`, then the shared walk `0x3e96d0(level)`, then `0x3eab30(level)`, then the common tail `0x3e75bc`. | `0x3e6d85..0x3e6da2` | static |
| `0x3eab30` is **not a room grower** (the research note's "own grower" was wrong). It draws one random number (`rng & 3`, from the level state at `+0x1e4`/`+0x1e8`) and then switches on the level id (`+0x1f8`): 47 (`0x3eac45`, a hand-built special room: it asks the room list `0x3ea1f0`, adds a room with `0x361140`, links it with `0x360bb0`), 48 (`0x3eac04`, two `0x3eb850` calls), 49 (`0x3eabdc`, one call) and 65 (`0x3eabb4`, one call). Every other level id jumps to the epilogue `0x3eb5fe` and returns. | `0x3eab38..0x3eabae`, `0x3eb5fe` | static |

Consequence: a slot level is none of 47, 48, 49 or 65, so `0x3eab30` only advances the level's random state and places nothing; the walk has already built the rooms. The case is the Jail recipe: ring grower, walk, no special-room call. No new hook is needed (no `ownGrower`, no signature). The row is `{"Sewer", 13, "MazeSewer", "Cartographer Sewer", 12, 301, {0,0,0,0}, true, 3000, 500, 0}`. Room pieces are 301 + exit mask (302..316; 301 itself is the Act 2 Town preset, so there is no closed piece and no filler ring, `ringCells` 0). The Prev, Next, Radament, Waypoint and Chest pieces (332..352) are never asked for by a slot level; the special-room hook suppresses them if the engine did ask.

The sewer rooms for the game's own levels 47 to 49 and 65 are placed by the level-id branches above and are not reachable from a slot level.
