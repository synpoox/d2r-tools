# Act 4 plains: Mesa (issue #240)

Status: built as the `Mesa` outdoor tileset (`src/outdoor/frame_fill_plan.*`, row `kMesa`, LevelType 27). Not yet checked in game.

Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 104 Mesa 1 (80 x 64), 105 Mesa 2 (64 x 80), 106 Mesa 3 (80 x 64). DrlgType 3, LevelType 27 |
| LevelType | 27 "Act 4 - Mesa" (mesa floor, stairs, arch, brick, chain and distant walls). The palette is Act 4 |
| `lvlprest.txt` | Def 798-835 (38 rows). 798 Fortress transition (8 x 24); 799-810 twelve border pieces, 8 x 8, 3 files each; 811 warp (8 x 8); three fill sets, Mesa 1 (812-816), Mesa 2 (817-821) and Mesa 3 (823-827), each in 24 x 24, 16 x 16, 16 x 8, 8 x 16 and 8 x 8 with 4-5 files each; 822 Izual spot (8 x 8); 828-835 Pits fills (dungeon levels, not outdoors) |
| `lvlsub.txt` | None (SubType -1) |
| Waypoint | Level 106 only: number 28 |
| Slot row | New: LevelType 27 |

## What the planners would need

Mesa is the most regular field: three parallel fill sets with all five sizes, and a LevelType the engine border pass supports.

- **Planner.** The frame-plus-fill planner with one fill set per map (the sets differ in look and in which 24 x 24 pieces they have). The 24 x 24 pieces fit on the 8-tile grid when mixed with 8 and 16.
- **Border.** LevelType 27 is supported by the border pass, but the Act 5 builder skips it: route B (the plan stamps the 12 border pieces). A plain rectangle.
- **Walkability.** Mesa walls and drops block natively; the 4-5 variants per fill give visible variety.
- **Specials.** Izual's spot (822) and the warp (811) are single 8 x 8 stamps, both optional. The Fortress transition is not needed.

## Risk, effort, checks

- Risk: low-medium. The most regular piece set, a border pass known to support the type.
- Effort: M. The easiest field tileset, a good first one on which to build the shared frame planner.
- Game checks: fill mode of LevelType 27; whether mixed 24-, 16- and 8-tile fills leave cells unfilled (the packing must reach every cell); the void edge against the Act 4 palette; Izual's spot as an arrival or boss cell.

## As built

- Frame: a ring of 8 x 8 border pieces (799-806, 3 files each), one cell deep, around an interior of 8 x 6 (Small) to 14 x 12 (ExtraLarge) grid cells. The ring is the plan's own border, so the edge is solved inside the tileset.
- Ring orientation, read from the DS1 walk flags (floor on the inner side): west 800, north 801, east 802, south 799; corners NW 804, NE 805, SE 806, SW 803.
- Fill: one set per map (812-816, 817-821 or 823-827), packed row-major with weighted picks; the 8 x 8 piece fits any cell, so every cell is covered. Pieces 823-826 have 4 files, the rest 5.
- Rooms: entrance (an inner corner picked by seed), boss (farthest) and event (farthest from both) are 2 x 2 cells each. The warp (811), Izual's spot (822) and the Fortress transition (798) are not used.
- Installer: slot `OutdoorMesa`, level "Cartographer Mesa" (Act 5 template row 111, LevelType 27, 128 x 112 tiles).
- Not proven in game: the stamper accepting these ids, void-free edges on each Size, the Act 4 palette in an Act 5 level, decoration walls inside fill pieces.
