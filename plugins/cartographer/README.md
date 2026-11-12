# Cartographer

A D2RLoader plugin for Diablo II: Resurrected that opens a map from the map's own item: in Harrogath, in Hell, a red portal
leads to a level built from your `maps.txt`. MIT licensed.

**Status: phase 1 (not released).** The table loader, the gates, the items and the red portal
are written, unit tested and checked live in one game (D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta; see Tested). If any
part is not found exactly once the plugin loads, reads and checks your tables, and the portal stays off
(`cartographer check` says which part is missing). Generated layouts come in later phases.

## Features

- **Plain-English tables**: `maps.txt` and `cartographer.txt` are checked row by row. A bad row is skipped and named in
  `cartographer-report.txt` ("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?"); the rest load.
- **One item, one map**: a map is opened only by the item named in its `Item` cell (for example `t20` opens Shorelines). There
  is no generic map item and no random map; an item no map names is an ordinary item. The item belongs to your mod and needs
  item spell 16 (`pSpell` in `misc.txt`); `cartographer check` names a map whose item is missing, unknown, shared or lacks it.
- **One map per game**: using a map item in Harrogath in Hell is meant to open a red portal, once per game. A refused map
  stays in your inventory; the reason is in the log and the console, and on screen you see "I can't use this yet." Both
  opening in Hell and a refusal (level gate) were seen in a game.
- **Console open**: `cartographer open <Name>` opens the named map's portal itself, with no item, through the same gates. It needs
  the `PlayerTick` part (`cartographer check` shows it); without it, or in a game you joined over TCP/IP, the command
  refuses and says so. Opening in town was seen in a game; the TCP/IP refusal was not.

## Your first map

New to Cartographer? [Your first map in 5 minutes](../../docs/cartographer/first-map.md) walks through the item, the
`maps.txt` row and the check. [tables.md](../../docs/cartographer/tables.md) lists every column of every table, and a working
set of tables is in [`examples/first-map/`](examples/first-map/) (plain text, no game files).

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested with 1.3.1-beta)
- Not `core-maps` (the PD2-Resurrected map plugin): use one or the other, never both, because both change how map items
  work. `cartographer check` always says so when `core-maps.dll` is loaded (and rewrites `cartographer-report.txt`). The
  check at startup may miss it if `core-maps` loads after Cartographer (the load order is not verified); the report is
  also rewritten at the first game start, which catches it without `check` when the game-start listener is registered.
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
| Shape | `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl`, or the name of a `shapes.txt` row (checked only; not applied in game yet). Not used by `Preset`. | required |
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
- In game (D2R 3.3.0 build 93847, image stamp 0x6ab3782c, D2RLoader 1.3.1-beta, test mod and test save, Normal difficulty
  unless noted; screenshots and logs checked by hand, not automated):
  - all thirteen parts are found by signature at load and the three hooks install; the unit id the tick sees equals
    `LocalPlayerReady.playerId` (1 and 1);
  - the map's item opens its own map's red portal in Harrogath; stepping through loads the map level and the portal back works;
  - a second open in the same game is refused with "a map has already been opened in this game";
  - the Hell gate refuses with "can only be opened in Hell" (a map whose `Difficulty` is Hell, used on Normal);
  - `cartographer open <Name>` opens the portal with no item in town, and refuses outside town ("only be opened in town");
  - a typo in `maps.txt` gives a plain-English line in `cartographer-report.txt` and in `cartographer check`
    (tileset 'Jungel' is not known, did you mean 'Jungle', that map is skipped);
  - town portal scrolls (blue portal, scroll used) and identify scrolls (ring identified, scroll used) work with Cartographer loaded;
  - (Hell game) a map open in Hell, `Gated` (character level 20): refused at level 1 (log "needs character level 20 (you are 1)", item kept),
    opened at level 25;
  - (Hell game) an item with spell 16 that no map names does nothing and is kept, as with Cartographer unloaded;
  - (Hell game) a town portal scroll used on the map level does nothing, with or without Cartographer (vanilla behaviour on that level);
  - (Hell game) Save and Exit from the map level with the portal open is clean, and the next game opens a map again (the open was not recorded as item or console);
  - (Hell game) Better Auras loaded alongside: both load, the map still opens.
- Not reproduced in game: the "no free spot" refusal (tried with 100 zombies around the player; the spot search still
  succeeded; the collision-bit reading is in `docs/reverse-engineering/cartographer.md`, section 3, and a refusal needs a
  forced null or a fully blocked area), and the TCP/IP client refusal (needs a second game instance).
- Seen in game: `core-maps` (pd2-resurrected) hooks the same item function, so it fails to install its own hook when
  loaded next to Cartographer; run one or the other (see Requirements).
- Expected, unit tested only and not verified in game: the spell-16 use with no matching check, whose code names a map, is
  dropped (it opens nothing and is not passed to the game). The game's code has a path to the use handler that skips the
  check, but which action takes it is not decoded, so no in-game test is named yet.
- Not yet in game for `cartographer open <Name>`: repeating with an aura that reserves mana; leaving the game with the portal open and using the console in the next game; as a TCP/IP client the command refuses.
