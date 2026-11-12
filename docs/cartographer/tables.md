# Cartographer table reference

> **Draft.** Features land in later phases. See [README.md](README.md) for a walk-through.

All tables are tab-separated `.txt` files with a header row. They live in the mod's `data/global/excel/d2rloader/cartographer/` folder.

- Words that are Cartographer's own (tileset, shape, size, target, kind) ignore case.
- Names from the game's own tables (monster `Id`, item codes, stat names) must match exactly. The check suggests the right spelling.
- A blank cell or a missing optional column takes the default.

## maps.txt (required)

One row per map.

| Column | Required | What to put | Default | Example |
|---|---|---|---|---|
| `Name` | yes | The map's name, shown on entry | none | Shorelines |
| `Tileset` | yes | A tileset name (see README) | none | Jungle |
| `Shape` | yes | `Corridor`, `Winding`, `Branching`, `Loop`, `Sprawl`, or a name from `shapes.txt` | none | Corridor |
| `Size` | no | `Small`, `Medium` or `Large` | Medium | Large |
| `Level` | no | Monster level in the map | 85 | 90 |
| `Item` | no | Item code of the map item | Cartographer's own map item | `shm1` |
| `Boss` | no | A monster `Id` or unique monster name placed at the end of the path (case must match) | no boss | `fallenshaman1` |
| `Monsters` | no | Up to 10 monster `Id`s, separated by commas | the tileset's own monsters | `quillrat1,zombie3` |
| `Difficulty` | no | `Normal`, `Nightmare` or `Hell` | Hell | Nightmare |
| `MinCharLevel` | no | Character level needed | 1 | 80 |
| `Properties` | no | Random properties for this map | the `cartographer.txt` value (6) | 4 |
| `EventChance` | no | Percent chance of an event | the `cartographer.txt` value (20) | 25 |
| `Density` | no | Percent more monsters than the tileset's normal | 0 | 50 |
| `Champions` | no | Percent more champion packs | 0 | 50 |
| `Uniques` | no | Percent more unique packs | 0 | 25 |

Smallest map: `Name`, `Tileset` and `Shape` filled in.

## mapstats.txt (optional)

One shared pool of random properties. Each map draws `Properties` of them. A property is never drawn twice in one map. If the file is missing, a small built-in pool is used.

There is no text column. The text comes from the game's own stat description.

| Column | What to put | Default | Example |
|---|---|---|---|
| `Target` | `Monsters` or `Players` | none (required) | Monsters |
| `Stat` | A stat name from the game's stat table | none (required) | `item_maxhp_percent` |
| `Min` | Lowest rolled value (limit -500,000,000 to 500,000,000) | none (required) | 10 |
| `Max` | Highest rolled value (limit -500,000,000 to 500,000,000) | none (required) | 20 |
| `Weight` | How often it is drawn compared with the others. 0 switches the row off. Limit 1,000,000 | 1 | 3 |

Rules and limits:

- `Weight` 0 means the row is off: never drawn, no message. A negative `Weight` is an error: the row is skipped and `cartographer check` says why. A `Weight` above 1,000,000 is used as 1,000,000 and `check` warns. Only the proportions matter, so `5000` and `2000` work as written.
- `Min` and `Max` beyond 500,000,000 (or below -500,000,000) are cut to that, and `check` warns.
- If `Max` is below `Min`, `Min` is rolled every time, and `check` warns.
- Two rows with the same `Target` and `Stat` are one property: drawing one removes the other for that map.
- If a map asks for more properties than there are distinct ones, all of them are drawn. No error.
- A negative `Properties` in `maps.txt` counts as empty (the setting is used); a negative `Properties` in `cartographer.txt` counts as 0.
- With `ItemStats` on, the item's own stats are used as they are, in order, and `Properties` is ignored. If the item has no stats, a normal draw is made instead.

## mapevents.txt (optional)

What events exist. A map has `EventChance` percent chance of one event. One row is drawn by `Weight`. Without this file there are no events.

| Column | What to put | Default | Example |
|---|---|---|---|
| `Name` | Shown to the player when the event starts | none (required) | The Drowned King |
| `Kind` | `Boss`, `Ambush`, `Horde`, `Treasure` or `Shrine` | none (required) | Boss |
| `Spawn` | A monster `Id`, a unique monster name or an object name | none (required) | `Bishibosh` |
| `Count` | Minions (Boss), wave size (Ambush) or horde size (Horde). 1 to 200 | 1 | 6 |
| `Waves` | Number of waves (Ambush only; ignored on other kinds). 1 to 50 | 3 | 4 |
| `Drop` | A treasure class name. For Treasure, the drop. For Boss, extra loot | none | Act 5 (H) Super C |
| `Message` | Extra text shown when the event starts | none | The water churns... |
| `Weight` | How often it is drawn compared with the others. 0 switches the row off. Limit 1,000,000 | 1 | 2 |

