# Adding an outdoor tileset to Cartographer

For developers. Jungle and Snow share the outdoor plumbing in `plugins/cartographer/src/outdoor/` (`outdoor_plan.*`, `stamp.*`, `outdoor.h`). A new tileset (Barbarian Highlands style fields, a third Act 5 look) is one row in a table plus its planner and data. The evidence for the two tilesets that exist is in [cartographer-snow.md](../reverse-engineering/cartographer-snow.md) and the outdoor builder notes there.

## Every touch point

The table is `OutdoorTilesets` in `src/outdoor/outdoor_tilesets.h`. A row is `{name, levelType, slot, levelName, planner}`: the `maps.txt` Tileset, the `levels.txt` LevelType its slot level must have, the `slots.txt` row name, the `levels.txt` Name the installer gives the level, and a `Planner` value.

1. **The table row** in `outdoor_tilesets.h`, and a new `Planner` enumerator. The row order is the slot index in the outdoor hooks; add new rows at the end.
2. **The installer row** in `$OutdoorTilesets` in `tools/install.ps1`: the same tileset, slot, level name and LevelType, plus the level's `Size` (the largest plan), `OffsetX`/`OffsetY` (clear of every Act 5 level and of the other plugin levels) and the `Template` level it is copied from. A unit test (`tests/outdoor_tests.cpp`) reads this block and fails when it disagrees with the C++ table.
3. **The planner**: a function that fills a `MapPlan`, and its case in `planOutdoorMap` (`outdoor_plan.cpp`). Snow is the model for a stamps-only planner (`snow_plan.*`); Jungle for a layout plus a picker.
4. **The data the planner reads**: the piece table (LvlPrest ids, DS1 files, file counts, and whatever joins pieces: exit masks for Jungle, fence sockets for Snow). Jungle's catalogue is built into the plugin; the planner receives it as an argument.
5. **The independent plan check** (`checkSnowPlan` is the model: re-derive every cell, socket and rectangle without the planner).
6. **Tests**: the pins in `tests/outdoor_tests.cpp` (the table test) and `tests/tables_tests.cpp` (tileset names, ready list) name every row, so they change with the table. Add the planner's own tests.
7. **Docs**: the tileset in the plugin README and `docs/cartographer/tables.md`, a `CHANGELOG.md` line under `Unreleased`, and a reverse-engineering note with the evidence for any new engine fact.
8. **The in-game checks** for the new level (arrival in the entrance cell, the Boss and the event standing in their rooms). Unit tests do not cover these.

## What the table drives (no edit needed)

- The ready list and the tileset names the loader accepts (`TilesetNames`, `ReadyTilesetNames` in `core/tables.cpp`).
- `OutdoorLevelType`, `MapIsOutdoor`, `MapSlotName`, `IsOutdoorSlotName`, `OutdoorSlotLevelName` and `HasOutdoorSlot`; `ValidateSlots` takes the required level name from the row, and `OutdoorSlotRefusal` and `OutdoorSlotProblem` work on any outdoor slot.
- `outdoorTilesetKnown` and the "Use Jungle or Snow." message.
- In `engine/outdoor_hooks.cpp`, `SlotCount`, `SlotIndexFor`, `SlotNameOf` and the per-slot arrays (level, geometry, refusal).

## What a tileset gets for free

- `pickFarthestCell` turns the candidate cells and the anchors (the entrance, then the entrance and the boss) into the boss and event cells, the same for a seed. With no anchors it picks nothing and draws nothing from the rng.
- `StampPlan` and `stampPlanHash` carry the result to the hook; `cellName`, `lowerAscii` (`layout/ascii.h`) and the plan cache key give the checker its wording and the cache its keys.
- `chooseCellRoom` and `planArrival` reduce each cell to one built room: the built room nearest the cell centre, ties going to the smaller y and then the smaller x. The arrival, the Boss and the event all use it. This is the cell's top-left room only for Snow's 16-tile cells (2 x 2 rooms, all four tied). In a Jungle cell (32 x 32 tiles, 4 x 4 rooms of 8) the four centre rooms tie and the winner is the top-left of those four, at offset (+8,+8).
- The outdoor hook, `event_apply` and AutoCheck read the entrance, boss and event rectangles from the stamp plan without knowing the tileset.

## What the tileset must supply

The way it fills a grid of cells with pieces (a layout plus a picker, or lines of pieces); its size rules (the cell counts of Small, Medium and Large, the minimum and maximum, the slot's `maxTilesX/Y`); the entrance cell; and the list of candidate cells for the Boss and the event room (Snow: run cells, never caps) with a seed for the tie-break.
