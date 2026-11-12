# Act 3 Kurast sewers (issue #285)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 92 Sewer 1 (maze). 93 Sewer 2 is a fixed preset (DrlgType 2, 18 x 22) |
| LevelType / DrlgType | LevelType 25 "Act 3 - Sewer" (three DT1 files); DrlgType 1; palette 2. Not the Act 2 sewer (LevelType 13, a different grower) |
| `lvlmaze.txt` | Rooms 32 in all difficulties, SizeX = SizeY = 12, Merge 800 |
| `lvlprest.txt` room pieces | Def 705-719: the 15 exit-mask pieces, 12 x 12, 2 files each (`pieceBase` 704, base + mask holds) |
| Entrance (Prev) | Def 735-738: Prev SW, SE, NW, NE, that is two-exit corner pieces, not one-exit |
| Other specials | Def 739-742 Drain W, E, S, N and Def 743-746 Chest W, E, S, N (one-exit); Def 747 Treasure X is level 93's preset. Theme pieces 720-734. No Next or Down group |
| Dead ends | One-exit mask pieces W, E, S, N, plus the Drain and Chest pieces |
| Warps | Up warps 58 and 59, warp 60 Sewer Down to level 93; Vis links to levels 80, 81 and 93 |

## Does the Cave/Crypt path work unchanged?

No, one thing differs. `Tileset::prev` and `planMaze` assume a one-exit entrance: the entrance cell must have exactly one exit and the piece is picked by that side. The Sewer Prev pieces open on two sides. Options, each more than a table entry:

1. Key the Prev family by exit mask (Sewer masks 5, 6, 9, 10) and let the layout generator make an entrance cell with that mask (a change in `layout.cpp` and its pinned tests).
2. No Prev piece (`prev.present = false`): the entrance is an ordinary mask room. The arrival finder then falls back to the centre room or a random room (05-maze-slot.md Q3), so the arrival would not be in the entrance cell; the done-when "player at the portal in the entrance cell" fails unless the arrival is steered another way.
3. Use a Drain piece (one-exit) as the entrance: what its DS1 holds is unknown (a drain to level 93 in vanilla), so it needs a game check.

Also needed: the slot generalisation and the unrecorded `0x3e69e0` case (ring grower, walk), as in the Dungeon note. Merge 800 is vanilla only; the Cartographer private row sets its own merge. 12 is not a multiple of 8, so the engine-room split needs checking (Dungeon note). At 360 tiles 29 rooms fit across. The tileset name `Sewer` is already in the refused list in `tables.cpp`; Act 2 and Act 3 sewers would clash, so name this one distinctly.

## Risk, effort, checks

- Risk: medium to high. The entrance rule is a core layout change, and the Act 3 tiles ride in an Act 5 slot level.
- Effort: M to L (entrance rule change plus the tileset entry); S if option 2 is accepted with a different arrival rule.
- Game checks: the `0x3e69e0` Sewer case; what a Prev, a Drain and a Chest room hold; arrival under each option; built grid equal to the plan; a Hell run for the Boss and event.
