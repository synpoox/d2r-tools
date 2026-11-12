# Act 2 Arcane Sanctuary (#280): generate as maze

**Verdict: generate as maze.** Effort **L** (an own-grower spike and hook; the piece data is already catalogued).

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run, no engine disassembly (no dump available to this research).

## How vanilla builds it

- Level 74 Arcane Sanctuary: **DrlgType 1 (maze)**, LevelType 19, 200 x 200 tiles, Position 1, Quest 11. Not a single preset despite the issue text: LvlMaze row for level 74 has 61 rooms of 12 x 12 tiles in all three difficulties and Merge 1000.
- LevelType 19 is used by this level alone, so it is shared with no other tileset issue.
- Pieces, LvlPrest Def 510 to 524 "Act 2 - Arcane W ... NSEW": one per exit mask (Def = 509 + mask, W 1, E 2, S 4, N 8), 12 x 12, each with 1 to 5 DS1 variants. Plus Def 525 to 528 "Arcane Summoner W/E/S/N", the four one-exit rooms that hold the Summoner. There is **no Prev, Next, Waypoint or Theme family** (the maze entrance is a room of the grown layout).

## Can the maze path recombine it

Yes in principle, and the data is ready: `plugins/cartographer/tools/tilesets.tsv` lists Arcane (15 mask pieces, group "maze, own growers") and `catalogue.tsv` has the rows. What is missing is the engine side: `maze.h` marks it `steerable = false` because the level grows its own rooms (the `Maze, own growers` row of the plan). The steering hooks replace the room walk and the ring grower only for tilesets whose case in the maze builder uses them; Arcane's case is a different grower that has not been located.

Design points for a Cartographer Arcane:
- Entrance: an ordinary room (the `DeadEndFamily` is absent), as the core already allows.
- Boss room: a plain one-exit dead end (Def 510, 511, 513 or 517), **not** the Summoner pieces, which carry the vanilla Summoner.
- Rooms are 12 tiles with void between platforms, so the planner's spacing and the void/collision at room edges need checking.

## Game checks needed

1. RE spike: find the Arcane case in the maze builder (`0x3e69e0`) and its grower, record signature and patch length in `docs/reverse-engineering/cartographer/` (static, then live).
2. A maze slot with LevelType 19 and a private LvlMaze row (12-tile rooms) builds in Act 5, with Act 2 tiles loaded.
3. The steered plan builds with the grower neutralised: rooms placed as planned, no crash-guard entry.
4. Arrival in the entrance room (Position 0 in the slot), walkability across room joins, automap.
5. Boss and event rooms are the planned rooms.
6. The Summoner pieces are never placed (or are handled) and the vanilla Summoner/Tyrael quest logic does not fire.

## Shared LevelType

None. Nothing else covers Arcane.
