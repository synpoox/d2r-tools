# Cartographer outdoor Siege (Bloody Foothills) tileset: what a Siege layout is built from (issue #288)

Companion to [cartographer-snow.md](cartographer-snow.md) and [cartographer.md](cartographer.md) section [8a](cartographer/08a-outdoor-build-hook.md) (the outdoor build hook). The pieces and their seams are read from tables and DS1 files of build 93847; the plan code is `plugins/cartographer/src/outdoor/siege_plan.*`. The research that came first is [a5-siege.md](../cartographer/tilesets/a5-siege.md).

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer outdoor builder, Siege family (`src/outdoor/siege_plan.*`, `tests/siege_tests.cpp`), through the slot row `OutdoorSiege` |
| Module | none: table and DS1 data only. No address, signature or byte guard is added or changed; the engine side is the existing outdoor build hook |
| Build identity | D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta |
| Data read | `levels.txt` (110), `lvltypes.txt` (30), `lvlprest.txt` (865..880), the DS1 files `strip01..13`, `striptownsiege`, `stripseigebar` under `data/global/tiles/expansion/siege/` |
| Guard | none needed. A request the plan cannot satisfy is refused with a reason and the game builds its own level |
| Validation | Unit tests (the plan, its independent check, a pinned golden). In game: two games in Hell opened the map, built it as planned and spawned the Boss and a Treasure event in their rooms ([siege-check](../cartographer/checks/2026-11-12-siege-check.md)). Seams are matched by socket letter from a hand reading of the DS1 walk flags (`kPieces`); no test checks them against the DS1 files. Template 110 is Layer 79, shared with Harrogath, so the 16-bit automap limit from #306 does not apply to it. Not settled: the north and south edge and the seams on screen beyond a few screenshots |

## Layout

The slot level is `levels.txt` row `Cartographer Siege`: LevelType 30 (Act 5 - Siege), 240 x 48 tiles at offset (6000, 3900), copied from level 110. It is the one slot level that is not square, so the installer has a `SizeY` for it. The 16 x 48 strips are stamped on the engine's 8-tile grid as 2 x 6 rooms each, 168 rooms for the 14-strip Medium map.

One line, west to east: 879 (barricade gate, closed west side), 878, free strips, the funnel block 875, 874, 873, 872, 871, 870, 869, 868 (fixed order), free strips, 865 (town, closed east side). The free strips are 866, 867, 876 and 877; the map `Size` sets how many are used (Small 12 strips, Medium 13, Large 14, ExtraLarge 15) and the seed sets their order, from the orders whose seams all match.

## Seams

Each strip has a west and an east seam, a corridor of open rows. The plan gives each seam a letter (the table is `pieces()`); neighbours must carry the same letter. A to E are the wide seams (open rows 5..40, 5..39, 5..37, 7..40 and 10..38), F to J the funnel's narrower ones. The funnel pair 874/873 differs by one row at the wall. The letters come from the DS1 walk flags (flag bit 0 of the outermost subtile column of each strip), not from the game, which pairs strips only by position.

## Arrival, Boss, event

The entrance is the strip next to the town strip (index n - 2). The Boss and the event stand on distinct wide strips: the Boss on the one farthest from the entrance, the event on the one whose nearer neighbour of {entrance, Boss} is farthest, ties by seed.

## Open

- The north and south edge: whether the strips leave a void or hard border there was not settled in game.
- The seams are checked in the data, and looked right in a few screenshots; a full walk was not done.
- The 16 x 32 piece 880 (barricade to siege) is not used.
