# Adding an outdoor tileset to Cartographer

For developers. Jungle and Snow share the outdoor plumbing in `plugins/cartographer/src/outdoor/` (`outdoor_plan.*`, `stamp.*`, `outdoor.h`). A new tileset (Barbarian Highlands style fields, a third Act 5 look) is one row in a table plus its planner and data. The evidence for the tilesets that exist is in [cartographer-snow.md](../reverse-engineering/cartographer-snow.md), [cartographer-siege.md](../reverse-engineering/cartographer-siege.md) and the outdoor builder notes there. Siege (`siege_plan.*`) is the model for a plan that is one line of strips with seam sockets, a slot level that is not square (the table's and the installer's `SizeY`) and macro cells that are not square (`compareGrid`).

## Every touch point

The table is `OutdoorTilesets` in `src/outdoor/outdoor_tilesets.h`. A row is `{name, levelType, slot, levelName, planner}`: the `maps.txt` Tileset, the `levels.txt` LevelType its slot level must have, the `slots.txt` row name, the `levels.txt` Name the installer gives the level, and a `Planner` value.

1. **The table row** in `outdoor_tilesets.h`, and a new `Planner` enumerator. The row order is the slot index in the outdoor hooks; add new rows at the end.
2. **The installer row** in `$OutdoorTilesets` in `tools/install.ps1`: the same tileset, slot, level name and LevelType, plus the level's `Size` (the largest plan), `Param` (the prefix of its offset parameters, below) and the `Template` level it is copied from. A unit test (`tests/outdoor_tests.cpp`) reads this block and fails when it disagrees with the C++ table.
   - **A new offset parameter pair**: `-<Param>OffsetX` and `-<Param>OffsetY` in the script `param(...)` block, with defaults clear of every Act 5 level and of the other plugin levels; the loop reads them by name, and the level is skipped (`$null`) when its rectangle overlaps another level.
   - **The `install.ps1` help text**: the `.DESCRIPTION` paragraph for the level and the `.PARAMETER` text that lists the offset pairs.
   - **The README manual-install table**: one row per level (name, template level, DrlgType, LevelType, size, offset), and the README step that lists the `Maze`, `Outdoor` and `OutdoorSnow` slot rows (add the new slot row there).
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

The way it fills a grid of cells with pieces (a layout plus a picker, or lines of pieces); its size rules (the cell counts of Small, Medium, Large and ExtraLarge, the minimum and maximum, the slot's `maxTilesX/Y`); the entrance cell; and the list of candidate cells for the Boss and the event room (Snow: run cells, never caps) with a seed for the tie-break.

## Adding a maze tileset

Cave, Crypt, Lava and Ice are rows of `MazeTilesets` in `src/maze/maze_tilesets.h`. A level's `LevelType` fixes its tileset, so each maze tileset needs a slot level of its own, as the outdoor tilesets have. A row is `{name, levelType, slot, levelName, roomTiles, pieceBase, prevPiece, callsRingGrower}`: the `maps.txt` Tileset, the `levels.txt` LevelType, the `slots.txt` row name, the `levels.txt` Name the installer gives the level, the room size in tiles, the first LvlPrest id of the tileset's piece set, the four preset pieces of the entrance family, and whether the builder case calls the ring grower. The maze hooks, the loader's ready list, `MapSlotName`, `ValidateSlots` and the planner's `builtinTileset` all read the row, so they need no edit.

1. **The table row** in `maze_tilesets.h`; new rows go at the end (the row order is the slot index in the maze hooks). Keep an existing slot name and level name as they are, so installs already made do not change.
2. **The installer row** in `$MazeTilesets` in `tools/install.ps1`: the same tileset, slot, level name and LevelType, plus `Size` and `Param`, and the `-<Param>OffsetX`/`-<Param>OffsetY` parameters with defaults that follow the offset rule below (the installer refuses an offset outside the automap range). `tests/maze_tests.cpp` reads that block and fails when it differs from the table.
3. **Tests and docs**: the table pin in `tests/maze_tests.cpp`, the slot names in `tests/tables_tests.cpp`, the README manual-install table and `slots.txt` rows, `docs/cartographer/tables.md`, a `CHANGELOG.md` line, and the evidence for any new engine fact in `docs/reverse-engineering/`.
4. **The in-game check**: arrival in the entrance room, the Boss and the event in their rooms, and the other slots untouched. Unit tests do not cover these.

**The offset rule.** The engine saves a level's automap cells as signed 16-bit numbers when the player leaves it (cell x = (tx - ty) * 8, y = (tx + ty) * 4), so a maze level with any corner out of range loses its revealed map after a trip to town (#306). Give each maze row y 0 and the next free x in the band 0..3735 (Cave 3500, Crypt 3000, Lava 2500, Ice 2000; Snow, 192 wide, has 1500..1692), 500 apart, which keeps the whole 360-tile level in range (ox - oy within -3736..3735, ox + oy at most 7471), overlaps no other row and stays above the first Act 5 level (y 1000); the next row takes x 1000 or less, clear of Snow. The row's `offsetX`/`offsetY` must equal the installer defaults, `automapInRange` (`maze_tilesets.h`) and `Test-AutomapRange` (`install.ps1`) hold the arithmetic, and `tests/maze_tests.cpp` checks range, overlap and the installer agreement for every row. Do not use the outdoor offsets (x 6000) for a maze level.

The one-map-per-game rule still holds across all slots (`g_job`, `g_slotState` and `MazeSlotBuilt` are shared).
