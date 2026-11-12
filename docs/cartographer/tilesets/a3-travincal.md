# Act 3 Travincal (#282): whole-preset only (fixed outdoor composition)

**Verdict: whole-preset only.** Effort **M**: it is DrlgType 3, so the outdoor slot and a fixed stamp list are needed, not the preset slot of [a1-tristram.md](a1-tristram.md).

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 83 Travincal: **DrlgType 3 (outdoor)**, LevelType 22 (Kurast), 64 x 64 tiles, SubType -1, Vis to level 100 (Durance 1). Not a DrlgType 2 preset, and it has no LvlPrest row with its own LevelId.
- Six single-DS1 piece rows, LvlPrest Def 653 to 658 "Act 3 - Travincal NW, N, NE, SW, S, SE": the corners 16 x 32, N and S 32 x 32. Widths 16 + 32 + 16 and heights 32 + 32 make exactly the 64 x 64 level.
- Each piece has **one** DS1 and a unique role (the compound, the river side, the stairs), so the six are not interchangeable and there is no variant to draw.

## Recombination

- **Maze path:** no (not a maze, no mask doors).
- **Outdoor stamp planner:** the engine side fits (the Act 5 slot already stamps Act 3 pieces: Jungle ran live in Act 5), and the sizes are multiples of the 8-tile grid. But the only valid arrangement is the vanilla one, so the planner is a constant six-stamp list ("Planner::Fixed"), not a picker.
- It could later be a landmark inside a generated Kurast map (plan phase 9), since Kurast cells are 8 tiles and these pieces are 16 and 32.

## What a map could be

The same layout every time, random boss, event, monsters and stats. The Council superuniques are placed by level id, so a slot level would not spawn them (inference); the Cartographer boss stands in for them. Rectangles for entrance, event and boss are constants (the south edge for the entrance, the compound for the boss).

## Game checks needed

1. Native stamp positions: log `0x3ecfc0` (Def, file, cell) while vanilla level 83 builds, to get the six cell positions (the same method as spike S1).
2. Outdoor slot with LevelType 22 builds the six stamps in an Act 5 slot, with Act 3 Kurast tiles loaded.
3. Void/edge behaviour outside the six pieces (none expected, they tile the rectangle), arrival cell, walkability across the piece joins.
4. Boss and event rectangles hold; Council superuniques stay off.

## Shared LevelType 22 (Kurast, #281, and Durance, #283)

LevelType 22 is the DT1 set shared by Kurast 1 to 4 (79 to 82), Travincal (83), the Kurast temples (94 to 99) and Durance (100 to 102). **#281 does not cover Travincal.** What it gives is the Kurast outdoor slot row (LevelType 22) and the tile set loading in an Act 5 slot; Kurast pieces are the 8 x 8 Slums/Burbs/Metro families, none of which is a Travincal piece. Travincal needs its own fixed planner (or a `Planner::Fixed` case on the same slot) and its own catalogue row, after #281's slot exists.
