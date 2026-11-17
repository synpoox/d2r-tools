## 46. The maze builder's case for LevelType 10 (Act 1 Catacombs) and its grower `0x3e8610` (issue #275)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e6d06 40` and `dis 0x3e8610 120`) and **game** (slot 11, a Catacombs map on Small, Medium, Large and ExtraLarge).

| Fact | Evidence | Level |
|---|---|---|
| LevelType 10 is jump-table index 7 (`10 - 3`); the case is at `0x3e6d06`. | dword at `0x3e76d8 + 7 * 4` | static |
| The case calls the grower `0x3e8610(level, level id == 0x22)` (the flag is true only on level 34), then the shared walk `0x3e96d0`, then one `0x3eb850` special-room call (the Next group, call-table rows 20..23 at `0x1d0f1f0`), and jumps to the tail `0x3e75bc`. Level 35 adds one Waypoint call (rows 24..27). | disassembly | static |
| The grower adds rooms and links next to the first room, using the level's own seed, and it writes the first room's piece id itself: 288 (`0x3e9143`, `0x3e929f`), 289 (`0x3e96a2`) and, on the level-34 branch, 290 (`0x3e8cf3`, `0x3e8e51`), then sets the fixed flag and file -1 (`0x3e96a8..0x3e96b5`). 288..290 are the Prev (Up stairs) pieces. It never calls `0x3eb850` or `0x3e77a0`. | disassembly | static |
| The grower runs before the walk, so the walk hook still fires. But the grower pre-places extra rooms (`SteeredWalk` needs exactly the one first room) and puts a stairs piece in the first room, so an unsteered or fallback slot build that ran it would have stairs, and the portal must be the only way in and out. | game: the walk hook saw more than one room until the grower was skipped | game |
| Signature `48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 30` (one match; the hook size is 19). | `dump_dis.py pat` | static |

Consequence: `CatacombsGrowerHook` returns without growing on every slot-level build (steered, unsteered or fallback; vanilla levels 34..36 have no build context and run the original). The walk then runs on the lone first room: `SteeredWalk` builds the plan, and a fallback grows from that room as Cave does, with no Prev piece. The special-room hook suppresses the Next call; Prev and Waypoint pieces are never placed. The row is `{"Catacombs", 10, ..., 12, 257, {0,0,0,0}, false, 3500, 500, 0}`: 257 is the Cathedral preset, not a room piece, so the mask pieces start at 258. No ring pieces are needed: the outer rooms show the usual N and W walls with black past the S and E sides, and no walkable void floor was seen on Medium.
