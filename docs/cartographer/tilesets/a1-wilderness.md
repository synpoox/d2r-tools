# Act 1 fields: Wilderness (issue #238)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 2-7 Act 1 - Wilderness 1-6, all 80 x 80, DrlgType 3 |
| LevelType | 2 "Act 1 - Wilderness" (floor, trees, stonewall, fence, tree groups, river and more). The palette is Act 1; the slot level is Act 5 |
| `lvlprest.txt` | Def 4-41 (38 rows). 4-15 border pieces, 8 x 8, 1-3 files each; 16-25 cliff borders and cave mouths, 8 x 8; 26-28 river upper, river lower and bridge, 8 x 8; 29-31 stone and corral fills, 8 x 8; 32-37 fence fills (16 x 16, 8 x 16, 16 x 8, 8 x 8); 38-39 swamp fills; 40 tree fill; 41 ruin, 16 x 16 |
| `lvlsub.txt` | Rows 4-15 (SubType 6, SubTheme 0-4 per level): scatter, waypoint and shrine substitutions, applied through the `D2RCore` TileSub callback, not by the plan |
| Waypoints | Levels 3-6 carry Waypoint numbers 1-4; the object comes from a substitution or a DS1 object, not from a piece row |
| Slot row | New: LevelType 2 (Jungle is 21, Snow 31). One more `OutdoorTilesets` row, one more installer row, one new Act 5 levels row |

## What the planners would need

Neither Jungle (exit-mask picker, 32 x 32 cells) nor Snow (stamp lines of Wang pieces, 16 x 16 cells) fits. Wilderness is a bordered field: an 8 x 8 border ring plus open fill pieces of 8 and 16 tiles, with river, cliffs and bridge as optional features.

- **Planner.** A new "frame plus fill" planner: walk a grid of 8 x 8 cells, stamp the border ring, then pack fills with a seeded random fit (16 x 16 first, then 16 x 8 or 8 x 16, then 8 x 8). Reuse `StampPlan`, `chooseCellRoom`, `planArrival` and the plan cache.
- **Border.** The engine border pass (`0x3efaf0`) knows LevelType 2, but the Act 5 builder does not call it (it calls `0x3efa20`). The plan stamps border and corner pieces itself ("route B" in the outdoor plan; corner table at `0x1d10c40`). The ring is a plain rectangle, the easy case.
- **Void.** Every cell the plan does not stamp must be void-marked, or `0x3ee780` makes tile-fill rooms. The fill mode for LevelType 2 is unknown (see checks).
- **Walkability.** Cliff and river tiles are blocked by the DT1 flags, so they block natively. A river needs a crossing, which only the bridge piece gives; leave the river out of a first version.
- **Decoration.** LvlSub substitutions may not run on a hook-built level, and the installer sets the slot row's SubType to -1. Do not count on them; place a waypoint through the plan if wanted.
- **Left out.** Cave mouths (24, 25) lead into dungeon levels.

## Risk, effort, checks

- Risk: medium. The fill packing and ring are new code; the unknown fill mode and decoration under the hook are the real risks.
- Effort: L. The most pieces of the six, plus the river question.
- Game checks: fill mode of LevelType 2 on an unstamped cell; whether DS1-embedded waypoints and cave entrances spawn in the slot level (Waypoint 255); whether the plan's own border really replaces the skipped border pass; corner piece choice by file index; the ring against the 80 x 80 size.