Rules and limits:

- `Weight` 0 means the row is off: never drawn, no message. A negative `Weight` is an error: the row is skipped and `cartographer check` says why. A `Weight` above 1,000,000 is used as 1,000,000 and `check` warns. Only the proportions matter, so `5000` and `2000` work as written.
- `Count` must be 1 to 200 and `Waves` 1 to 50; a value outside that is an error and the row is skipped. `Waves` is only looked at on `Ambush` rows.
- A `Kind` that is not one of the five is an error, and `check` suggests the closest one ("Did you mean Boss?").
- A `Treasure` with no `Drop` still works but drops nothing special, so `check` warns.
- Errors name the line of the row in the file.

The kinds:

| Kind | What happens |
|---|---|
| `Boss` | A boss waits in the event room with `Count` minions |
| `Ambush` | When a player reaches the event room, `Waves` waves of `Count` of `Spawn` attack |
| `Horde` | The event room is packed with `Count` of `Spawn` |
| `Treasure` | A treasure monster flees and drops from `Drop` when killed. If `Spawn` is an object, a chest that drops from `Drop` is placed |
| `Shrine` | An object (shrine, well or chest) is placed in the event room |

## cartographer.txt (optional)

Global settings. Two columns, `Setting` and `Value`.

| Setting | Meaning | Default | Example |
|---|---|---|---|
| `Properties` | Random properties per map | 6 | 4 |
| `EventChance` | Percent chance of an event per map | 20 | 30 |
| `ItemStats` | 1 means a map item's own stats are used in place of a draw | 0 | 1 |
| `Town` | The town the portal opens from | Harrogath | Harrogath |

## tilesets.txt (optional, advanced)

Adds your own room-piece set to a built-in tileset. Only for maze tilesets. Your rooms are used only by Cartographer maps.

| Column | What to put | Default | Example |
|---|---|---|---|
| `Name` | The name to use in `maps.txt` | none (required) | Mossy Cave |
| `Base` | The built-in tileset it copies | none (required) | Cave |
| `First` | The `Def` value of the "W" piece in the game's room piece table (`LvlPrest`); the other 14 follow it | none (required) | 1200 (illustrative) |

## shapes.txt (optional, advanced)

Adds a new shape defined with numbers. Use its name in the `Shape` column.

Every column except `Name` is optional. A blank column keeps the value of the base shape (`Base`) at the chosen `Size`. A row that can't be built is skipped with an error that names the line and column. The generator gives each layout a fixed amount of work (about 20 ms at most on a desktop), so a row that is too tight to build is reported as too tight right away instead of making the game wait. A row parses with at most 48 tries of that size, about a second at the very worst.

| Column | What to put | Default | Example |
|---|---|---|---|
| `Name` | The name to use in `maps.txt` | none (required) | Snake |
| `Base` | A built-in shape, or the `Name` of an earlier row, to start from | `Winding` | Branching |
| `MinPath`, `MaxPath` | Cells in the main path, entrance and boss room included. At least 4; a `Loop` needs an even number in range and at most 200 (and at most half of `Width` x `Height`) | the base shape's | 12, 16 |
| `Straight` | Percent chance to keep going straight at each step (0 to 100) | the base shape's | 30 |
| `MinTurns`, `MaxTurns` | Direction changes along the main path | the base shape's | 8, 99 |
| `MinBranches`, `MaxBranches` | Dead-end side rooms (at most half of `Width` x `Height`) | the base shape's | 2, 4 |
| `MinBranchLen`, `MaxBranchLen` | Cells in one side room (at least 1; `MaxBranchLen` at most half of `Width` x `Height`). `MinBranches` x `MinBranchLen`, plus 2 cells per `MinLoops`, must fit in the grid beside the main path | the base shape's | 1, 3 |
| `Loop` | `1` for a ring that returns to the entrance, `0` for a path | the base shape's | 1 |
| `MinLoops`, `MaxLoops` | Extra loops that rejoin the layout (at most half of `Width` x `Height`) | the base shape's | 0, 2 |
| `Width`, `Height` | Grid size in cells, 2 to 32 each. When blank, the caps above are checked against every `Size`'s grid (6, 9 and 12), so a row has to fit the smallest | the `Size`'s grid | 9, 9 |
| `BossRoom` | `1` to place a boss room, `0` for none | the base shape's | 1 |
