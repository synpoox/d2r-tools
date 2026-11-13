# Cartographer table reference

> Every column of every table, with its default and an example. New here? Start with [first-map.md](first-map.md).
> A working copy of each table is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).

All tables are tab-separated `.txt` files with a header row (save from a spreadsheet as "Text (Tab delimited)", or edit in
any text editor). They live in the mod's `<Mod>.mpq\data\global\excel\d2rloader\cartographer\` folder.

What the plugin reads, and what takes effect in game today:

| File | Read today? | Applied in game today? |
|---|---|---|
| `maps.txt` | Yes. Checked row by row when the game starts and again by `cartographer check`. | `Name`, `Item`, `MinCharLevel`, `Tileset`, `Shape`, `Size`, `Boss` and `EventChance` act; a Cave, Crypt or Ice map opens the installed Maze level of its tileset, a Jungle, Snow or Siege map the Outdoor, OutdoorSnow or OutdoorSiege level. `Monsters` is written into the map's level when it opens and the listed monsters filled the map in game; `Level`, `Density` and `Uniques` are written the same way (experimental, their effect not measured). `Champions` is checked, then never applied: the game has no setting for it, and a value above 0 is logged as not applied. |
| `cartographer.txt` | Yes, checked the same way. | All settings are used (see below). |
| `slots.txt` | Yes. Written by `tools\install.ps1` (the `Maze`, `MazeCrypt`, `MazeIce`, `Outdoor` and `OutdoorSnow` rows); you do not edit it. | Yes: it names the level the portal opens. |
| `shapes.txt`, `mapevents.txt` | Yes, each is optional. Checked row by row like `maps.txt`; a bad row is skipped and reported. | `shapes.txt`: yes, a map's `Shape` can name a row and the layout is built from it exactly like a built-in shape (unit tested; not verified in game). `mapevents.txt`: yes, a map's event is drawn from it by `Weight` when the map opens (Boss, Horde, Ambush, Shrine and Treasure were seen in game; see testing.md). |
| `tilesets.txt` | Not yet. | No. A file you add now is ignored. |

Where a column below says what a map "gets" or "places", that is the plan the column is checked against, not yet what
happens in game.

Nothing in this plugin has been run in a game yet. This page is checked against the code and the unit tests only.

General rules for every table:

