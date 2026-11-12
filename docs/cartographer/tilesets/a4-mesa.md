# Act 4 plains: Mesa (issue #240)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

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
