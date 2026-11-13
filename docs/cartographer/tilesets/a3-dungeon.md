# Act 3 dungeon (issue #244)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied. Engine facts are from `docs/reverse-engineering/cartographer/` (sections 05 and 01a).

| Item | Finding |
|---|---|
| Levels | 86-89 (Dungeon 1 A/B, 2 A/B: the Swampy Pit and Flayer Dungeon levels). 90 and 91 (Dungeon Treasure) are fixed presets (DrlgType 2) |
| LevelType / DrlgType | LevelType 24 "Act 3 - Dungeon" (one DT1 file in LvlTypes); DrlgType 1 (maze); palette 2 (Act 3) |
| `lvlmaze.txt` | Rooms 4 in all difficulties, SizeX = SizeY = 14, Merge 500 |
| `lvlprest.txt` room pieces | Def 665-679: the 15 exit-mask pieces W..NSEW, 14 x 14, 3 files each. So `pieceBase` 664 and "piece = base + mask" holds (same rule as Cave 52 and Crypt 108) |
| Theme pieces | Def 680-694, 15 pieces (Cartographer picks only base + mask, so they are unused) |
| Entrance (Prev) | Def 695-698: one-exit W, E, S, N, in the same order as Cave's Prev group, so a prev family of 695-698 fits unchanged |
| Exit (Next) | Def 699-702, one-exit W, E, S, N: not placed (a Cartographer map has no exit) |
| Dead ends, specials | Dead ends are the one-exit mask pieces. No waypoint piece. Treasure rooms are separate preset levels, not maze pieces |
| Warps | Warp ids 55 and 56 on the levels; the Prev piece carries the up-stairs tile |

## Does the Cave/Crypt path work unchanged?

Almost. The plan side is one new `builtinTileset` entry (name Dungeon, levelType 24, roomTiles 14, pieceBase 664, prev 695-698), plus the table touch points of `adding-a-tileset.md` (ready list, README, `tables.md`). It needs more in three places:

- **A slot per tileset.** Each maze tileset has a slot level of its own (a row of `MazeTilesets` in `src/maze/maze_tilesets.h`, installed by `$MazeTilesets` in `install.ps1`; #303). Dungeon needs a row with LevelType 24.
- **The builder case.** Only the Cave and Crypt cases of the maze builder `0x3e69e0` are recorded. Whether the Dungeon case calls the ring grower `0x3e9820` (the plugin has `ring.noOp`) and whether a walk follows it (`walkFollowsRing`) is not recorded. The plan lists Dungeon among the tilesets that steer once the ring grower is a no-op, from the pd2-resurrected prototype, not from a game check in this repo.
- **Room size 14.** Cave (24) and Crypt (8) are multiples of 8, and the probe saw 8 x 8 engine rooms. 14 is not, so how the game splits a 14 x 14 piece into engine rooms, and whether the entrance-cell arrival check (it divides by `roomTiles`) holds, is unknown.

At the 360 tile limit 25 rooms fit across (12 each side of the first room).

## Risk, effort, checks

- Risk: low to medium. Pieces and Prev family match the proven pattern; the unknowns are the builder case and the 14-tile rooms. Act 3 tiles in an Act 5 slot level is the cross-act preload risk of plan section 6 (Ice drew fine in Act 1; Act 3 is unchecked).
- Effort: S for the tileset entry and tests; the slot generalisation is shared work.
- Game checks: static read of the `0x3e69e0` Dungeon case (ring grower, walk, `0x3eb850` call count and order); a built map with built grid equal to planned grid; arrival in the entrance cell; Act 3 tiles drawn in the Act 5 slot; a Hell run for the Boss and event room.
