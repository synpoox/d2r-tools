# Act 5 ice caves (issue #245)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 113, 115, 118 Ice Cave 1, 2, 3 (Drifter Cavern, Icy Cellar, Glacial Caves). Sub-levels 114, 116, 119 (Ice Cave 1A, 2A, 3A) are DrlgType 1 with Rooms 1 and big single rooms (64 and 32 tiles) |
| LevelType / DrlgType | LevelType 33 "Act 5 - Ice Caves" (one DT1 file); DrlgType 1; palette 4 (Act 5, the slot level's own act) |
| `lvlmaze.txt` | 113 and 115: Rooms 10; 118: Rooms 15; SizeX = SizeY = 16, Merge 500 in all. 114: 64 x 64, 116 and 119: 32 x 32, Rooms 1 |
| `lvlprest.txt` room pieces | Def 1003-1017: the 15 exit-mask pieces, 16 x 16, 3 files each (`pieceBase` 1002, base + mask holds) |
| Entrance (Prev) | Def 1018-1021: one-exit W, E, S, N (same order as Cave's group) |
| Exit (Next), Down | Def 1022-1025 Next and 1026-1029 Down, one-exit W, E, S, N. Not placed |
| Dead ends, specials | One-exit mask pieces; Theme 1030-1033 (one-exit); waypoint 1034-1037 (one-exit, N, S, E, W). Large River A/B (64 x 64) and Pool A/B (32 x 32) are for the sub-levels, not the maze |
| Warps | Up L/R (73), Down L/R (74), Down Floor (75): left and right variants by wall side |

## Does the Cave/Crypt path work unchanged?

Closest of the seven: the piece set and Prev family are the Cave pattern exactly. One new `builtinTileset` entry (levelType 33, roomTiles 16, pieceBase 1002, prev 1018-1021) plus the usual table touch points. Still open, as for every non-Cave tileset:

- **A slot per tileset** (one Maze slot is one LevelType; shared work).
- **The `0x3e69e0` Ice case** is not recorded (ring grower call, walk after it). The plan lists Ice among the "one extra hook" tilesets (ring grower as a no-op), unchecked here.
- **Waypoint pieces.** The vanilla Ice levels place a waypoint room through the special-room calls; Cartographer answers every non-entrance `0x3eb850` call with a suppression, so the waypoint is simply absent. Not expected to matter, but unchecked.
- **Tiles.** Act 5 tiles in the Act 5 slot level: no cross-act problem. At 360 tiles 21 rooms fit across.

## Risk, effort, checks

- Risk: low. The same act as the slot level, the same piece rules, the plan's earlier note that Act 5 Ice drew fine in Act 1.
- Effort: S for the tileset entry and tests (plus the shared slot work); M if the Ice builder case differs from Cave's.
- Game checks: static read of the Ice case of `0x3e69e0`; built grid equals planned grid; arrival in the entrance cell; Boss and event rooms stand in their rooms; Hell run.
