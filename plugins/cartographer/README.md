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
- **Cave and Crypt mazes**: these maps are built from the map's layout (`Corridor`, `Winding`, `Branching`, `Loop`,
  `Sprawl`) through a `Maze` slot you add by hand (see below).
- **Map properties**: random properties from `mapstats.txt` apply to the map's monsters. Player properties are off by
  default (`PlayerStats`, test characters only, experimental).
- **Map events**: Boss, Horde, Ambush, Shrine and Treasure events, with a banner on entry (Shrine and chest
  placement are experimental).
- **Map names** (off by default, experimental): `MapNames=1` shows a map's own name on its portal, automap and entry banner.
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
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds the map level and the `Preset` slot to the mod's
   tables (backing up each file once as `.bak`) and adds no items. Running it again is safe. If the mod ships its own
   `.bin` table files the game reads those instead: rebuild or remove them first.
4. Give each map an item of its own in your mod's `misc.txt`: any item with item spell 16 (`pSpell`), for example a copy
   of a scroll. Put `maps.txt` and, if you like, `cartographer.txt` in
   `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`.
5. Start the game through D2RLoader, then run `cartographer check` in the console and read `cartographer-report.txt` in
   the plugin's log folder. If an engine part is not found, the plugin still loads and checks your tables, but the
   portal stays off and `cartographer check` says which part.

## Configuration

`maps.txt` has one row per map and a header row. Only `Name`, `Tileset`, `Shape` and `Item` are required.

| Column | Meaning | Default |
|---|---|---|
| Name | Unique name | required |
| Tileset | `Preset`, `Cave` or `Crypt`; other tileset names are accepted but refused for now | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl` (not used by `Preset`) | required |
| Size | `Small`, `Medium` or `Large` | Medium |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map (unique, item spell 16) | required |
| Boss, Monsters | A monster id or superunique name; up to 10 monster ids | none |
| Difficulty | `Normal`, `Nightmare` or `Hell` | Hell |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| Properties, EventChance | Numbers that override the global setting for this map | cartographer.txt |
| Density, Champions, Uniques | Read and checked; not applied yet | cartographer.txt |

`cartographer.txt` has the columns `Setting` and `Value`:

| Setting | Meaning | Default |
|---|---|---|
| Properties | How many random properties a map gets | 6 |
| EventChance | Percent chance a map has an event | 20 |
| PlayerStats | `1` applies the map's player properties; test characters only | 0 |
| MapNames | `1` shows a map's own name on the portal, automap and banner (experimental) | 0 |
| MazeProbe | `1` logs what the game builds for a maze (for debugging) | 0 |
| ItemStats | Read; not applied yet | 0 |
| Town | The town the portal opens in; only `Harrogath` is supported | Harrogath |

Optional files beside `maps.txt`: `mapstats.txt` (the property list), `mapevents.txt` (the events), `shapes.txt` (your
own shapes; checked only, not applied yet), `slots.txt` (which level each slot uses; the installer creates it) and
`signatures.txt` (engine signatures for a new game build, see
[the reverse-engineering notes](../../docs/reverse-engineering/cartographer.md)).

## Maze slot (Cave and Crypt)

A `Cave` or `Crypt` map opens through a second level, the `Maze` slot, which you add by hand. One Maze slot serves one
tileset (Cave or Crypt); a map of the other tileset is refused. The maze is built once per game, so a second Cave or
Crypt map in the same game is refused. Without the slot, only Cave and Crypt maps are refused.

1. In the mod's `levels.txt`, add a level in Act 5 with `DrlgType` 1, `LevelType` 3 (Cave) or 4 (Crypt), `SizeX` and
   `SizeY` up to 360 in all three difficulties, `Depend` 0, `Position` 0 and `OffsetX` about 6000, plus a matching
   `lvlmaze.txt` row. Keep a `LevelName` that has a string.
2. Add a `Maze` row to `slots.txt` (tab separated, header `Slot` and `Level`) with the new level's id:

   ```
   Slot	Level
   Preset	138
   Maze	139
   ```

3. Restart the game. If the plan cannot be built, the game builds its own maze and the log says why.

## Status

Tested on D2R 3.3.0 build 93847 with D2RLoader 1.3.1-beta, on a test mod and test save, mostly in single player.

- **Works in game**: the map item opening the red portal, the one-map-per-game and Hell refusals, `cartographer open`
  in town, the plain-English table report, Cave and Crypt mazes built from the plan, map properties on monsters,
  Boss, Horde and Ambush events, and the portal tooltip reading "Colossal Summit".
- **Experimental or not verified in game**: Shrine and Treasure chest placement (not placed in the test), the event
  banner text (not seen on screen), `PlayerStats`, `MapNames`, multiplayer sync, arriving in the planned maze entrance
  room, Nightmare and Hell difficulty for mazes, and a second Crypt map in one game.

The full testing log, with what each check showed, is in [docs/cartographer/testing.md](../../docs/cartographer/testing.md).
