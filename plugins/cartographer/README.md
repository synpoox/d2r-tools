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
- **Cave, Crypt, Lava and Ice caves mazes**: every map is generated (Cave, Crypt, Lava or Ice) from the map's layout (`Corridor`, `Winding`,
  `Branching`, `Loop`, `Sprawl`) through the maze slots that `tools\install.ps1` adds (see below).
- **Map items**: a map item is meant to be normal, rare or unique (a plugin hook makes a map the game rolls as magic normal; a map forced magic by a cheat or already existing stays magic, and the hook applies to any item with the map spell 16). A rare map is rolled by the game from the
  `magicprefix`, `magicsuffix`, `rareprefix` and `raresuffix` rows whose type is `cmap`; a unique map is a
  `uniqueitems.txt` row. The map level and the modifiers are stats on the item, shown on hover. When the map opens,
  monster modifiers (life, damage, speed, resistances) go on its monsters and the others on you; yours are removed when
  you leave the map and are not kept in your save (`PlayerStats=0` turns that off). A normal map has no modifiers.
- **Map panel**: in a map, its monster level and properties (under "Monsters have" and "Players have", in blue like item properties), the map's total Density (its maps.txt `Density` plus the modifiers') and the total Magic Find of its modifiers show at the right of the screen in the game's own terror zone text field, in one colour, in the place of the zone lines (with a terror zone on or off), and go away outside it; outside a map a terror zone shows as usual. If the game's text cannot be hooked, or does not show it, the panel sits under the game's info text instead. A TCP/IP client does not show it.
- **Map boss**: a map's `Boss` stands in the far room of a Cave, Crypt or Lava maze (Ice caves: not always yet, #364).
- **Outdoor map rooms** (Jungle, Snow and Siege, always on; checked in single player, see Status): you arrive in the planned entrance clearing, the Boss stands in the boss clearing.
- **Map events**: Boss, Horde, Ambush, Shrine and Treasure events, with a banner on entry. A Treasure chest is always placed unlocked
  (its loot is not confirmed in game yet).
- **Map names** (experimental): a map's own name shows on its portal, automap and the game's "Entering" text; `MapNames=0` turns it off.
- **Multiplayer sync** (experimental): when the host opens a map, TCP/IP clients are told which map it is. Not on Battle.net.
- **Console**: `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` opens a map's portal
  in town without its item.

## Your first map

[Your first map](../../docs/cartographer/first-map.md) walks through the item, the `maps.txt` row and the
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
3. From the folder you unzipped, run
   `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds the Maze, Crypt, Lava, Ice, Outdoor, Snow and Siege levels and their slots to the mod's
   tables, plus a `superuniques.txt` row for each boss in `bosses.txt` and the `cmap` item type in `itemtypes.txt` (backing up each file once as `.bak`). It adds
   no map items. Each tileset has a level of its own, so one install serves Cave, Crypt, Lava and Ice maps (one map per game).
   Running it again is safe. If the mod ships its own
   `.bin` table files the game reads those instead: rebuild or remove them first.
   If another level of the mod already sits where a map level would go, the installer leaves that one out and says how
   to pick another spot (`-MazeOffsetX` and the like).
4. Give each map an item of its own in your mod's `misc.txt`: a row of type `cmap` with item spell 16 (`pSpell`) and `spellicon` -1, for example a copy
   of a scroll with `compactsave` set to 0 (1 keeps every map Normal). Put `maps.txt` and, if you like, `cartographer.txt` in
   `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`.
5. Start the game through D2RLoader, then run `cartographer check` in the console and read `cartographer-report.txt` in
   the plugin's log folder. If an engine part is not found, the plugin still loads and checks your tables, but the
   portal stays off and `cartographer check` says which part.
Step by step, with the example tables: [Your first map](../../docs/cartographer/first-map.md).

1. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
2. Put `bosses.txt` (for map bosses) in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`, then run
   `powershell -ExecutionPolicy Bypass -File tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"` from `plugins\cartographer`
   of the repository (the release zip does not carry it). It adds the Maze, Crypt, Lava, Ice, Outdoor, Snow and Siege levels and their seven slots,
   plus a `superuniques.txt` row per boss and the `cmap` item type in `itemtypes.txt`.
