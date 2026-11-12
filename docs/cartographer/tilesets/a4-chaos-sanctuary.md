# Act 4 Chaos Sanctuary (#287): whole-preset only (fixed star, arm variants)

**Verdict: whole-preset only.** Effort **L**: DrlgType 3 with a hard-coded star, so it needs the outdoor slot, a fixed stamp list, native placement data from the game, and the seal/Diablo behaviour checked.

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 108 Diablo 1: **DrlgType 3 (outdoor)**, LevelType 28 (Lava), 120 x 120 tiles = 5 x 5 pieces of 24, no LvlPrest row with LevelId 108, Vis to level 107.
- Six piece rows, all 24 x 24: Def 857 "Act 4 - Diablo Entry" (1 DS1), Def 858 to 861 "Diablo Arm W, E, S, N" (2 DS1 each), Def 862 "Diablo Heart" (1 DS1). The seals and the Diablo summon spot are objects inside the arm and heart DS1s.
- The "Act 4 - Mesa 3 ... Chaos" rows (Def 823 to 827) belong to the Mesa type (City of the Damned, level 106), not to the Sanctuary.

## Recombination

- **Maze path:** no. Not a maze.
- **Outdoor stamp planner:** the pieces stamp (Act 5 slot stamping Act 3 pieces ran live), and 24 is three 8-tile grid cells. But the arms are named for the side they sit on and the star is a fixed shape, so the only freedom is the DS1 variant per arm (2 files each, 2^4 = 16 combinations at most, which the vanilla level RNG already draws). A "Planner::Fixed" with six stamps and per-arm variant choice is the most that can be generated.
- Where each piece sits on the 5 x 5 grid, and what fills the other cells, is not in the tables.

## What a map could be

The same star every time (entry, four arms, heart), random boss, event, monsters and stats. Natural placement: entrance at the Entry piece, event in an arm, boss at the Heart.

## Game checks needed

1. Native placement: log `0x3ecfc0` (Def, file, cell) while vanilla level 108 builds, to get the six positions and the fill around them (the S1 spike method).
2. Outdoor slot with LevelType 28 builds the stamps in an Act 5 slot; what the empty cells look like (void or lava fill) and that they block movement.
3. Seals: do the seal objects and the Diablo summon behave (quest and object code may be keyed to level 108) in a slot level, or sit inert. If they fire, it is a vanilla boss in a map.
4. Arrival cell, walkability across the piece joins, boss and event rectangles.

## Shared LevelType 28 (River of Flame / lava, #239)

LevelType 28 is shared with level 107 River of Flame (Lava 1, **DrlgType 1 maze**, 6 rooms of 24 x 24; tilesets.tsv already lists `Lava` as a maze, ready, 15 pieces). #239's text says "outdoor", but the tables say maze. Either way **#239 does not cover the Sanctuary**: its pieces are the Lava mask family, not Def 857 to 862. What #239 gives Chaos is the LevelType 28 tile set loading in an Act 5 slot and, if it ever makes an outdoor lava slot, the lava-edge-blocks-movement rule that check 2 here also needs.
