# Cartographer

A D2RLoader plugin for Diablo II: Resurrected that opens a map from the map's own item: in Harrogath, in Hell, a red portal
leads to a level built from your `maps.txt`. MIT licensed.

**Status: phase 1 (not released).** The table loader, the gates, the items and the red portal
are written and unit tested. Nothing has been run in a game yet. The engine parts the portal needs have byte signatures
checked against a memory dump of build 93847 only; until they are checked in game, treat the portal as untested. If any
part is not found exactly once the plugin loads, reads and checks your tables, and the portal stays off
(`cartographer check` says which part is missing). Generated layouts come in later phases.

## Features

- **Plain-English tables**: `maps.txt` and `cartographer.txt` are checked row by row. A bad row is skipped and named in
  `cartographer-report.txt` ("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?"); the rest load.
- **One item, one map**: a map is opened only by the item named in its `Item` cell (for example `t20` opens Shorelines). There
  is no generic map item and no random map; an item no map names is an ordinary item. The item belongs to your mod and needs
  item spell 16 (`pSpell` in `misc.txt`); `cartographer check` names a map whose item is missing, unknown, shared or lacks it.
- **One map per game**: using a map item in Harrogath in Hell is meant to open a red portal, once per game. A refused map is
  meant to stay in your inventory with a message saying why. Neither has been tried in a game yet.
- **Console open**: `cartographer open <Name>` opens the named map's portal itself, with no item, through the same gates. It needs
  the `PlayerTick` part (`cartographer check` shows it); without it, or in a game you joined over TCP/IP, the command
  refuses and says so. Not tried in a game yet.

## Your first map

New to Cartographer? [Your first map in 5 minutes](../../docs/cartographer/first-map.md) walks through the item, the
`maps.txt` row and the check. [tables.md](../../docs/cartographer/tables.md) lists every column of every table, and a working
set of tables is in [`examples/first-map/`](examples/first-map/) (plain text, no game files).

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (not yet tested with any version)
- A mod (without one, no tables are read, so there are no maps and only the default settings apply), because the map level is added to the mod's tables (step 3 below).

## Installation

1. Close the game.
2. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds only what the slot level needs: one level
   ("Cartographer Preset", a copy of Colossal Summit) and its level preset to the mod's `levels.txt` and `lvlprest.txt`
   (backing each up once as `.bak`), and writes `slots.txt`. It adds no items. Running
   it again changes nothing. D2RLoader compiles the mod's `.txt` tables itself at start (no `-txt` needed). If the mod ships
   its own `.bin` table files, the game reads those instead and ignores the edited `.txt`: rebuild the `.bin` files from the
   edited tables, or remove them, before starting.
4. Give each map an item of its own in your mod's `misc.txt`: any item with item spell 16 (`pSpell`), for example a copy of a
   scroll. Put `maps.txt` and, if you like, `cartographer.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`.
5. Start the game through D2RLoader. Check `cartographer-report.txt` in the plugin's log folder.

## Configuration

`maps.txt` has one row per map, with a header row. Only `Name`, `Tileset`, `Shape` and `Item` are needed; a blank cell means the
default. Cartographer's own words (tilesets, shapes, sizes, difficulties) ignore case; names from the game's tables
(monster ids, boss names) must match exactly; item codes ignore case.

| Column | Meaning | Default |
|---|---|---|
| Name | Unique name | required |
| Tileset | `Preset` for now; the other tileset names are accepted but refused until they are built | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl` (not used by `Preset`) | required |
| Size | `Small`, `Medium` or `Large` | Medium |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map, and only this map: required, unique across maps, item spell 16 | required |
| Boss, Monsters | A monstats `Id` or superunique name; up to 10 monster ids | none |
| Difficulty | `Normal`, `Nightmare` or `Hell` | Hell |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| Properties, EventChance, Density, Champions, Uniques | Numbers, kept for later phases | cartographer.txt |

`cartographer.txt` has the columns `Setting` and `Value`: `Properties` (6), `EventChance` (20), `ItemStats` (0) and
`Town` (`Harrogath`, the only town supported). Phase 1 reads and checks the map columns beyond the table above, but every
map in `maps.txt` opens through the one map level (`Preset` slot); nothing opens without a mod and a `maps.txt`.

Engine signatures that a new game build needs can be given in `signatures.txt` (`Part`, `Signature`) beside `maps.txt`
without a rebuild; see `docs/reverse-engineering/cartographer.md`. A hooked part (the two item-spell functions) needs at least the hook size in exact bytes (no `??`) and a function start, or it stays off; a part the plugin only calls needs a unique match only.

## Tested

- Unit tests of the table loader, the gates and the signature scanner (`tools\build.ps1 -Plugin cartographer -Test`).
- The installer on a copy of the vanilla 93847 tables, run twice.
- The portal parts' byte signatures, each unique and resolving to its recorded address in a dump of build 93847
  (`python -I plugins\cartographer\tools\check_portal_signatures.py <dump>`; static, no game).
- Not yet: installed into a game, or opened in game. Game checks: all twelve parts found at load, `UnitStat` in particular (it is
  D2RLoader's thunk and must be in place when Cartographer loads; if it is not, `cartographer check` looks again for the called
  parts with the load-time `signatures.txt` and turns the portal on if all are now found; the two item hooks are written at
  load only, so a missing hooked part needs a restart).
- Not yet in game either: an item no map names is used like an ordinary item (the hooks pass it through).
- Not yet in game either: a spell-16 use with no matching check, whose code names a map, is dropped (it opens nothing and is not passed to the game).
- Game check for `cartographer open <Name>`: the log line "first player unit seen by the tick" shows the same id as
  "local player ready"; the portal opens; step through it and back; leave the game with the portal still open; repeat with
  Better Auras loaded and an aura that reserves mana; as a TCP/IP client the command refuses.