3. Give each map an item: a `misc.txt` row of type `cmap` with item spell 16 (`pSpell`); add `maps.txt` next to `bosses.txt`.
4. Start the game through D2RLoader and run `cartographer check`; the report is in the plugin's log folder.

## Manual install

`tools\install.ps1` is optional. It only makes the edits below, adds rows to the end of each table and never changes a
game or mod row, so you can make them yourself. Work in `<Mod>.mpq\data\global\excel\`; column detail is in
[docs/cartographer/tables.md](../../docs/cartographer/tables.md).

1. **levels.txt**: add six rows at the end, each a copy of a game row with these cells changed. Give each the next free
   `Id`, `Act` 4 (Act 5), `Depend` 0, `Position` 0, `Vis0` to `Vis7` 0, `Warp0` to `Warp7` -1, `Waypoint` 255, `Quest` 0,
   `LevelGroup` Null, and empty `LevelEntry` and `LevelWarp`. Set `SizeX`, `SizeY` and the same two in every difficulty
   column to the size below. Pick an `OffsetX`/`OffsetY` no other Act 5 level uses.

   | Name | Copy of | DrlgType | LevelType | Size | OffsetX, OffsetY |
   |---|---|---|---|---|---|
   | `Cartographer Maze` | a plain game maze row (DrlgType 1) of the wanted type, such as level 9 for a Cave or 18 for a Crypt | 1 | 3 (Cave) or 4 (Crypt) | 360 | 3500, 0 |
   | `Cartographer Lava` | the game's River of Flame (level 107, DrlgType 1) | 1 | 28 (Lava) | 360 | 2500, 0 |
   | `Cartographer Ice Caves` | a plain game Ice caves maze row (DrlgType 1), such as level 113 | 1 | 33 (Ice) | 360 | 2000, 0 |
   | `Cartographer Outdoor` | level 111 | 3 | 21 | 384 | 6000, 3000 |
   | `Cartographer Snow` | level 117 | 3 | 31 | 192 | 1500, 0 |
   | `Cartographer Siege` | level 110 | 3 | 30 | 240 x 48 | 6000, 3900 |

   The two outdoor rows also get `SubType` and `SubTheme` -1. Leave `LevelName` as copied (it is the string the portal
   shows). Keep a maze level's offsets small: the game saves its map cells as 16-bit numbers when you leave it, so a maze level with
   OffsetX minus OffsetY above 3735 (or below -3736), or OffsetX plus OffsetY above 7471, loses its revealed map after a trip to town.
2. **lvlmaze.txt**: copy the first row once for each maze level; set `Name` to `Cartographer Maze`, `Cartographer Crypt`, `Cartographer Lava` or `Cartographer Ice Caves`, `Level` to that
   row's `Id`, every `Rooms` column to 4, `SizeX` and `SizeY` to 24 and `Merge` to 500.
3. **superuniques.txt**: for each `bosses.txt` entry copy the `Bishibosh` row and set `Superunique` to `cartoboss:` plus
   the `Name`, `Name` to `cartographer:boss_` plus the `Name`, `Class` to the entry's `Base`, `Mod1` to `Mod3` to its
   `Mods` (0 when unused), `MinGrp` and `MaxGrp` to its `Minions`, and `hcIdx` to one more than the highest `hcIdx` in the
   table (then one more for each further row). In both names, write every character that is not A-Z, a-z or 0-9 as `_` and
   its two-digit **uppercase** hex code (`Shoreline Warden` becomes `Shoreline_20Warden`). Keep to the
   [bosses.txt](#bossestxt) limits: at most 16 entries, `Mods` 1-255 (up to 3), `Minions` 0-10. Set the treasure class cells `TC`,
   `TC(N)` and `TC(H)` to `Act 5 Super C`, `Act 5 (N) Super C` and `Act 5 (H) Super C`, `TC(H) Desecrated` to
   `Act 5 (H) Super C Desecrated`, and the other two `Desecrated` cells to empty.
4. **slots.txt**: in `d2rloader\cartographer\` beside `maps.txt` (not in `d2rloader\plugins`), a tab-separated file:
   header `Slot` and `Level`, then rows `Maze`, `MazeCrypt`, `MazeLava`, `MazeIce`, `Outdoor`, `OutdoorSnow` and `OutdoorSiege`, each with the `Id` of your own row from step 1.
5. **itemtypes.txt**: add one row at the end, a copy of the `herb` row, with `ItemType` `Map`, `Code` `cmap`, `Equiv1` `misc`, `Equiv2` empty, `Repair`, `Body`, `Throwable`, `Reload`, `ReEquip`, `AutoStack`, `Normal` and `Beltable` 0, `Rare` 1, `Magic` 0, and `Shoots`, `Quiver`, `BodyLoc1` and `BodyLoc2` empty (`StorePage` stays `misc`, as on `herb`). The `Map` type is for the map items: a type of its own
   because the game names an item of type `scro` or `book` after its item spell (not its `namestr`), and `Equiv1` `misc` makes it plain
   miscellaneous like `herb`, `scro` and `key` (it is not a scroll, so it cannot be a Town Portal scroll; not a potion
   type, so no belt). A row with code `cmap` that is not this one must be changed first.

## Configuration

`maps.txt` has one row per map and a header row. Only `Name`, `Tileset`, `Shape` and `Item` are required.

| Column | Meaning | Default |
|---|---|---|
| Name | Unique name | required |
| Tileset | `Cave`, `Crypt`, `Lava` or `Ice` (each opens through its own level); other tileset names are accepted but refused for now | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop`, `Sprawl` or `Open` (an open room grid: all rooms joined, a few walls, no dead ends; not for `Snow`). On Cave, a 200-tile slot shrinks ExtraLarge Open to about 7 x 7 rooms; on Jungle, ExtraLarge Open uses the 9 x 9 Large grid, since every block is stamped. `Snow` and `Siege` ignore Shape: leave the cell blank there | required (blank is fine for Snow and Siege) |
| Size | `Small`, `Medium`, `Large` or `ExtraLarge` | Large |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map (unique, item spell 16) | required |
| Boss | The `Name` of a boss in `bosses.txt`; a vanilla boss is refused | none |
| Monsters | Up to 10 monster ids | none |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| EventChance | A number that overrides the global setting for this map | cartographer.txt |
| Density, Uniques | `Density`: percent more monsters than the level's own, no upper limit except the game's own field size. `Uniques`: percent more unique packs over the whole map (the level's own count raised by that percent, at most 255). The plugin hands them out with the rooms you enter, so they keep appearing toward the far end (not tried while walking; a map with 0 is left to the game). The game itself makes about 6 percent of packs unique, so a count above that share of the map's packs is not reached; raise `Density` for more (experimental) | 0 |
| Champions | Read and checked, never applied: the game has no setting for it | 0 |

`cartographer.txt` has the columns `Setting` and `Value`:

| Setting | Meaning | Default |
|---|---|---|
| EventChance | Percent chance a map has an event | 20 |
| PlayerStats | `1` applies the map's player properties while you are in the map (at most 16 stats, the most the character sheet shows); removed when you leave the map and not kept in your save (checked in single player, one test character); `0` turns it off | 1 |
| MapNames | `0` turns off showing a map's own name on the portal, automap and "Entering" text (experimental) | 1 |
| MazeProbe | `1` logs what the game builds for a maze and, per room, the level record's room and unique counters (for debugging) | 0 |
| OutdoorProbe | `1` logs what the game builds for an outdoor map (for debugging) | 0 |
| Town | The town the portal opens in; only `Harrogath` is supported | Harrogath |
| AutoCheck | `1` runs a game check by itself in a Hell game and writes `AUTOCHECK PASS`, `FAIL` or `N/A` lines to the plugin log; for test slots only | 0 |
| AutoCheckMap | The `Name` of the map `AutoCheck` opens, without its item; for test slots only | none |

The map level can also sit on the map item as the stat `cartographer_map_level` (added to `itemstatcost.txt` by `tools\install.ps1`, with an `Area Level: N` line on hover). Every new map item is given its `maps.txt` Level as this stat and shows it first, above its other properties (the stat's `descpriority`); maps that already exist have no line and use the `maps.txt` Level, and a map made by a cube recipe gets the line too. A `map-level` property on an affix or unique replaces the stamped Level. Checked in game: `cheat spawn` and cube recipes. A monster drop was stamped (log) but its hover was not seen; vendor, chest and gamble maps were not checked. A cube recipe can raise it: a `mod` of `map-level` on an output that keeps the map (`usetype,nor,mod`) adds to the map's level, starting from its `maps.txt` Level, and a second transmute adds again (checked in game: 85 to 95 to 105, same map; details in `docs/cartographer/tables.md`).

Walk into the portal yourself and leave the game by hand; the plugin does the rest and writes the results to its log. Leave `AutoCheck` at 0 for normal play.

### Optional files

These files sit beside `maps.txt` and are tab separated with a header row. `slots.txt` is required, but the installer
writes it; the others can be left out. A bad row is skipped and named in `cartographer-report.txt`, except in
`signatures.txt`, where a bad row is skipped silently and shows only as that engine part being off. [tables.md](../../docs/cartographer/tables.md) is the full reference for every file.

#### Map item modifiers

There is no `mapstats.txt`. Rarity and modifiers are the game's own: add `magicprefix.txt`, `magicsuffix.txt`, `rareprefix.txt`,
`raresuffix.txt` rows with `itype1` `cmap`, and `uniqueitems.txt` rows for unique maps (`code` is the map's `misc.txt` code). The
installer adds the monster properties `mon-life`, `mon-damage`, `mon-speed`, `mon-fire-res`, `mon-cold-res`, `mon-light-res` and
`mon-poison-res`, and `map-density`, a bonus to the map's `Density` that adds up over the item's modifiers (magic find is the vanilla `mag%`); any other property is the player's. [tables.md](../../docs/cartographer/tables.md#map-item-rarity-and-modifiers) has the rows and an example.

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

Save `bosses.txt` (and the other tables) as UTF-8. A file that is not valid UTF-8, such as an "ANSI" save, is read as Latin-1 and converted, by the plugin and by the installer alike, so `Gérard Östberg` gives the same name and key (`cartoboss:G_C3_A9rard_20_C3_96stberg`) either way; characters outside Latin-1 need UTF-8.

The boss's name in game is the entry's `Name`: the installer writes the row's `Name` cell as a text key (`cartographer:boss_<Name>`, with every character other than a letter or digit written as `_` and its two-digit hex code, so `Shoreline Warden` is `boss_Shoreline_20Warden`)
and the plugin defines that text when it loads. Run the installer again and restart. The names shown on hover were checked in game (single player, Hell, a Latin-1 `bosses.txt`; PR #261).
If you rename an entry while the game runs, its boss shows placeholder text until you rerun the installer and restart the game.

| Column | Meaning | Default |
|---|---|---|
| Name | The boss's name; unique, and not the name of a vanilla boss or monster. It is the name shown in game (see above) | required |
| Base | A monster id from the game's `monstats.txt` that the boss is made from | required |
| Mods | Up to 3 monster mod ids from `monumod.txt`, separated by commas (for example `5,6`: strong, fast) | none |
| Minions | How many of the base monster come with the boss, 0 to 10 | 0 |

Example row: `Shoreline Warden	zombie1	5,6	2` under the header `Name	Base	Mods	Minions`. The name, the mods
and the minions come from the `superuniques.txt` row (name and mods seen on hover and the minions seen with the boss, single player, Hell).

#### mapevents.txt

The events a map can have. A map has `EventChance` percent chance of one event, picked by `Weight`. Without the file there
are no events.

| Column | Meaning | Default |
|---|---|---|
| Name | Shown to the player when the event starts | required |
| Kind | `Boss`, `Ambush`, `Horde`, `Treasure` or `Shrine` (a Treasure chest is placed unlocked; its loot is not confirmed in game) | required |
| Spawn | A monster id, a unique monster name or an object name | required |
| Count | Minions (Boss), wave size (Ambush) or horde size (Horde), 1 to 200 | 1 |
| Waves | Number of waves, 1 to 50; Ambush only | 3 |
| Drop | A treasure class name: the drop of a Treasure, or extra loot for a Boss | none |
| Message | Extra text shown when the event starts | none |
| Weight | How often the row is drawn compared with the others; `0` switches it off | 1 |

Example row: `The Drowned King	Boss	Bishibosh	6			The water churns...	2`

#### shapes.txt

Your own maze shapes, for Cave, Crypt, Lava and Ice maps. Use a row's `Name` in the `Shape` column of `maps.txt`; the built-in shape
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

Which level each slot opens: `Maze` (Cave), `MazeCrypt` (Crypt), `MazeLava` (Lava), `MazeIce` (Ice caves), `Outdoor` (Jungle), `OutdoorSnow` (Snow) and `OutdoorSiege` (Siege). `tools\install.ps1` writes the rows (see [Maze slot](#maze-slot-cave-crypt-lava-and-ice)). A slot
that names a level of the original game, a town, a level with no row in `levels.txt`, a maze or outdoor slot on a level the installer did not write (`Maze` needs `Cartographer Maze`, `MazeCrypt` needs `Cartographer Crypt`, `MazeLava` needs `Cartographer Lava`, `MazeIce` needs `Cartographer Ice Caves`), or a level another slot uses is
left out and named in `cartographer-report.txt`; maps then do not open. An old `Preset` row is ignored and can be deleted.

| Column | Meaning | Default |
|---|---|---|
| Slot | `Maze`, `MazeCrypt`, `MazeLava`, `MazeIce`, `Outdoor`, `OutdoorSnow` or `OutdoorSiege` | required |
| Level | The level id in the mod's `levels.txt`, above 137 | required |

#### signatures.txt

For a game build the plugin does not know: replaces how Cartographer finds one of the game's functions, without a
rebuild. You will not normally need it. See [the reverse-engineering notes](../../docs/reverse-engineering/cartographer.md) (an index, one file per section).

| Column | Meaning | Default |
|---|---|---|
| Part | The name of the engine part, as the plugin log lists it | required |
| Signature | The byte pattern, hex bytes separated by spaces, `??` for any byte | required |

A part found this way is checked before it is used; if the check fails the part stays off and the log says why.

## Maze slot (Cave, Crypt, Lava and Ice caves)

Every map is a generated maze, built on a maze level that `tools\install.ps1` adds. A level has one `LevelType`, which fixes
its tileset, so each maze tileset has a level of its own: `Cartographer Maze` (Cave, slot `Maze`), `Cartographer Crypt`
(Crypt, slot `MazeCrypt`), `Cartographer Lava` (Lava, slot `MazeLava`) and `Cartographer Ice Caves` (Ice caves, slot `MazeIce`). One install serves all, but the maze is built once per game, so a second map in the
same game is refused. Without the tileset's level, its maps do not open.

The installer adds, to the mod's tables, those levels copied from a plain game Cave level, a Crypt level, River of Flame and a game Ice caves level, and
set apart from the game's levels (no quest, no level group, `Vis0`..`Vis7` set to 0), with their `lvlmaze.txt` rows and the `Maze`,
`MazeCrypt`, `MazeLava` and `MazeIce` rows of `slots.txt`. `-Tileset` is no longer needed. A Maze level made by an older install with `-Tileset Crypt` is set back to LevelType 3 when you rerun the installer (Crypt maps use the new level).
Restart the game afterwards. If the plan cannot be built, the game builds its own maze and the log says why.

## Outdoor slot (Jungle)

A map with Tileset `Jungle` is an outdoor map. It opens through the `Outdoor` slot (a second row in `slots.txt`, level
"Cartographer Outdoor") instead of the Maze slot, from its item and the same red portal from Harrogath, in Hell, once per
game. The installer adds that level next to the Maze level: Act 5, outdoor (DrlgType 3), Jungle (LevelType 21), 384 x 384 tiles at
(6000, 3000), copied from a plain Act 5 outdoor level with no quest, no level group, no warps and `Vis0`..`Vis7` = 0
(`-1` crashes the game). It needs no `lvlprest.txt` or `lvlmaze.txt` row. Running it again changes nothing, and the
game's own rows are never edited. Restart the game afterwards.

Like every map, an outdoor map is built only when you open it from its map item (or with `cartographer open`). If the Outdoor slot is missing, not the level the
installer wrote, not Act 5 or not an outdoor Jungle level, or an outdoor part cannot be found, the map is refused with a
plain message, named in `cartographer-report.txt` and `cartographer check`. Players with no Jungle map see no change.
`examples/first-map` has one Jungle row (item `t22`).
A map with Tileset `Snow` (Act 5 Barricade fences) opens the same way through the `OutdoorSnow` slot. The installer adds that
second level too ("Cartographer Snow", LevelType 31, 192 x 192 tiles at (1500, 0)); a Snow map's Shape is ignored and its Size
is Small, Medium, Large or ExtraLarge. A Snow map gets an entrance, a boss room and an event room from its plan, as a Jungle map does.
A map with Tileset `Siege` (Act 5 Bloody Foothills) opens through the `OutdoorSiege` slot. The installer adds that third outdoor level ("Cartographer Siege", LevelType 30, 240 x 48 tiles at (6000, 3900)); a Siege map is one east-west strip of 12 to 15 pieces (Small, Medium, Large or ExtraLarge), you arrive near the east end and the Boss and event stand in other wide pieces. Its Shape is ignored.

Checked in single player, Hell: opening from the item, arriving in the entrance clearing, the Boss, Horde and Treasure events, map stats, the entry banner and the Jungle automap title. Snow edges: a cliff and ravine border ring seen on all four sides and corners; black void remains at the cap ends (issue #395). Not checked: Jungle edges and void cells (issue #142), and multiplayer, where (TCP/IP, experimental) the host sends the outdoor layout to clients.

## Status

Tested on D2R 3.3.0 build 93847 with D2RLoader 1.3.1-beta, on a test mod and test save, mostly in single player.

- **Works in game**: the map item opening the red portal, the one-map-per-game and Hell refusals, `cartographer open`
  in town, the plain-English table report, Cave and Crypt mazes built from the plan, a Lava map opening and building (no automap lines drawn, #366), an Ice caves map opening, building and running an Ambush (the Boss was not placed in one cell, #364),
  Boss, Horde, Ambush and Shrine events, and `MapNames=1` (a map's own name seen on its portal, automap and banner in single player), arriving in the planned maze entrance room, `PlayerStats` in single player (one test character, kept out of the save), opening a map in Hell (a map's Boss from `bosses.txt` spawned in the boss room in 6 of 6 games) and its refusal in Normal and Nightmare, map items (own name on hover, not beltable, not sold by Malah, sold for gold, kept through Save and Exit).
- **Outdoor maps (Jungle, Snow and Siege)**: opening from the item, arrival, Boss, events, map stats and the entry banner were seen in single player; the edges and void cells (#142), Snow corners and multiplayer were not checked; a Snow wall stopped the character and the Snow automap title was seen. Siege was checked in two games (opening, arrival, Boss and Treasure event); its north and south edges and how its pieces meet were not settled.
- **Experimental or not verified in game**: a Boss `Drop` roll's rare and unique items (9 kills gave normal, low, superior, magic and socketed items) and a Treasure chest's loot beyond a rune in one game (item quality was not compared), the event banner with long text or at other resolutions,
  `PlayerStats` in multiplayer, `MapNames` in multiplayer, multiplayer sync.

The full testing log, with what each check showed, is in [docs/cartographer/testing.md](../../docs/cartographer/testing.md).
