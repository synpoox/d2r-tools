# Act 3 Durance of Hate (#283): generate as maze

**Verdict: generate as maze.** Effort **S to M**: the tileset is already catalogued and the ring-grower hook exists; what remains is the data row, a no-op check and a game check.

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Levels 100 Mephisto 1 and 101 Mephisto 2: **DrlgType 1 (maze)**, LevelType 22, 200 x 200. LvlMaze rows: level 100 has 10 / 16 / 16 rooms (Normal / Nightmare / Hell), level 101 has 12 / 48 / 48, rooms 12 x 12 tiles.
- Level 102 Mephisto 3: DrlgType 2, 41 x 29, LvlPrest Def 796 "Act 3 - Mephisto Complex", **one DS1** (Mephisto's lair). A fixed preset; not needed for a map (the map Boss replaces Mephisto).
- Pieces, LvlPrest Def 754 to 768 "Act 3 - Mephisto W ... NSEW": one per exit mask (Def = 753 + mask), 12 x 12, **two DS1 variants** each. Theme rooms Def 769 to 783 (768 + mask). Entrance family "Prev" W/E/S/N = Def 784 to 787, "Next" 788 to 791, "Waypoint" 792 to 795.

## Can the maze path recombine it

Yes, and it is the same shape as Cave and Crypt: base + mask pieces, a one-exit entrance family in the order W, E, S, N, which is what `DeadEndFamily` expects. `plugins/cartographer/tools/tilesets.tsv` already lists `Durance` (maze, LevelType 22, 30 pieces, group "maze, one extra hook") and `catalogue.tsv` has the rows. "One extra hook" means the ring grower `0x3e9820` must be a no-op for this tileset (`callsRingGrower` in `maze.h`); that hook exists, and the plan notes Durance as steering with it.

## Game checks needed

1. Confirm the Durance case in the maze builder calls the ring grower (and whether a walk follows it: `callsRingGrower` / `walkFollowsRing`).
2. A maze slot with LevelType 22 and a private LvlMaze row (12-tile rooms) builds in Act 5 with Act 3 tiles loaded (Cave and Crypt are the live precedent for Act 1 types in an Act 5 slot).
3. The steered plan builds: rooms as planned, no crash-guard entry, arrival in the entrance (Prev) room, walkable joins, automap.
4. Boss and event rooms are the planned rooms; waypoint and Next pieces are not placed.

## Shared LevelType 22 (Kurast #281, Travincal #282)

Same DT1 set as Kurast and Travincal, but a different builder: Durance uses the **maze** slot, Kurast the outdoor slot. **#281 does not cover Durance**, and Durance does not need it: the maze slot only needs LevelType 22. The shared tile set loading in an Act 5 slot is proved once by whichever of the two runs first.
