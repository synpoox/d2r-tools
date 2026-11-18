# Act 3 Kurast, with Travincal and Durance (issues #281, #282, #283)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 79-81 Kurast 1-3, 80 x 64; 82 Kurast 4, 48 x 16; 83 Travincal, 64 x 64. All DrlgType 3, LevelType 22 |
| LevelType | 22 "Act 3 - Kurast" (dark grass, mud, Kurast floors, huts, interior and more). The Durance of Hate (#283) shares LevelType 22 but is a maze |
| `lvlprest.txt` | Def 605-658 (54 rows). Three districts: Slums 605-618, Burbs 619-635, Metro 636-651. Each has N/S/E/W edges (8 x 8, 1-3 files), four corners (8 x 8), a north and south gate, and four fill sizes (8 x 8, 8 x 16, 16 x 8, 16 x 16; 2-5 files each). Burbs and Metro add sewer (8 x 8) and temple (16 x 16); Burbs has a waypoint piece (631). 652 bridge, 48 x 16. 653-658 Travincal: six pieces, 16 x 32 and 32 x 32 |
| `lvlsub.txt` | None for these levels (SubType -1) |
| Waypoints | Levels 79-81 and 83: numbers 22-25 |
| Slot row | New: LevelType 22 |

## What the planners would need

Kurast is "bordered districts": each district is an edged rectangle (8 x 8 frame with gates) packed with fills.

- **Planner.** The frame-plus-fill planner (Wilderness, Desert) fits: frame from the edge and corner ids of one district, fills from the four size classes. A map is one to three districts; the gates are the connection points and the bridge (652) joins them.
- **Border.** The engine border pass has no entry for LevelType 22. All edges and corners are explicit rows, so the plan stamps them by id and the corner table is not needed.
- **Waypoint, sewer, temple.** The Burbs waypoint piece can serve as the map's waypoint. Sewer and temple pieces lead into dungeon levels; keep them out of a first version.
- **Travincal (#282).** Six pieces (653-658) in a fixed arrangement with no variety. A trivial stamp list (small alone, but one more slot level). The fixed-layout note is `a3-travincal.md`.
- **Durance (#283).** A maze, not outdoor; only the LevelType is shared. Not covered here; see `a3-durance-of-hate.md`.
- **Walkability.** Huts and walls block natively; gates are the only openings.

## Risk, effort, checks

- Risk: medium. The unknown fill mode for LevelType 22 (an entry exists in the lookup) and whether gates line up when districts sit next to each other.
- Effort: M for Kurast districts; S for Travincal alone; Durance belongs to the maze notes.
- Game checks: fill mode of LevelType 22; north gate against south gate alignment; temple and sewer entrances spawning in the slot level; the bridge between two districts on the 8-tile grid.

## As built (issues #507, #281)

One district, the Burbs, as a `FrameFillTileset` row (`kKurast`, LevelType 22, `OutdoorKurast` slot, offset 6000, 3660, 128 x 112): a ring of Burbs border pieces (W 622, N 619, E 621, S 620, corners 624, 623, 625, 626) round one fill set `Burbs` (632 8 x 8, 633 8 x 16, 634 16 x 8, 635 16 x 16). Maps and arenas (`Tileset` Kurast, `Fill` Burbs) both use it. No gates, bridge, sewer, temple, waypoint, Slums or Metro.

Answers to the open checks, read from the DS1/DT1 walk flags offline:

- **Fill mode of LevelType 22:** not reached. The plan stamps every cell with a piece, so no fill pass runs.
- **Gates and bridge:** not used. One district needs neither.
- **Sewer and temple:** left out.
- **Ring:** the eight Burbs border pieces form a closed ring with floor on the inner side and no void tiles.
- **Edges (#550):** the ring is not a cliff. N (619) and E (621) end in a low wall, S (620) and W (622) in a street of open floor on their outer side (DS1 walk flags, `tools/d2r_readers.py`). Past the level the world is black, so a map showed black void beyond the ring. A map now has an outer ring (`kKurast.outer`) of the same pieces back to back, as Kurast districts meet in the game: W 621, N 620, E 622, S 619, corners NW 625, NE 626, SE 624, SW 623. It needs 4 cells more each way, so in the 128 x 112 slot Small, Medium and Large get it and ExtraLarge (144 x 128 with it) keeps the one ring. Arenas keep one ring. The outer ring rooms populate like the inner ring's.
- **Risk:** some Burbs fill files carry link-like wall tiles (main 8, 12, 16); the planner cannot exclude files. Vanilla uses them. Game check result: [checks/kurast.md](../checks/kurast.md).
