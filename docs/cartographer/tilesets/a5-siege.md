# Act 5 siege (issue #288)

Status: built as the `Siege` outdoor tileset (`src/outdoor/siege_plan.*`); evidence in [cartographer-siege.md](../../reverse-engineering/cartographer-siege.md), game check in [siege-check](../checks/2026-11-12-siege-check.md). The notes below are the research that came first.

Research: Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 110 Act 5 - Siege 1, 240 x 48, DrlgType 3, LevelType 30 |
| LevelType | 30 "Act 5 - Siege" (ascendors, camp, fence, ground, rockcliff and more). The same act and palette as the slot level |
| `lvlprest.txt` | Def 865-880 (16 rows), 16 tiles wide and 48 high, one file each: 865 to-town, 866-878 thirteen strips, 879 to-barricade; 880 barricade-to-siege is 16 x 32 |
| `lvlsub.txt` | None (SubType -1) |
| Waypoint | None (255) |
| Slot row | New: LevelType 30. At 240 x 48 it is the widest slot level, so the installer's offset and size validation must allow it |

## What the planners would need

A one-dimensional chain: 15 strips of 16 x 48 across 240 tiles, read left to right.

- **Planner.** Snow's "stamps only" style with a single line. Fixed order: to-town at one end, strips in the middle, to-barricade at the other. Only the strips 866-878 can be interchanged, and only where their seams match; the DS1 edge scan shows how much the strips differ at the seams.
- **Variety.** Low. The seed only shuffles the middle strips.
- **Border.** No ring. North and south sides are cliffs or fences inside the strips; whether the very north and south edge is a hard border or a void edge needs a game check.
- **Void.** The strips cover the full 48 rows, so nothing is left to void if all 15 columns are stamped.
- **Arrival and boss.** To-town is the arrival; the far end is the natural boss room.

## Risk, effort, checks

- Risk: low-medium. Simple code; the seam match between strips is the unknown.
- Effort: S-M. The smallest planner of the six; extra work only for the 240-wide slot size.
- Game checks: strip-to-strip seams (does any pair look wrong side by side); fill mode of LevelType 30; the void edge on the north and south sides; whether the installer's size check accepts 240 x 48; the 16 x 32 final piece against 16 x 48 strips.
