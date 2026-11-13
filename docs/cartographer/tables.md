# Cartographer table reference

> Every column of every table, with its default and an example. New here? Start with [first-map.md](first-map.md).
> A working copy of each table is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).

All tables are tab-separated `.txt` files with a header row (save from a spreadsheet as "Text (Tab delimited)", or edit in
any text editor). They live in the mod's `<Mod>.mpq\data\global\excel\d2rloader\cartographer\` folder.

What the plugin reads, and what takes effect in game today:

| File | Read today? | Applied in game today? |
|---|---|---|
| `maps.txt` | Yes. Checked row by row when the game starts and again by `cartographer check`. | `Name`, `Item`, `MinCharLevel`, `Tileset`, `Shape`, `Size`, `Boss`, `Properties` and `EventChance` act; a Cave or Crypt map opens the installed Maze level, a Jungle or Snow map the Outdoor or OutdoorSnow level. `Monsters` is written into the map's level when it opens and the listed monsters filled the map in game; `Level`, `Density` and `Uniques` are written the same way (experimental, their effect not measured). `Champions` is checked, then never applied: the game has no setting for it, and a value above 0 is logged as not applied. |
| `cartographer.txt` | Yes, checked the same way. | All settings are used (see below). |
| `slots.txt` | Yes. Written by `tools\install.ps1` (the `Maze` row); you do not edit it. | Yes: it names the level the portal opens. |
| `shapes.txt`, `mapstats.txt`, `mapevents.txt` | Yes, each is optional. Checked row by row like `maps.txt`; a bad row is skipped and reported. | `mapstats.txt`: yes, when a map opens its properties are drawn and applied to the map's monsters, and to the player unless `PlayerStats` is 0 (monsters and the player checked in game, single player, one test character). `shapes.txt`: yes, a map's `Shape` can name a row and the layout is built from it exactly like a built-in shape (unit tested; not verified in game). `mapevents.txt`: yes, a map's event is drawn from it by `Weight` when the map opens (Boss, Horde, Ambush, Shrine and Treasure were seen in game; see testing.md). |
| `tilesets.txt` | Not yet. | No. A file you add now is ignored. |

Where a column below says what a map "gets" or "places", that is the plan the column is checked against, not yet what
happens in game.

Nothing in this plugin has been run in a game yet. This page is checked against the code and the unit tests only.

General rules for every table:

- Column names ignore case and can be in any order. A column the plugin does not know is ignored (`maps.txt`, `mapstats.txt` and `mapevents.txt` say so in the report).
- Blank lines are skipped. A leading byte order mark (what some editors add) is fine.
- Text is UTF-8, in game and in the installer. A file that is not valid UTF-8 is read as Latin-1 (what an editor's "ANSI" save gives) and converted, with one note in the log and `check`; characters outside Latin-1 need UTF-8. A Boss `Name` gives the same key and the same shown name either way.
- Each cell is trimmed of spaces at both ends.
- An empty optional file (`shapes.txt`, `mapstats.txt`, `mapevents.txt`, `cartographer.txt`) is the same as no file: `check` shows a note.
- A column named twice in the header of `maps.txt`, `mapstats.txt`, `mapevents.txt` or `shapes.txt` is an error, but the file still loads and only the first one is used. (`cartographer.txt` is not checked for this.)
- Words that are Cartographer's own (tileset, shape, size, target, kind) ignore case.
- The old `maps.txt` column `Difficulty` is no longer used. A table that still has it loads, and `cartographer check` notes that the column is ignored.
- Names from the game's own tables (monster `Id`, superunique name, stat names) must match exactly, capital letters included. Item codes ignore case. The check suggests the right spelling.
- A blank cell or a missing optional column takes the default.
- In `mapstats.txt` and `mapevents.txt`, a message about a `Weight`, `Count` or `Waves` past 2,147,483,647 (or below -2,147,483,648) adds "(you typed N)".
- A bad row is skipped and named in `cartographer-report.txt` (and by `cartographer check`); the other rows still load.

## maps.txt (required)

One row per map. The header row must have `Name`, `Tileset`, `Shape` and `Item`; without one of them the whole file is refused.

| Column | Required | What to put | Default | Example |
|---|---|---|---|---|
| `Name` | yes | The map's name, used by `cartographer open` and in the message "Cartographer: <Name> is open." when the portal opens. Unique across maps, ignoring case | none | Shorelines |
| `Tileset` | yes | A tileset name. `Cave` and `Crypt` are built (the Maze level is one of them: see the installer's `-Tileset`), and `Jungle` and `Snow` (always built; outdoor maps have no setting). `Lava`, `Barracks`, `Jail`, `Lair`, `Durance`, `Dungeon`, `Sewer`, `Ice`, `Harem`, `Basement`, `Spider` and `Temple` are known names that are refused with a message until they are built | none | Cave |
| `Shape` | yes | `Corridor`, `Winding`, `Branching`, `Loop`, `Sprawl` or `Open`, or the `Name` of a row in `shapes.txt` (matched ignoring case). | none | Corridor |
| `Size` | no | `ExtraSmall`, `Small`, `Medium` or `Large` | Medium | Large |
| `Level` | no | Monster level in the map, 1 to 255 (written into the map's level when it opens; experimental) | 85 | 90 |
| `Item` | yes | Item code that opens this map, and only this map. Up to 4 characters. Unique across maps. The item must be in your mod's `misc.txt` with item spell 16 (`pSpell`) and type `cmap` (see [Map item type](#map-item-type-itemtypestxt)): the plugin ships no items | none | `t20` |
| `Boss` | no | The `Name` of an entry in `bosses.txt` (ignoring case). A vanilla boss or monster, or a name not in `bosses.txt`, is refused with a message and the map opens without a Boss | no boss | `Shoreline Warden` |
| `Monsters` | no | Up to 10 monstats `Id`s, separated by commas (no empty entries; case must match; written into the map's level when it opens; the listed monsters filled the map in game) | the tileset's own monsters | `quillrat1,zombie1` |
| `MinCharLevel` | no | Character level needed, 1 to 99 | 1 | 80 |
| `Properties` | no | Random properties for this map, 0 to 20 (drawn from `mapstats.txt` when the map opens; checked in game) | the `cartographer.txt` value (6) | 4 |
| `EventChance` | no | Percent chance of an event, 0 to 100 | the `cartographer.txt` value (20) | 25 |
| `Density` | no | Percent more monsters than the tileset's normal, 0 and up (no cap; the result is cut at the game's 32-bit limit; scales the level's own density; experimental; adds nothing on a level whose density is 0) | 0 | 50 |
| `Champions` | no | Percent more champion packs, 0 and up (checked only; never applied, the game has no setting for it) | 0 | 50 |
| `Uniques` | no | Percent more unique packs the map may have, 0 and up (raises the level's own most-unique-packs limit, `MonUMax`, which stops at 255, the most the game stores; the level's minimum is left alone. How many appear is the minimum plus about 6 percent of the map's packs, so it depends on `Density` and the map size, and a `Uniques` above that adds nothing: about 73 on a Large Cave with `Density` 100, reached at about 800 percent; experimental; adds nothing on a level whose `MonUMax` is 0) | 0 | 25 |

Smallest map: `Name`, `Tileset`, `Shape` and `Item` filled in.

A row is refused with a message, and skipped, when:

- `Name` is empty or already used;
- `Tileset` is empty, unknown or not built yet;
- `Shape` or `Size` is not one of the words above;
- `Shape` is `Open` (or a `shapes.txt` shape based on it) and `Tileset` is `Snow`: Snow is lines of fence pieces, with no rooms to join, and takes no Shape;
- a number is not a whole number in its range;
- `Item` is empty, longer than 4 characters, not an item in `misc.txt`, `weapons.txt` or `armor.txt`, an item without item spell 16, or an item another map already uses;
- a `Monsters` entry is not in the game's tables, or `Monsters` has more than 10 entries.

A `Boss` that is not a `bosses.txt` entry is also an error, but only the Boss is dropped: the map still loads.

Each name check is skipped only when the game table it needs cannot be read, and the report says so: `Monsters` needs `monstats` (a `Boss` is checked against `bosses.txt`, and against `monstats` and `superuniques` to name a vanilla boss), and `Item` needs the item tables. The spell-16 check needs `misc.txt` with a `pSpell` column; without it the check is skipped, and an item without spell 16 simply never opens its map.

A map opens only in Hell, in Harrogath, for a character of at least `MinCharLevel`, once per game. This is what acts in game today. A refused map stays in the inventory and a sentence says why. Planned, not built yet: a maze map has an entrance and no exit, the player arrives through the portal, and the boss and the event room are at the far end of the path.

## Map item type (itemtypes.txt)

Not a Cartographer table: one row `tools\install.ps1` appends to the mod's `itemtypes.txt` (never changing an existing row), and a modder without the installer adds by hand. Every map item in `misc.txt` has `type` `cmap` and `belt` 0.

| Column | Value | Why |
|---|---|---|
| `ItemType`, `Code` | `Map`, `cmap` | The game names an item of type `scro` or `book` after its item spell (a spell-16 item would read "Scroll of Town Portal"), so a map item needs a type of its own. A row with code `cmap` that is not this one stops the installer. |
| `Equiv1` | `misc` | The parent of `herb`, `scro`, `key` and `elix`: plain miscellaneous. `scro`, `book` or a potion type would make it a scroll (a Town Portal scroll) or a potion (belt). |
| `Equiv2`, `Magic`, `Rare`, `Shoots`, `Quiver`, `BodyLoc1`, `BodyLoc2` | empty | Not wearable, not ammunition, no magic or rare quality. |
| `Repair`, `Body`, `Throwable`, `Reload`, `ReEquip`, `AutoStack`, `Beltable` | 0 | Not throwable, not beltable, no auto-stack. |
| `Normal` | 0 | 1 would mean normal quality only (as on `scro`, `book` and `key`); the item never spawns, so it is harmless, and 0 is what `herb` has. |
| `StorePage` | `misc` (as `herb`) | Only names the vendor tab the item shows on, for example when a player sells it to an NPC. Vendors do not stock the item because the `misc.txt` vendor columns are blank, not because of this column. |
| everything else | as `herb` | `herb` is a vanilla non-quest miscellaneous type. |

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
- `Min` and `Max` need a column and a number on every row; a blank cell is an error and the row is skipped.
- `Min` and `Max` beyond 500,000,000 (or below -500,000,000), even past 2,147,483,647, are cut to that, and `check` warns.
- `Stat` must be a `Stat` name in the game's `itemstatcost.txt` (exact spelling). `check` reports a name it does not find, with a suggestion. A row with a wrong name is still in the pool; if it is drawn it is skipped and the plugin log says so. A value that rolls 0 is skipped, as it changes nothing.
- When a map opens, `Monsters` rows are applied to every monster that spawns in the map level, and `Players` rows to the player while the player is in the map level. Neither has been seen working in a game yet; the plugin log (`statprobe:` lines) records what happened.
- If `Max` is below `Min`, `Min` is rolled every time, and `check` warns.
- Two rows with the same `Target` and `Stat` are one property: drawing one removes the other for that map.
- If a map asks for more properties than there are distinct ones, all of them are drawn. No error.
- `Properties` in `maps.txt` and in `cartographer.txt` is 0 to 20. A negative value is refused (the map row is skipped; the setting keeps its default).
- With `ItemStats` on, the item's own stats are used as they are, in order, and `Properties` is ignored. Each item stat is matched to the first `mapstats.txt` row with the same `Stat`, which says whether it goes on monsters or players; a stat with no such row (or a layer, or a value of 0) is left out and the log says so. If the item has no usable stats, a normal draw is made instead (seen in game: a player stat from the item went onto the character; monster stats from an item were not checked). At most the first 16 stats are read.
- `Players` rows are applied unless `PlayerStats` is `0` in `cartographer.txt` (default `1`; `Monsters` rows are applied either way). The player's stats sit on the character while it is in the map and are removed on leaving it. In the Hell check (single player, one test character, 2026-11-12) a Save and Exit inside the map and a reload left the character as before the map. An autosave inside the map, multiplayer, death and curses were not checked.
- Some stats cannot be used. The game reads them as a total and writes the result back as the saved value, so the map's value would stay in the character: `statpts`, `newskills`, `gold`, `goldbank`, `hitpoints`, `mana`, `stamina`, `level` and `experience`. Such a `Players` row is skipped when drawn, and `cartographer check` and the report name it. `level` is also skipped on `Monsters`, because the game sets the monster's level itself, and so are `hitpoints`, `mana` and `stamina`, because the game rewrites them on every hit and regeneration (use `maxhp`, `maxmana`, `maxstamina`). Stats that need a skill, state or monster type number (`item_addclassskills`, `item_nonclassskill`, `state`, `item_singleskill`, `item_elemskill`, `item_aura`, `item_reanimate`, `attack_vs_montype`, `damage_vs_montype`, `item_addskill_tab`, `item_skillonattack`, `item_skillonkill`, `item_skillondeath`, `item_skillonhit`, `item_skillonlevelup`, `item_skillongethit` and `item_charged_skill`; the stats with a Save Param Bits value in the vanilla 3.3.0 `itemstatcost.txt`) are skipped on both, because a map row cannot name the skill, state or monster type yet. The maxima (`maxhp`, `maxmana`, `maxstamina`) and the attributes are fine.

## bosses.txt (optional)

The bosses a map's `Boss` can name. A boss is always an entry of its own here, never a vanilla one. See the README for the columns (`Name`, `Base`, `Mods`, `Minions`) and how `tools\install.ps1` writes a `superuniques.txt` row per entry, keyed `cartoboss:` plus the escaped Name.

- `Name` cannot be the name of a vanilla superunique or monster (any case) and cannot repeat; such a row is skipped and reported.
- `Base` must be a monstats `Id` whose `boss` column is not 1 (a vanilla boss such as `andariel` is refused); `Mods` has up to 3 ids that exist in `monumod.txt`; `Minions` is 0 to 10. At most 16 entries are used; the rest are reported and skipped.
- `tools\install.ps1` runs the same checks, writes only the entries that pass, and rebuilds its `cartoboss:` rows each run (a renamed or removed entry loses its old row).
- A `Boss` in `maps.txt` that names a vanilla boss or monster, a `cartoboss:` key, or is not an entry here, is an error in the report; the map still loads and opens without a Boss.
- If the game has no `superuniques.txt` row for the entry, or its spawn gives no unit, the boss is spawned as a plain monster of its `Base` and the log says so.

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
| `Treasure` | A treasure monster is spawned and drops from `Drop` when killed (it does not flee). If `Spawn` is an object, a chest that drops from `Drop` is placed; it is always unlocked (its loot is not confirmed in game) |
| `Shrine` | An object (shrine, well or chest) is placed in the event room |

## cartographer.txt (optional)

Global settings, read when the game starts; `Properties` and `PlayerStats` are checked in game (single player). Two columns, `Setting` and `Value`; one setting per row. Setting names ignore case. A blank `Value` keeps the default. An unknown setting, or a value outside its range, is reported and the default is kept. Without the file, every default applies.

| Setting | Meaning | Default | Example |
|---|---|---|---|
| `Properties` | Random properties per map, 0 to 20 | 6 | 4 |
| `EventChance` | Percent chance of an event per map, 0 to 100 | 20 | 30 |
| `ItemStats` | `1` means a map item's own stats are used in place of a draw; `0` or `1` | 0 | 1 |
| `PlayerStats` | `1` puts the `Players` rows of a map on the player while in the map (see above for what was checked); `0` turns it off; `0` or `1` | 1 | 0 |
| `MapNames` | `0` turns off a map's own name on its portal tooltip, the automap title and the entry banner (experimental; seen in single player, multiplayer unverified); `0` or `1` | 1 | 1 |
| `MazeProbe` | `1` logs what the game builds for a maze, for debugging; `0` or `1` | 0 | 0 |
| `Town` | The town the portal opens from. Only `Harrogath` works (maps are in Act 5, and a portal only reaches its own act) | Harrogath | Harrogath |
| `OutdoorProbe` | `1` logs what the game builds for an outdoor map, for debugging; `0` or `1` | 0 | 0 |
| `AutoCheck` | `1` runs a game check by itself in a Hell game, for test slots only; `0` or `1` | 0 | 0 |
| `AutoCheckMap` | The `Name` of the map `AutoCheck` opens, for test slots only | none | Shorelines |

## tilesets.txt (optional, advanced)

Adds your own room-piece set to a built-in tileset. Only for maze tilesets. Your rooms are used only by Cartographer maps. Not read by the plugin yet (see the table at the top).

| Column | What to put | Default | Example |
|---|---|---|---|
| `Name` | The name to use in `maps.txt` | none (required) | Mossy Cave |
| `Base` | The built-in tileset it copies | none (required) | Cave |
| `First` | The `Def` value of the "W" piece in the game's room piece table (`LvlPrest`); the other 14 follow it | none (required) | 1200 (illustrative) |

## shapes.txt (optional, advanced)

Adds a new shape defined with numbers. A row based on `Open` keeps the open room grid: `Width`, `Height`, `BossRoom`, `MinPath`/`MaxPath` and `MinTurns`/`MaxTurns` (checked against the shortest path from the entrance to the boss room) still apply, as does the event position; `Straight` and the side room and loop columns do nothing, and a row that asks for side rooms (`MaxBranches`), extra loops (`MaxLoops`) or a `Loop` ring is refused. Use its name in the `Shape` column of `maps.txt` (ignoring case). Built-in shape names cannot be reused. Column names ignore case; an unknown or repeated column is an error. Cells are whole numbers, except `Loop` and `BossRoom`, which take `1` or `0` (or `yes` or `no`).

Every column except `Name` is optional. A blank column keeps the value of the base shape (`Base`) at the chosen `Size`. A row that can't be built is skipped with an error that names the line and column. The generator gives each layout a fixed amount of work (about 25 ms at most on a desktop), so a row that is too tight to build is reported as too tight right away instead of making the game wait. A row parses with at most 48 tries of that size, about a second at the very worst.

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
| `Width`, `Height` | Grid size in cells, 2 to 32 each. When blank, the caps above are checked against the grids of Small, Medium and Large (6, 9 and 12) when the file loads, so a row has to fit the smallest. At `ExtraSmall` (a 4 x 4 grid) it is checked when a `maps.txt` row uses the shape: a row that does not fit is refused with the reason | the `Size`'s grid | 9, 9 |
| `BossRoom` | `1` to place a boss room, `0` for none | the base shape's | 1 |
