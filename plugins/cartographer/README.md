# Cartographer

A D2RLoader plugin for Diablo II: Resurrected that opens a map from the map's own item. In Harrogath, in Hell, the item
opens a red portal to a level built from your `maps.txt`. Maps can carry random properties and events (a boss, a horde,
an ambush, a shrine or a treasure). MIT licensed.

**Not released yet.** See Status below for what has been seen working in game.

## Features

- **One item, one map**: a map is opened by the item named in its `Item` cell. An item no map names stays an ordinary item.
- **Red portal**: one map per game, in Harrogath, in Hell. A refused map stays in your inventory and you see "I can't use this yet."
- **Plain-English tables**: `maps.txt` and `cartographer.txt` are checked row by row. A bad row is skipped and named in
  `cartographer-report.txt`; the rest load.
- **Cave and Crypt mazes**: every map is generated (Cave or Crypt) from the map's layout (`Corridor`, `Winding`,
  `Branching`, `Loop`, `Sprawl`) through the `Maze` slot that `tools\install.ps1` adds (see below).
- **Map properties**: random properties from `mapstats.txt` apply to the map's monsters. Player properties are on by
  default (`PlayerStats`, `0` turns them off) and are removed when you leave the map and are not kept in your save.
- **Map boss**: a map's `Boss` stands in the far room of a Cave or Crypt maze.
- **Outdoor map rooms** (`OutdoorBuild`, off, not checked in game): you arrive in the planned entrance clearing, the Boss stands in the boss clearing.
- **Map events**: Boss, Horde, Ambush, Shrine and Treasure events, with a banner on entry (Shrine and chest
  placement are experimental).
- **Map names** (experimental): a map's own name shows on its portal, automap and entry banner; `MapNames=0` turns it off.
- **Multiplayer sync** (experimental): when the host opens a map, TCP/IP clients are told which map it is. Not on Battle.net.
- **Console**: `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` opens a map's portal
  in town without its item.

## Your first map

[Your first map in 5 minutes](../../docs/cartographer/first-map.md) walks through the item, the `maps.txt` row and the
check. [tables.md](../../docs/cartographer/tables.md) lists every column of every table, and a working set of tables is
in [`examples/first-map/`](examples/first-map/).

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested with 1.3.1-beta).
- A mod. Without one no tables are read, so there are no maps.
- Not `core-maps` (the PD2-Resurrected map plugin): use one or the other, never both. `cartographer check` warns when
  both are loaded.

## Installation

1. Close the game.
2. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds the Maze level and the `Maze` slot to the mod's
   tables, plus a `superuniques.txt` row for each boss in `bosses.txt` (backing up each file once as `.bak`), and adds
   no items; it also adds the item type `Map` (`cmap`) to `itemtypes.txt`. The level is a Cave by default; add
   `-Tileset Crypt` for a Crypt. One level serves one tileset. Running it again is safe. If the mod ships its own
   `.bin` table files the game reads those instead: rebuild or remove them first.
   The installer refuses a spot another level in `levels.txt` already uses (it says which, and `-MazeOffsetX` and the like pick another).
4. Give each map an item of its own: a new row in your mod's `misc.txt` with item spell 16 (`pSpell`) and `type` `cmap` (the item
   type the installer added), for example a copy of the Scroll of Identify, `isc`, with `belt` 0 and `useable` 1 (`isc` has `useable` 1; `cost` 80 and `UICatOverride` `scrlt` also come from `isc`) (not type `scro` or `book`: the game
   names those by their spell, so your item would read "Scroll of Town Portal"). Put `maps.txt` and, if you like, `cartographer.txt` in
   If another level of the mod already sits where a map level would go, the installer leaves that one out and says how
   to pick another spot (`-MazeOffsetX` and the like).
4. Give each map an item of its own in your mod's `misc.txt`: any item with item spell 16 (`pSpell`), for example a copy
   of a scroll. Put `maps.txt` and, if you like, `cartographer.txt` in
   `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`.
5. Start the game through D2RLoader, then run `cartographer check` in the console and read `cartographer-report.txt` in
   the plugin's log folder. If an engine part is not found, the plugin still loads and checks your tables, but the
   portal stays off and `cartographer check` says which part.

## Manual install