- Column names ignore case and can be in any order. A column the plugin does not know is ignored (`maps.txt` and `mapevents.txt` say so in the report).
- Blank lines are skipped. A leading byte order mark (what some editors add) is fine.
- Text is UTF-8, in game and in the installer. A file that is not valid UTF-8 is read as Latin-1 (what an editor's "ANSI" save gives) and converted, with one note in the log and `check`; characters outside Latin-1 need UTF-8. A Boss `Name` gives the same key and the same shown name either way.
- Each cell is trimmed of spaces at both ends.
- An empty optional file (`shapes.txt`, `mapevents.txt`, `cartographer.txt`) is the same as no file: `check` shows a note.
- A column named twice in the header of `maps.txt`, `mapevents.txt` or `shapes.txt` is an error, but the file still loads and only the first one is used. (`cartographer.txt` is not checked for this.)
- Words that are Cartographer's own (tileset, shape, size, target, kind) ignore case.
- The old `maps.txt` column `Difficulty` is no longer used. A table that still has it loads, and `cartographer check` notes that the column is ignored.
- Names from the game's own tables (monster `Id`, superunique name, stat names) must match exactly, capital letters included. Item codes ignore case. The check suggests the right spelling.
- A blank cell or a missing optional column takes the default.
- In `mapevents.txt`, a message about a `Weight`, `Count` or `Waves` past 2,147,483,647 (or below -2,147,483,648) adds "(you typed N)".
- A bad row is skipped and named in `cartographer-report.txt` (and by `cartographer check`); the other rows still load.

## maps.txt (required)

One row per map. The header row must have `Name`, `Tileset`, `Shape` and `Item`; without one of them the whole file is refused.

| Column | Required | What to put | Default | Example |
|---|---|---|---|---|
| `Name` | yes | The map's name, used by `cartographer open` and in the message "Cartographer: <Name> is open." when the portal opens. Unique across maps, ignoring case | none | Shorelines |
| `Tileset` | yes | A tileset name. `Cave`, `Crypt`, `Lava` (River of Flame) and `Ice` (Ice caves) are built (each opens through its own level, `Maze`, `MazeCrypt`, `MazeLava` and `MazeIce`), and `Jungle`, `Snow` and `Siege` (always built; outdoor maps have no setting). `Barracks`, `Jail`, `Lair`, `Durance`, `Dungeon`, `Sewer`, `Harem`, `Basement`, `Spider` and `Temple` are known names that are refused with a message until they are built | none | Cave |
| `Shape` | yes | `Corridor`, `Winding`, `Branching`, `Loop`, `Sprawl` or `Open`, or the `Name` of a row in `shapes.txt` (matched ignoring case). | none | Corridor |
| `Size` | no | `ExtraSmall`, `Small`, `Medium` or `Large` | Medium | Large |
| `Level` | no | Monster level in the map, 1 to 255 (written into the map's level when it opens; experimental) | 85 | 90 |
| `Item` | yes | Item code that opens this map, and only this map. Up to 4 characters. Unique across maps. The item must be in your mod's `misc.txt` with item spell 16 (`pSpell`) and type `cmap` (see [Map item type](#map-item-type-itemtypestxt)): the plugin ships no items | none | `t20` |
| `Boss` | no | The `Name` of an entry in `bosses.txt` (ignoring case). A vanilla boss or monster, or a name not in `bosses.txt`, is refused with a message and the map opens without a Boss | no boss | `Shoreline Warden` |
| `Monsters` | no | Up to 10 monstats `Id`s, separated by commas (no empty entries; case must match; written into the map's level when it opens; the listed monsters filled the map in game) | the tileset's own monsters | `quillrat1,zombie1` |
| `MinCharLevel` | no | Character level needed, 1 to 99 | 1 | 80 |
| `EventChance` | no | Percent chance of an event, 0 to 100 | the `cartographer.txt` value (20) | 25 |
| `Density` | no | Percent more monsters than the tileset's normal, 0 and up (no cap; the result is cut at the game's 32-bit limit; scales the level's own density; experimental; adds nothing on a level whose density is 0) | 0 | 50 |
| `Champions` | no | Percent more champion packs, 0 and up (checked only; never applied, the game has no setting for it) | 0 | 50 |
| `Uniques` | no | Percent more unique packs the map may have, 0 and up. Raises the level's `MonUMax` (stops at 255); `MonUMin` is left alone. Past the minimum the game makes about 6 percent of packs unique, so the count depends on `Density` and map size and a larger `Uniques` adds nothing once that is reached (about 73 on a Large Cave at `Density` 100, near 800 percent). Experimental; no effect on a level whose `MonUMax` is 0 | 0 | 25 |

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
| `Equiv2`, `Shoots`, `Quiver`, `BodyLoc1`, `BodyLoc2` | empty | Not wearable, not ammunition. |
| `Rare`, `Magic`, `Normal` | `1`, `0`, `0` | Rare on, Magic and Normal off. A plugin hook turns a magic roll into normal; forced and existing magic maps stay magic (see Map item rarity and modifiers). |
| `Repair`, `Body`, `Throwable`, `Reload`, `ReEquip`, `AutoStack`, `Beltable` | 0 | Not throwable, not beltable, no auto-stack. |
| `StorePage` | `misc` (as `herb`) | Only names the vendor tab the item shows on, for example when a player sells it to an NPC. Vendors do not stock the item because the `misc.txt` vendor columns are blank, not because of this column. |
| everything else | as `herb` | `herb` is a vanilla non-quest miscellaneous type. |

## Map level on the item (itemstatcost.txt, properties.txt, cubemain.txt)

A map item can carry its own map level as a stat, so a cube recipe can change it and the item shows it on hover. `tools\install.ps1` adds two rows (never changing an existing one); a modder without the installer adds them by hand.

| File | Row | Notes |
|---|---|---|
| `itemstatcost.txt` | `cartographer_map_level` | A copy of `item_maxhp_percent` with 9 save bits (0 to 511), unsigned, no op, `descfunc` 19, `descstrpos` and `descstrneg` `cartographer:map_level_stat` (the plugin registers the text `Map Level: %d`). The stat id is the row number, so the row goes last. |
| `properties.txt` | `map-level` | A copy of `str` with `stat1` `cartographer_map_level`, so `cubemain.txt` can write `map-level` in a `mod` column. |

- **Fallback:** a map item without the stat, with a value under 1 or with a layer uses the `maps.txt` Level. A value over 255 is cut to 255. The log line `map level N (...)` says which one applied.
- **Where it is read:** the item-use hook, from the item's stat list (the stat is taken out before the rest are used as modifiers). `cartographer open` has no item and uses the `maps.txt` Level.
- **Rarity and modifiers** are in the next section; this stat is only the level.

Example recipe, one `cubemain.txt` row (columns not named are empty):

| Column | Value |
|---|---|
| `description` | `Map + Town Portal scroll: +10 map level` |
| `enabled`, `version` | `1`, `100` |
| `numinputs` | `2` |
| `input 1`, `input 2` | the map's `misc.txt` code (for example `t20`), `tsc` |
| `output` | `usetype,nor,mod` |
| `mod 1`, `mod 1 chance`, `mod 1 min`, `mod 1 max` | `map-level`, `100`, `10`, `10` |

**On a map item.** The plugin sets the stat when a recipe's output keeps the map: use `usetype,nor,mod` (or `t22,mod`, a fixed map) rather than `useitem,mod`, which rebuilds a misc item as a Hand Axe. The level is added to the map's own stat, so transmuting the same map again adds again (10, then 20). The result is cut to 1 to 511, but a map opens at level 255 at most. A recipe adds to the map's level; it cannot set an exact level on a map that already has one. With a range (`mod 1 min` below `mod 1 max`), a value the game already rolled inside the range stands, so such a recipe can add less than a fixed one, never twice. Without a readable level on the map the game's own result stands. Checked in game (Hell, test save, the final build of the cube change, `usetype,nor,mod` only): a fresh map went 0 to 10, a second transmute 10 to 20, and the map opened at level 20. The hover of a map item shows no `Map Level` line. Without the plugin hook (older versions, or a part that did not match) the cube drops the `mod` on a map, and the `maps.txt` Level is used.

**Keep the stat rows where they are.** Once saved items carry `cartographer_map_level` or any of the seven `cartographer_mon_*` stats (`life`, `damage`, `speed`, `fire_res`, `cold_res`, `light_res`, `poison_res`) or `cartographer_map_density`, their rows in `itemstatcost.txt` must stay at the same position: restoring the `.bak`, or replacing the file with one that has more rows and running the installer again (it appends at a new id), makes saved items read the wrong stat or fail to load. The same goes for the stat's bit widths: items already saved with a stat are read with the widths it has now, so do not change them once items exist. The installer widens one row only, `cartographer_map_density` written with the early 11 / 10 / 512 widths (from a development build) to the 20-bit widths above; it touches no other row.

## Map item rarity and modifiers

A map item has the game's own quality. The installer sets the `Map` item type (`cmap`) to Rare on, Magic and Normal off, so the game rolls normal or rare quality for a map and gives a rare one affixes. Maps are meant to be normal, rare or unique, and a plugin hook makes a map the game rolls as magic normal, including the fallback after a failed set, rare or unique roll; a map forced magic by a cheat (`typ m`) or already existing stays magic, and the hook applies to any item with the map spell 16 (`Magic` 1 would mean always magic, so no column can do it; an existing `cmap` row is repaired to this; it is the one existing row the installer changes); the map's `misc.txt` row needs `compactsave` 0 (a compact item carries no quality or stats). A unique map is a `uniqueitems.txt` row.

| Quality | Where the modifiers come from |
|---|---|
| Normal | none |
| Rare | `magicprefix.txt` and `magicsuffix.txt` rows, and the `rareprefix.txt` and `raresuffix.txt` names, with `itype1` `cmap`; each `modNcode` is a `properties.txt` code, `Min` and `Max` its range |
| Unique | a `uniqueitems.txt` row with `code` = the map's `misc.txt` code, `prop1` to `prop12` as for any unique |

Added by `tools\install.ps1` (an existing row is never changed, except the `cmap` quality cells and the one old `cartographer_map_density` row below): eight `itemstatcost.txt` stats and a `properties.txt` property for each (seven monster stats and the bonus density). The monster stats are signed, -512 to 511, and shown as `+20% Monster Life` and so on. The density stat is wider, 20 bits, -524288 to 524287 per item (the most a saved item holds; vanilla item stats go to 22 bits), shown as `+25% Map Density`.

| Property | Stat | Monsters get |
|---|---|---|
| `mon-life` | `cartographer_mon_life` | `item_maxhp_percent` |
| `mon-damage` | `cartographer_mon_damage` | `damagepercent` |
| `mon-speed` | `cartographer_mon_speed` | `velocitypercent` (keep it above -100: the plugin cuts a map's own value to -99, because what -100 or lower does to monsters has not been checked in game, and other slows on a monster still stack; `cartographer check` notes a `magicprefix.txt`, `magicsuffix.txt`, `uniqueitems.txt`, `setitems.txt` or `cubemain.txt` row that reaches -100 or lower) |
| `mon-fire-res`, `mon-cold-res`, `mon-light-res`, `mon-poison-res` | `cartographer_mon_fire_res` and so on | `fireresist`, `coldresist`, `lightresist`, `poisonresist` |
| `map-density` | `cartographer_map_density` | not a monster stat: added to the map's `Density` (see below) |

Any other stat on the map item (a resistance, `str`, `item_magicbonus`) is the player's while in the map. When the map opens the plugin reads the item's stats (up to 16), takes the map level out, and applies the rest: the monster ones to every monster that spawns in the map, the player ones to the player (removed on leaving; `PlayerStats` `0` turns that off). The plugin log line `map stats for ...` lists what was read and applied.

Example rows (columns not named are as in the vanilla file; `spawnable` 1, `rare` 1, `itype1` `cmap`):

| File | Row |
|---|---|
| `magicprefix.txt` | `Name` `Brutish`, `mod1code` `mon-life`, `mod1min` 20, `mod1max` 40 |
| `magicsuffix.txt` | `Name` `of Resilience`, `mod1code` `res-all`, `mod1min` 5, `mod1max` 9 |
| `uniqueitems.txt` | `index` `Cartographer Test Unique`, `code` the map code, `prop1` `mon-life` 30, `prop2` `mon-fire-res` -20, `prop3` `res-all` 15, `prop4` `map-level` 95 |

**Bonus density and magic find (#313).** An affix row's first mod is the property itself (the `mon-life` above). Its other mods are fixed values (`Min` = `Max`) for the map's density and for `item_magicbonus`, added up over all the item's affixes when the map opens. The density is the property `map-density` (a percent) and the magic find is the vanilla property `mag%` (`item_magicbonus`, a player stat while in the map). The `map-density` values on the item are summed and added to the map's `Density` from `maps.txt` when the map opens (a base of 0 works: 0 plus 75 is 75), with no cap in the plugin; a negative value only cancels positive ones (the result is never below 0, and 0 leaves the tileset's own density). A saved item holds one value of -524288 to 524287 for the stat, the sum of its affixes; a value outside it wraps when the item is saved (524288 reloads as -524288, which opens the map with no bonus). The plugin log line `map density` shows the sum. Example `magicprefix.txt` row: `mod1code` `mon-life` 20 to 40, `mod2code` `mag%` 15 to 15, `mod3code` `map-density` 25 to 25, so every `Brutish` map adds 15 magic find and 25 percent more monsters on top of the life roll.

Name text: the game shows an affix by its `Name` as a string key. Add the names to your strings (a missing one shows as the key, and the log notes `Missing string`).

Retired: `mapstats.txt`, the `Properties` column and setting and `ItemStats`. A file or column left over is ignored and `cartographer check` says so.

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

Global settings, read when the game starts; `PlayerStats` is checked in game (single player). Two columns, `Setting` and `Value`; one setting per row. Setting names ignore case. A blank `Value` keeps the default. An unknown setting, or a value outside its range, is reported and the default is kept. Without the file, every default applies.

| Setting | Meaning | Default | Example |
|---|---|---|---|
| `EventChance` | Percent chance of an event per map, 0 to 100 | 20 | 30 |
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