`tools\install.ps1` is optional. It only makes the edits below, adds rows to the end of each table and never changes a
game or mod row, so you can make them yourself. Work in `<Mod>.mpq\data\global\excel\`; column detail is in
[docs/cartographer/tables.md](../../docs/cartographer/tables.md).

1. **levels.txt**: add three rows at the end, each a copy of a game row with these cells changed. Give each the next free
   `Id`, `Act` 4 (Act 5), `Depend` 0, `Position` 0, `Vis0` to `Vis7` 0, `Warp0` to `Warp7` -1, `Waypoint` 255, `Quest` 0,
   `LevelGroup` Null, and empty `LevelEntry` and `LevelWarp`. Set `SizeX`, `SizeY` and the same two in every difficulty
   column to the size below. Pick an `OffsetX`/`OffsetY` no other Act 5 level uses.

   | Name | Copy of | DrlgType | LevelType | Size | OffsetX, OffsetY |
   |---|---|---|---|---|---|
   | `Cartographer Maze` | a plain game maze row (DrlgType 1) of the wanted type, such as level 9 for a Cave or 18 for a Crypt | 1 | 3 (Cave) or 4 (Crypt) | 360 | 6000, 0 |
   | `Cartographer Outdoor` | level 111 | 3 | 21 | 384 | 6000, 3000 |
   | `Cartographer Snow` | level 117 | 3 | 31 | 192 | 6000, 3600 |

   The two outdoor rows also get `SubType` and `SubTheme` -1. Leave `LevelName` as copied (it is the string the portal
   shows).
2. **lvlmaze.txt**: copy the first row; set `Name` to `Cartographer Maze`, `Level` to the Maze row's `Id`, every
   `Rooms` column to 4, `SizeX` and `SizeY` to 24 and `Merge` to 500.
3. **superuniques.txt**: for each `bosses.txt` entry copy the `Bishibosh` row and set `Superunique` to `cartoboss:` plus
   the `Name`, `Name` to `cartographer:boss_` plus the `Name`, `Class` to the entry's `Base`, `Mod1` to `Mod3` to its
   `Mods` (0 when unused), `MinGrp` and `MaxGrp` to its `Minions`, and `hcIdx` to one more than the highest `hcIdx` in the
   table (then one more for each further row). In both names, write every character that is not A-Z, a-z or 0-9 as `_` and
   its two-digit **uppercase** hex code (`Shoreline Warden` becomes `Shoreline_20Warden`). Keep to the
   [bosses.txt](#bossestxt) limits: at most 16 entries, `Mods` 1-255 (up to 3), `Minions` 0-10. Set the treasure class cells `TC`,
   `TC(N)` and `TC(H)` to `Act 5 Super C`, `Act 5 (N) Super C` and `Act 5 (H) Super C`, `TC(H) Desecrated` to
   `Act 5 (H) Super C Desecrated`, and the other two `Desecrated` cells to empty.
4. **slots.txt**: in `d2rloader\cartographer\` beside `maps.txt` (not in `d2rloader\plugins`), a tab-separated file:
   header `Slot` and `Level`, then rows `Maze`, `Outdoor` and `OutdoorSnow`, each with the `Id` of your own row from step 1.
5. **itemtypes.txt**: add one row at the end, a copy of the `herb` row, with `ItemType` `Map`, `Code` `cmap`, `Equiv1` `misc`, `Equiv2` empty, `Repair`, `Body`, `Throwable`, `Reload`, `ReEquip`, `AutoStack`, `Normal` and `Beltable` 0, and `Magic`, `Rare`, `Shoots`, `Quiver`, `BodyLoc1` and `BodyLoc2` empty (`StorePage` stays `misc`, as on `herb`). The `Map` type is for the map items: a type of its own
   because the game names an item of type `scro` or `book` after its item spell (not its `namestr`), and `Equiv1` `misc` makes it plain
   miscellaneous like `herb`, `scro` and `key` (it is not a scroll, so it cannot be a Town Portal scroll; not a potion
   type, so no belt). A row with code `cmap` that is not this one must be changed first.

## Configuration

`maps.txt` has one row per map and a header row. Only `Name`, `Tileset`, `Shape` and `Item` are required.

| Column | Meaning | Default |
|---|---|---|
| Name | Unique name | required |
| Tileset | `Cave` or `Crypt` (the kind the installed Maze level is); other tileset names are accepted but refused for now | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl` | required |
| Size | `Small`, `Medium` or `Large` | Medium |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map (unique, item spell 16) | required |
| Boss | The `Name` of a boss in `bosses.txt`; a vanilla boss is refused | none |
| Monsters | Up to 10 monster ids | none |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| Properties, EventChance | Numbers that override the global setting for this map | cartographer.txt |
| Density, Uniques | Percent more monsters and unique packs than the level's own (experimental) | 0 |
| Champions | Read and checked; not applied yet | 0 |

`cartographer.txt` has the columns `Setting` and `Value`:

| Setting | Meaning | Default |
|---|---|---|
| Properties | How many random properties a map gets | 6 |
| EventChance | Percent chance a map has an event | 20 |
| PlayerStats | `1` applies the map's player properties while you are in the map (at most 16 stats, the most the character sheet shows); removed when you leave the map and not kept in your save (checked in single player, one test character); `0` turns it off | 1 |
| MapNames | `0` turns off showing a map's own name on the portal, automap and banner (experimental) | 1 |
| MazeProbe | `1` logs what the game builds for a maze (for debugging) | 0 |
| OutdoorBuild | `1` lets Jungle maps open and builds them (see [Outdoor slot](#outdoor-slot-jungle)); needs a restart, not verified in game | 0 |
| OutdoorProbe | `1` logs what the game builds for an outdoor map (for debugging) | 0 |
| ItemStats | `1` uses the map item's own stats as the map's properties (not checked in game) | 0 |
| Town | The town the portal opens in; only `Harrogath` is supported | Harrogath |
| AutoCheck | `1` runs a game check by itself in a Hell game and writes `AUTOCHECK PASS`, `FAIL` or `N/A` lines to the plugin log; for test slots only | 0 |
| AutoCheckMap | The `Name` of the map `AutoCheck` opens, without its item; for test slots only | none |

Walk into the portal yourself and leave the game by hand; the plugin does the rest and writes the results to its log. Leave `AutoCheck` at 0 for normal play.

### Optional files

These files sit beside `maps.txt` and are tab separated with a header row. `slots.txt` is required, but the installer
writes it; the others can be left out. A bad row is skipped and named in `cartographer-report.txt`, except in
`signatures.txt`, where a bad row is skipped silently and shows only as that engine part being off. [tables.md](../../docs/cartographer/tables.md) is the full reference for every file.

#### mapstats.txt

The pool of random properties. Each map draws `Properties` of them. Without the file a small built-in pool is used.

| Column | Meaning | Default |
|---|---|---|
| Target | `Monsters` or `Players`; `Players` rows apply only when `PlayerStats` is `1` (experimental) | required |
| Stat | A stat name from the game's `itemstatcost.txt`, spelled exactly | required |
| Min, Max | Lowest and highest rolled value; if `Max` is below `Min`, `Min` is used | required |
| Weight | How often the row is drawn compared with the others; `0` switches it off | 1 |

Example row: `Monsters	item_maxhp_percent	10	20	3`

#### bosses.txt

The bosses a map can have. A map's `Boss` in `maps.txt` must name a row here by its `Name`. A boss is always one of
these own entries, never a vanilla boss: a `Boss` that names a vanilla superunique (such as Bishibosh) or a vanilla
monster, or a name that is not in this file, is refused at load (the report and the log say so) and that map opens
without a Boss. Without this file no map has a Boss. `tools\install.ps1` writes one `superuniques.txt` row per entry,
keyed `cartoboss:` plus the Name (non-letters and digits written as `_` and a hex code, like the text key), so it can never match a vanilla row; it never edits a vanilla one. It checks each entry first
(the `Base` must be an ordinary monster in `monstats.txt`, never a vanilla boss such as `andariel`; each `Mods` id must
be in `monumod.txt`) and reports and skips a bad one. Run it again after you change this file, then restart the game: it
rebuilds the `cartoboss:` rows, so a renamed or removed entry loses its old row. At most 16 entries are used (the game
keeps a once-per-game spawn flag per row in a list of unknown length). If the game does not have an entry's row, or
the game returns no unit for it, the boss is spawned as a plain monster of its `Base` instead (without mods or
minions) and the log says which path was taken. The same happens for a second spawn of the same entry in one game
(two maps with that boss, or the same map opened twice): the game allows each boss once per game.

The boss's name in game is the entry's `Name`: the installer writes the row's `Name` cell as a text key (`cartographer:boss_<Name>`, with every character other than a letter or digit written as `_` and its two-digit hex code, so `Shoreline Warden` is `boss_Shoreline_20Warden`)
and the plugin defines that text when it loads. Run the installer again and restart; not yet checked in game. A row the installer writes but the plugin refuses (for example after `bosses.txt` was edited following the install) gets an unregistered key, which is harmless.
If you rename an entry while the game runs, its boss shows placeholder text until you rerun the installer and restart the game.

| Column | Meaning | Default |
|---|---|---|
| Name | The boss's name; unique, and not the name of a vanilla boss or monster. It is the name shown in game (see above) | required |
| Base | A monster id from the game's `monstats.txt` that the boss is made from | required |
| Mods | Up to 3 monster mod ids from `monumod.txt`, separated by commas (for example `5,6`: strong, fast) | none |
| Minions | How many of the base monster come with the boss, 0 to 10 | 0 |

Example row: `Shoreline Warden	zombie1	5,6	2` under the header `Name	Base	Mods	Minions`. The name, the mods
and the minions come from the `superuniques.txt` row and have not been checked in game yet.

#### mapevents.txt

The events a map can have. A map has `EventChance` percent chance of one event, picked by `Weight`. Without the file there
are no events.

| Column | Meaning | Default |
|---|---|---|
| Name | Shown to the player when the event starts | required |
| Kind | `Boss`, `Ambush`, `Horde`, `Treasure` or `Shrine` (Shrine and Treasure chest placement are experimental) | required |
| Spawn | A monster id, a unique monster name or an object name | required |
| Count | Minions (Boss), wave size (Ambush) or horde size (Horde), 1 to 200 | 1 |
| Waves | Number of waves, 1 to 50; Ambush only | 3 |
| Drop | A treasure class name: the drop of a Treasure, or extra loot for a Boss | none |
| Message | Extra text shown when the event starts | none |
| Weight | How often the row is drawn compared with the others; `0` switches it off | 1 |

Example row: `The Drowned King	Boss	Bishibosh	6			The water churns...	2`

#### shapes.txt

Your own maze shapes, for Cave and Crypt maps. Use a row's `Name` in the `Shape` column of `maps.txt`; the built-in shape
names cannot be reused. Every column except `Name` is optional, and a blank one keeps the value of the `Base` shape. A row
that cannot be built is skipped.

| Column | Meaning | Default |
|---|---|---|
| Name | The name to use in `maps.txt` | required |
| Base | A built-in shape, or an earlier row, to start from | Winding |
| MinPath, MaxPath | Rooms on the main path (at least 4) | the base shape's |
| Straight | Percent chance to keep going straight at each step, 0 to 100 | the base shape's |
| MinTurns, MaxTurns | Direction changes along the main path | the base shape's |
| MinBranches, MaxBranches | Dead-end side rooms | the base shape's |
| MinBranchLen, MaxBranchLen | Rooms in one side room | the base shape's |
| Loop | `1` for a ring back to the entrance, `0` for a path | the base shape's |
| MinLoops, MaxLoops | Extra loops that rejoin the layout | the base shape's |
| Width, Height | Grid size in rooms, 2 to 32 | from `Size` |
| BossRoom | `1` to place a boss room, `0` for none | the base shape's |

Example row: `Snake	Winding	12	16	30	2	4	0` under the header `Name	Base	MinPath	MaxPath	Straight	MinBranches	MaxBranches	Loop`.

#### slots.txt

Which level the `Maze` slot opens. `tools\install.ps1` writes the row (see [Maze slot](#maze-slot-cave-and-crypt)). A slot
that names a level of the original game, a town, a level with no row in `levels.txt`, a `Maze` level whose row is not named `Cartographer Maze`, or a level another slot uses is
left out and named in `cartographer-report.txt`; maps then do not open. An old `Preset` row is ignored and can be deleted.

| Column | Meaning | Default |
|---|---|---|
| Slot | `Maze` | required |
| Level | The level id in the mod's `levels.txt`, above 137 | required |

#### signatures.txt

For a game build the plugin does not know: replaces how Cartographer finds one of the game's functions, without a
rebuild. You will not normally need it. See [the reverse-engineering notes](../../docs/reverse-engineering/cartographer.md) (an index, one file per section).

| Column | Meaning | Default |
|---|---|---|
| Part | The name of the engine part, as the plugin log lists it | required |
| Signature | The byte pattern, hex bytes separated by spaces, `??` for any byte | required |

A part found this way is checked before it is used; if the check fails the part stays off and the log says why.

## Maze slot (Cave and Crypt)

Every map is a generated maze, built on the `Maze` level that `tools\install.ps1` adds. One Maze level serves one
tileset (Cave or Crypt); a map of the other tileset is refused. The maze is built once per game, so a second map in the
same game is refused. Without the level, no map opens.

The installer adds, to the mod's tables, a level "Cartographer Maze" copied from a plain game Cave (or Crypt) level and
set apart from the game's levels (no quest, no level group, `Vis0`..`Vis7` set to 0), with its `lvlmaze.txt` row and the `Maze` row of
`slots.txt`. To make a Crypt level, run `tools\install.ps1 -Tileset Crypt`.
Restart the game afterwards. If the plan cannot be built, the game builds its own maze and the log says why.

## Outdoor slot (Jungle)

A map with Tileset `Jungle` is an outdoor map. It opens through the `Outdoor` slot (a second row in `slots.txt`, level
"Cartographer Outdoor") instead of the Maze slot, from its item and the same red portal from Harrogath, in Hell, once per
game. The installer adds that level next to the Maze level: Act 5, outdoor (DrlgType 3), Jungle (LevelType 21), 384 x 384 tiles at
(6000, 3000), copied from a plain Act 5 outdoor level with no quest, no level group, no warps and `Vis0`..`Vis7` = 0
(`-1` crashes the game). It needs no `lvlprest.txt` or `lvlmaze.txt` row. Running it again changes nothing, and the
game's own rows are never edited. Restart the game afterwards.

An outdoor map opens only while `OutdoorBuild` is `1` in `cartographer.txt` (default 0, so the game runs as without the
outdoor code). With it at 0, a Jungle map's item says so and nothing opens. If the Outdoor slot is missing, not the level the
installer wrote, not Act 5 or not an outdoor Jungle level, or an outdoor part cannot be found, the map is refused with a
plain message, named in `cartographer-report.txt` and `cartographer check`. Players with no Jungle map see no change.
`examples/first-map` has one Jungle row (item `t22`).
A map with Tileset `Snow` (Act 5 Barricade fences) opens the same way through the `OutdoorSnow` slot. The installer adds that
second level too ("Cartographer Snow", LevelType 31, 192 x 192 tiles at (6000, 3600)); a Snow map's Shape is ignored and its Size
is Small, Medium or Large. A Snow map gets an entrance, a boss room and an event room from its plan, as a Jungle map does (not checked in game).

Not verified in game. In multiplayer (TCP/IP, experimental) the host sends the outdoor layout to clients; that has not been run in a game either.

## Status

Tested on D2R 3.3.0 build 93847 with D2RLoader 1.3.1-beta, on a test mod and test save, mostly in single player.

- **Works in game**: the map item opening the red portal, the one-map-per-game and Hell refusals, `cartographer open`
  in town, the plain-English table report, Cave and Crypt mazes built from the plan, map properties on monsters,
  Boss, Horde and Ambush events, and `MapNames=1` (a map's own name seen on its portal, automap and banner in single player), arriving in the planned maze entrance room, `PlayerStats` in single player (one test character), and opening a map in Hell (a map's Boss spawned in the far room; a superunique Boss was refused once and spawned in a second game).
- **Experimental or not verified in game**: Shrine and Treasure chest placement (not placed in the test), the event
  banner text (not seen on screen), `PlayerStats` in multiplayer, `MapNames` in multiplayer, multiplayer sync, Nightmare
  difficulty for mazes, and a second Crypt map in one game.

The full testing log, with what each check showed, is in [docs/cartographer/testing.md](../../docs/cartographer/testing.md).
