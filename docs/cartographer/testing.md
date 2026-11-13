# Cartographer: testing log and developer notes

This is the developer-facing detail that used to be in `plugins/cartographer/README.md`: what was unit tested, what
was run in game (build 93847, D2RLoader 1.3.1-beta), what was not, and the long per-feature behaviour notes. Addresses
and signatures are in [`../reverse-engineering/cartographer.md`](../reverse-engineering/cartographer.md) (an index; one file per section under `../reverse-engineering/cartographer/`). The
player-facing summary is the plugin README. Relative links below were adjusted for this folder.

---

A D2RLoader plugin for Diablo II: Resurrected that opens a map from the map's own item: in Harrogath, in Hell, a red portal
leads to a level built from your `maps.txt`. MIT licensed.

**Status: phase 1 (not released); the maze steering for Cave and Crypt maps is written: its steering decisions are unit tested, and the hooks were run in game for Cave and Crypt maps (see Tested).** The table loader, the gates, the items and the red portal
are written, unit tested and checked live in one game (D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta; see Tested). If any
part is not found exactly once the plugin loads, reads and checks your tables, and the portal stays off
(`cartographer check` says which part is missing).

## Features

- **Plain-English tables**: `maps.txt` and `cartographer.txt` are checked row by row. A bad row is skipped and named in
  `cartographer-report.txt` ("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?"); the rest load.
- **One item, one map**: a map is opened only by the item named in its `Item` cell (for example `t20` opens Shorelines). There
  is no generic map item and no random map; an item no map names is an ordinary item. The item belongs to your mod and needs
  item spell 16 (`pSpell` in `misc.txt`); `cartographer check` names a map whose item is missing, unknown, shared or lacks it.
- **One map per game**: using a map item in Harrogath in Hell is meant to open a red portal, once per game. A refused map
  stays in your inventory; the reason is in the log and the console, and on screen you see "I can't use this yet." Both
  opening in Hell and a refusal (level gate) were seen in a game.
- **Map item modifiers** (rare and unique map items rolled and shown on hover in game, slot 11, 2026-11-12, and a unique map opened with its stats read in the log; the effect on spawned monsters and on the character sheet is not checked with item modifiers): when a map opens, the stats on the item are read (see tables.md, Map item rarity and modifiers) and applied through the game's own stat lists: the monster-side ones to every
  monster that spawns in the map level. The player-side ones go on the player unless you set `PlayerStats` to `0` in
  `cartographer.txt`; it is `1` by default (`0` attaches nothing to the player). The stats apply and are removed when the player leaves the map level (what follows was seen only with the retired random `mapstats.txt` properties, not with item modifiers), and a Save and Exit inside the
  map saved a test character without them (an autosave inside the map with `PlayerStats=1` is untested), and the character screen showed them inside the map and back to normal on leaving (single player, one run, test character; `cartographer check` prints the state it would use). Not checked: joining clients and multiplayer, more than 16 player stats and curses (on a death in the map the plugin forgot the list instead of freeing it, seen once in the outdoor gaps check). Leaving the game removes nothing itself; the plugin forgets them and the game frees them with the
  character. Stats the game writes back into the saved character (`gold`, `hitpoints`, `level`, ...) are refused on players
  (on monsters `level`, `hitpoints`, `mana` and `stamina`; stats that need a skill, state or monster type number, such as `state`, `item_aura` and `item_skillon...`, are refused on both; the list is the Save Param Bits column of the vanilla 3.3.0 `itemstatcost.txt`); `cartographer check` names such rows. Each part is found by byte signature at load; a part that is not found
  once leaves that half off and `cartographer check` says which. The player half needs the `PlayerTick` part. The area columns
  (`Level`, `Density`, `Uniques`, `Monsters`) are written at map open (`Monsters` filled the map in game and the level's fields were put back on leaving; the others written and unit tested, their effect not measured; see section 15 of the reverse-engineering notes); `Champions` is never applied (the game has no field for it; the log says so), and the panel is not applied yet. The item's own stats (a player stat from an item was applied in game once; monster stats from an item not) are read at use time. The map level stat on the item (`cartographer_map_level`) is read there too and falls back to the `maps.txt` Level (fallback seen in game; the stat on a map and the cube recipe not). The plugin log gets `statprobe:` lines
  that record what the game did, for the checks listed in `docs/reverse-engineering/cartographer/07-map-stats-parts.md`.
- **Map events** (Boss and Horde spawns and the game-table names checked in game, with the event distance lowered to 12 in a scratch build for the small test map, the scaled distance itself not yet run in game; the entry banner and the event's name and message were seen on screen in a plugin panel (2026-11-12 banner check; see Tested)): when a map opens, `EventChance` and `mapevents.txt` decide whether it has an event
  (the row is drawn by `Weight`). On arrival in the map level the game's own "Entering" text shows the map's `Name` and a banner shows, for an event that will spawn, the event's `Name` and `Message` (on screen in a plugin panel near the top, else in the console; the Overlay service's text never reaches the screen, see the 2026-11-12 banner check). A `Boss` event
  spawns its `Spawn` (a `monstats.txt` `Id` or a `superuniques.txt` name) at the centre of the event room with `Count`
  minions of its class; a `Horde` spawns `Count` of it. In a Cave or Crypt map the event room is the layout's reserved
  event cell; in other maps it is the first room the game fills that is a third of the level's larger side (at most 24 tiles) from the first room, or the farthest room of the level if that is nearer, so a small map always gets its event and a player who walks the map meets it (if the room list cannot be read, only the third-of-the-side rule is used; seen in game in Cave, Crypt and outdoor maps; #115, #121). The banner names the event only once it has spawned, so it can follow the game's "Entering" text by a moment. A superunique
  can only exist once per game, so a second one spawns as its plain class. The `Spawn` name is looked up in the game's own loaded tables when the map opens (checked in game with a superunique name, Bishibosh; a plain `monstats.txt` Id was not looked up live); if that route fails its self-check, the plugin log says why and the mod's `monstats.txt` and `superuniques.txt` in its excel folder are used instead. With that fallback, a missing `monstats.txt` means no Boss or Horde events and a missing `superuniques.txt` means no superunique names (plain monsters still spawn); `cartographer check` and the report say so. A Boss `Drop` (a treasure class name) is rolled once more when the event's boss dies, and a `Shrine` places the objects.txt row named in `Spawn` (its `Class`, such as `ShrineHealth`; or a `Name` used by one line only; or a row number) at a free spot in the event room; both are unit tested; in game the Boss `Drop` roll ran (what it dropped is not settled) and the Shrine was placed (see Tested). An `Ambush` is announced when its first wave spawns, and spawns `Waves` waves of `Count` x `Spawn` one after another once a player is in the event room (a wave ends when all its monsters died or after a time-out). A `Treasure` whose `Spawn` is a monster spawns it (it wanders, it does not flee), and its death rolls `Drop`; any other `Spawn` is an objects.txt chest, placed like a Shrine, that rolls `Drop` when opened; all unit tested; in game an Ambush ran and the chest was placed; a Treasure chest is always unlocked, and it opened when clicked, but its loot did not drop in one game (#220, not rechecked; see Tested). The room populator and the two spawn
  functions are found by byte signature at load; a part that is not found once turns the spawns off (`cartographer check`
  says which) while the map and its entry message still work. The log gets `eventprobe:` lines for the checks in
  `docs/reverse-engineering/cartographer/09-map-events.md` (the Boss and Horde spawns) and `docs/reverse-engineering/cartographer/13-ambush-treasure-shrine.md` (Shrine, Boss `Drop`, Ambush and Treasure).
- **Console open**: `cartographer open <Name>` opens the named map's portal itself, with no item, through the same gates. It needs
  the `PlayerTick` part (`cartographer check` shows it); without it, or in a game you joined over TCP/IP, the command
  refuses and says so. Opening in town was seen in a game; the TCP/IP refusal was not.
- **Multiplayer map sync, part 1** (written and unit tested; not verified in game, needs a second game instance; #125): when the host opens a map, it tells every joined TCP/IP client which map it is (the `maps.txt` row, the slot level and, for a Cave or Crypt map, the layout seed) over D2RLoader's plugin network. A client connects to the host a few seconds after it enters a game, says hello, checks the row and the map name against its own `maps.txt` and keeps the answer (a host and client with different `maps.txt` files are noticed and logged). Part 2 (#130, also written and unit tested, not verified in game): the client then shows the map's name on its own portal tooltip, automap and banner (unless `MapNames=0` on the client) and, for a Cave or Crypt map, builds the slot level from the host's layout seed with its own `maps.txt` row, so both sides get the same maze; each half is skipped, with the reason in the log, when the client's own `maps.txt`, slots or maze steering do not fit (the name half also needs the maze parts hooked, as it takes its data flag from the level build; otherwise it logs that reason), and the layout is not applied if the client built the slot level before the host's map arrived. Map stats and events stay with the host; the portal hold comes later. It uses no engine address and no hook; if D2RLoader has no network service of the expected version the sync is off and the game runs as before. Single player and a host with no client send nothing, and a process that has the server never connects. A client needs the `PlayerTick` part for the game events it starts from, and the same Cartographer version as the host (the channel token carries the message version). `cartographer check` prints one `map sync` line. Not available on Battle.net.

## Your first map

New to Cartographer? [Your first map](./first-map.md) walks through the item, the
`maps.txt` row and the check. [tables.md](./tables.md) lists every column of every table, and a working
set of tables is in [`examples/first-map/`](../../plugins/cartographer/examples/first-map/) (plain text, no game files).

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
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds the Maze, Outdoor and Snow levels, the `cmap` item type and a `superuniques.txt` row per boss (see the README); the Maze level is one level
   ("Cartographer Maze", copied from a plain game Cave level (level 9), or a Crypt level (18) with `-Tileset Crypt`, so it keeps a name string; `Quest` 0, `LevelGroup` `Null` and no `LevelEntry` or `LevelWarp`, so it is in no vanilla level group)
   and its maze row to the mod's `levels.txt` and `lvlmaze.txt` (backing each up once as `.bak`), and adds the `Maze` row to
   `slots.txt` (other rows are kept). It adds no map items (step 4). Running it again changes nothing once the tables are up to date.
   Rows of an older install ("Cartographer Preset", its `lvlprest.txt` row, the `Preset` slot row) are left alone and
   can be deleted. A "Cartographer Maze" row from an earlier install (copied from the Den of Evil) has its quest flag, level group, entry and warp text, `Vis0` to `Vis7` and name string (tooltip) repaired on re-run. Written and run on a copy of the vanilla 3.3.0 tables only; not run against a game install. D2RLoader compiles the
   mod's `.txt` tables itself at start (no `-txt` needed). If the mod ships
   its own `.bin` table files, the game reads those instead and ignores the edited `.txt`: rebuild the `.bin` files from the
   edited tables, or remove them, before starting.
4. Give each map an item of its own in your mod's `misc.txt`: a new row with item spell 16 (`pSpell`) and type `cmap` (the installer adds that item type), for example a copy of the
   Scroll of Identify with `belt` 0. Put `maps.txt` and, if you like, `cartographer.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`.
5. Start the game through D2RLoader. Check `cartographer-report.txt` in the plugin's log folder.

## Configuration

`maps.txt` has one row per map, with a header row. Only `Name`, `Tileset`, `Shape` and `Item` are needed; a blank cell means the
default. Cartographer's own words (tilesets, shapes, sizes) ignore case; names from the game's tables
(monster ids, boss names) must match exactly; item codes ignore case.

| Column | Meaning | Default |
|---|---|---|
| Name | Unique name | required |
| Tileset | `Cave` or `Crypt` (needs the `Maze` slot below); the other tileset names are accepted but refused until they are built | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop`, `Sprawl` or `Open`, or the name of a `shapes.txt` row (applied). | required |
| Size | `ExtraSmall`, `Small`, `Medium` or `Large` | Medium |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map, and only this map: required, unique across maps, item spell 16 | required |
| Boss, Monsters | A monstats `Id` or superunique name; up to 10 monster ids | none |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| EventChance, Density, Champions, Uniques | Numbers; `EventChance` is used when a map opens; `Level`, `Monsters`, `Density` and `Uniques` are written into the map's level (`Monsters` seen in game, the rest experimental); `Champions` is checked only, never applied | cartographer.txt |

`cartographer.txt` has the columns `Setting` and `Value`: `EventChance` (20), `PlayerStats` (1; `0` turns it off, see above), `MapNames` (1, see below) and
`Town` (`Harrogath`, the only town supported) and `MazeProbe` (0; 1 logs `[maze probe]` lines about the maze the game builds). Phase 1 reads and checks the map columns beyond the table above, a Cave or Crypt
map opens through the `Maze` slot (below), a Jungle or Snow map through the `Outdoor` or `OutdoorSnow` slot (README); nothing opens without a mod and a `maps.txt`. The `Preset` tileset is gone: a `Preset` row is refused with "Preset maps are gone: use Cave or Crypt."
A slot is dropped at load (and named in the report) when its level is one of the original game's (id 137 or below), a town, has no `levels.txt` row, is a `Maze` slot on a level not named `Cartographer Maze`, or repeats another slot's level or name; unit tested, not run in game.

Map names are on by default: a map's own `Name` shows on its portal, the automap and the entry banner; `MapNames=0` turns them off. Seen in single player; multiplayer not verified. A second map opened in the same game shows its name on the first map's portal too.

Engine signatures that a new game build needs can be given in `signatures.txt` (`Part`, `Signature`) beside `maps.txt`
without a rebuild; see `docs/reverse-engineering/cartographer.md` (the index of the per-section files). A hooked part (the two item-spell functions) needs at least the hook size in exact bytes (no `??`) and a function start, or it stays off; a part the plugin only calls needs a unique match only.

## Maze slot (Cave and Crypt, manual setup)

A `Cave` or `Crypt` map opens through a second level, the `Maze` slot, and the plugin builds that level from the map's layout
instead of the game's random maze. **One Maze slot is one tileset**: its level has one `LevelType` in `levels.txt`, 3 (Cave)
or 4 (Crypt), and only maps of that tileset can use it. A map of the other tileset is refused with a plain message when you
try to open it (and named in `cartographer check` and the report); this check never lets a vanilla maze open in its place. A plan
refused later, at build time (for example the slot is too small for the map's size), or `.bin` tables that differ from the
`.txt` files the plugin read, can still give a game-built maze; the log says why. `tools\install.ps1` adds the Maze level
(`-Tileset Crypt` for a Crypt; Cave is the default):

- `slots.txt` is in the mod's tables at `<Mod>.mpq\data\global\excel\d2rloader\cartographer\slots.txt` (the installer writes
  it). It is tab separated with the header `Slot` and `Level`, one row per slot, the level being the `Id` in `levels.txt`:

  ```
  Slot	Level
  Maze	138
  ```

  Running `tools\install.ps1` again keeps it: the installer only adds or updates the `Maze` row and leaves every other row alone.
- `levels.txt`: the installer's new level is in Act 5 with `DrlgType` 1, `LevelType` 3 (Cave) or 4 (Crypt), `SizeX` and `SizeY` 360 in
  all three difficulty columns, `Depend` 0, `Position` 0, `OffsetX` 3500, every `Warp` -1 (0 crashed the game), `Quest` 0, `LevelGroup` `Null`, empty `LevelEntry` and `LevelWarp`, a `LevelName` copied from a
  game level so it has a string, and a matching `lvlmaze.txt` row. The row also keeps the copied level's Act 1 monster list and
  `MonLvl*` values; check in game that the map's own `Monsters` and `Level` settings override them. At load, a `Maze` slot is kept only if
  its `levels.txt` row is named `Cartographer Maze`. Cartographer
  reads this `LevelType` from the mod's `levels.txt` at load; if it cannot, Cave and Crypt maps are refused.
- Restart the game. If a part or the slot is missing, only Cave and Crypt maps are refused; the rest keep working. If the
  plan cannot be built or the engine refuses a step, the game's own walk finishes that maze (with the slot's own size and
  `lvlmaze.txt` row restored first) and the reason is logged.
- The Maze level is built once per game. Opening a second Cave or Crypt map in the same game is refused ("leave the game and
  start a new one").

Run in game for Cave maps and, in a second run, for a Crypt slot (see Tested): the plan, the steered build, the tileset refusal, the fallback and new games. The player did not arrive in the planned entrance cell in the 5 games with an arrival line; the build now marks the planned entrance cell as the level entry; that room gets none of the level's random objects (chests, shrines and the like), which is accepted (written and unit tested, not verified in game; #127). Not run in game: a second map in the same game on the Crypt slot. Hell (arrival, Boss, event) and the Normal and Nightmare refusal were run later ([crypt-difficulty-check](checks/2026-11-12-crypt-difficulty-check.md)).

## Tested

- Game check records, one file per run: [docs/cartographer/checks/](checks/), latest [map item check](checks/2026-11-12-items-check.md).
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
  - the Hell gate refuses with "can only be opened in Hell" (a map used on Normal; when this was run, a map row could still name another difficulty);
  - `cartographer open <Name>` opens the portal with no item in town, and refuses outside town ("only be opened in town");
  - a typo in `maps.txt` gives a plain-English line in `cartographer-report.txt` and in `cartographer check`
    (tileset 'Jungel' is not known, did you mean 'Jungle', that map is skipped);
  - town portal scrolls (blue portal, scroll used) and identify scrolls (ring identified, scroll used) work with Cartographer loaded;
  - (Hell game) a map open in Hell, `Gated` (character level 20): refused at level 1 (log "needs character level 20 (you are 1)", item kept),
    opened at level 25;
  - (Hell game) an item with spell 16 that no map names does nothing and is kept, as with Cartographer unloaded;
  - (Hell game) a town portal scroll used on the map level does nothing, with or without Cartographer (vanilla behaviour on that level);
  - (Hell game) Save and Exit from the map level with the portal open is clean, and the next game opens a map again (the open was not recorded as item or console);
  - (Hell game) Better Auras loaded alongside: both load, the map still opens;
  - map events (2026-11-12, one operator run, Normal, 100 percent chance, the minimum event distance lowered to 12 for the test map): a Boss spawned 5 of 5 in the event room (one superunique, once-per-game bit set), a Horde of 3 spawned, their names come from the game's own tables, and the map's stats reach them; the banner's text was not seen on screen in that run (it was drawn through the Overlay service, which shows nothing; since replaced by a panel, seen in the 2026-11-12 banner check); nothing carries over to a new game. Not run: the txt fallback, a repeated `Id`, a second superunique in one game, the maze event room.
  - phase 6b events (2026-11-12, Normal, one game per event (the Shrine twice), the build of the earlier run but with the #121 event room rule and no lowered event distance; details in `docs/reverse-engineering/cartographer/13-ambush-treasure-shrine.md`): a Boss with a `Drop` spawned and its `Drop` rolled once when it died (item quality not seen); an Ambush ran 2 waves of 3 and ended each wave when its monsters died; a Treasure monster spawned and its flee call ran (that it fled is not confirmed). Not working in that run: a Shrine and a Treasure chest were not placed (the spot found was outside the small event room), and an `objects.txt` name was refused on the stock file (588 lines against the game's 587 rows; its `Expansion` marker line has no row). Both are fixed in the code (the spot is tried from several points inside the room, the marker line is skipped), written and unit tested, and seen working in game in a later run ([Hell checks 4 and 5](checks/2026-11-12-hell-check-4-5.md): Shrine and chests placed, 587 `objects.txt` lines for 587 rows; #135). Boss and Horde names read from `monstats.txt` and `superuniques.txt` when the game's tables are not used no longer pick the next row after the stock files' `Expansion` line (unit tested, not verified in game). The banner text was later seen on screen as a plugin panel ([banner check](checks/2026-11-12-banner-check.md)).
  - (2026-11-12, Normal) the portal tooltip in Harrogath reads "Colossal Summit" (not "Missing String"), with no owner suffix in single player; the entry banner reads "Entering The Colossal Summit", the automap title "Colossal Summit", the return portal "Harrogath"; leaving and entering again works. The map's own `Name` was not yet shown on the portal tooltip, the game's entry banner or the automap in that run (issue #90; later fixed, see `MapNames=1` below); Cartographer's own overlay banner was not looked for in this run; details in `docs/reverse-engineering/cartographer/10-portal-tooltip.md`.
- In game, the maze slot (Cave or Crypt level 139, 360 x 360, OffsetX 6000 when these runs were made (the installer now writes 3500, so a revealed map survives a trip to town: see note 22), same build and tools; log lines checked by hand with `MazeProbe` on, results in `docs/reverse-engineering/cartographer/05-maze-slot.md`):
  - 46 games in a row, each with a new game and a Cave map (`Corridor`, `Winding` and `Loop` in turn): the slot built the planned shape (built room grid equal to the plan), no crash, no error line;
  - a map of the other tileset (`Crypt` on a Cave slot) is refused when opened, and a second map in the same game is refused;
  - a slot too small for the plan (60 x 60) logs the reason and the game's own maze is built, no crash;
  - entering the slot with no map opened builds the game's own maze;
  - (second run, 2026-11-12) 3 Crypt maps (`Branching`, `Corridor`, `Loop`) built as planned with no crash, and a Cave map on a Crypt slot is refused; in 5 games with an arrival line the player did not arrive in the planned entrance cell; every game logged WARN lines that monster map stats were skipped (no `itemstatcost.txt` in the test mod) (tracked in an issue, not fixed);
  - (arrival check, 2026-11-12, build 93847, D2RLoader 1.3.1-beta) in a Cave Corridor, a Cave Loop and a Crypt Branching game the player arrived in the planned entrance cell, and no chest or shrine was seen in that room (details in `docs/reverse-engineering/cartographer/05-maze-slot.md`); Nightmare, Hell and clients were not run;
  - (Hell check, 2026-11-12, origin/main da9deab, D2R build 93847, D2RLoader 1.3.1-beta, one slot, test mod and two test saves, single player, live in game; for Refs #152) a Cave Corridor map with the plain monstats Boss `zombie1` and a second with the superunique `Bishibosh`, no `Difficulty` column, `MazeProbe=1`:
    - (a) opening in Hell passed: using the map item in Harrogath in Hell showed "Cartographer: HellCave is open." and a red portal (log `opening 'HellCave' (Cave) through level 139`, `maze slot level 139: built as planned (16 rooms, entrance placed)`);
    - (b) arrival passed: the player arrived in the entrance cell (`maze arrival: room 139 of 144 (at 6008,8) marked as the level entry, in plan cell (0,0); 0 other room(s) lost the mark`, probe `ARRIVAL IS IN THE ENTRANCE CELL`); the same held for a second map;
    - (c) the map Boss passed: `map Boss 'zombie1': spawned in the boss room (30480,720) 40 x 40 (as a plain monster)` appeared once, when the player went near the far room; one monster, no minions (the code spawns none); the monsters around it were killed with a console command, and which corpse was the boss was not confirmed on screen;
    - superunique `Bishibosh` as the map Boss: in the first game the spawn was refused by the game (`superunique row 0 ... returned 0000000000000000`, `map Boss 'Bishibosh': refused by the game ... (as a plain monster)`, nothing appeared); in a second, fresh game it spawned (`returned 000000097EFBBC20`, `spawned in the boss room (30000,960) 40 x 40 (as a superunique)`) with the game's own Bishibosh minions around it, and it was killed. The cause of the first refusal was not found (open, one of two games);
    - (d) the Normal refusal passed: the same item on a Normal character logged "Cartographer: HellCave can only be opened in Hell." and showed "I can't use this yet.";
    - not checked: a second map in the same game, TCP/IP, and a natural (non-console) walk to the boss room (Nightmare was refused later, see the next entry).
- Later Hell checks, single player, test saves (2026-11-12; builds, log lines and limits in [checks/](checks/)): the map Boss, now its own `bosses.txt` entry, spawned in the boss room as a superunique in 6 of 6 games in Cave and Crypt ([hell-check-3](checks/2026-11-12-hell-check-3.md)), with its minions and its name on hover ([hell-check-2](checks/2026-11-12-hell-check-2.md), [names-drop-check](checks/2026-11-12-names-drop-check.md)); a Crypt map opens in Hell and arrives in the entrance cell, and the same item is refused in Normal and Nightmare and kept ([crypt-difficulty-check](checks/2026-11-12-crypt-difficulty-check.md)); Jungle and Snow open from their items with arrival, Boss, map stats and the entry banner ([outdoor-check-2](checks/2026-11-12-outdoor-check-2.md), [outdoor-check-3](checks/2026-11-12-outdoor-check-3.md), [snow-check-1](checks/2026-11-12-snow-check-1.md), [items-check](checks/2026-11-12-items-check.md), [outdoor-gaps-check](checks/2026-11-12-outdoor-gaps-check.md)); Horde 6 of 6 in Cave, Jungle and Snow (snow-check-2); the Boss `Drop` roll fires on death (plain Boss 3 of 3, superunique 2 of 2) but what it dropped is not settled ([boss-drop-check](checks/2026-11-12-boss-drop-check.md)); an unlocked Treasure chest opens but its loot did not drop in one game (#220). Latin-1 names on hover: a Latin-1 file showed `*` before the conversion, and the accents were right after it (PR #261). The Snow Horde was 6 of 6, walked to ([snow-check-2](checks/2026-11-12-snow-check-2.md)). A Snow wall stopped the character and the Snow automap title was seen (snow-check-2); not settled: Snow corners. Siege (Bloody Foothills) opened from its item in two games, arrival, the Boss and a Treasure event ([siege-check](checks/2026-11-12-siege-check.md)); not settled: its north and south edges and the piece seams.
- Open shape, single player, test save (2026-11-12, [open-check](checks/2026-11-12-open-check.md)): Crypt Large Open (144 rooms, two seeds) and Jungle Large Open (2304 rooms) built and were entered in Hell without a crash, with the arrival in the entrance cell and, on Crypt, the boss in the boss room. Not checked: Cave Open, a full walk of a Large map, Snow refusal live. One unexplained `BC_VERIFY ptRoomFirst` line followed two invalid `cheat goto` calls once.
- Not reproduced in game: the "no free spot" refusal (tried with 100 zombies around the player; the spot search still
  succeeded; the collision-bit reading is in `docs/reverse-engineering/cartographer/03-phase1-parts.md`, and a refusal needs a
  forced null or a fully blocked area), and the TCP/IP client refusal (needs a second game instance).
- Map item modifiers: unit tests of the item-stat-to-plan mapping and the apply plan (`statplan_tests`), and the six stat parts' signatures, each
  unique and resolving to its recorded address in a dump of build 93847 (`check_portal_signatures.py`). In game (same
  build and test setup, 2026-11-12; details in `docs/reverse-engineering/cartographer/07-map-stats-parts.md`): monster stats
  apply to monsters spawned in the map level and not in town (life doubled, fire resistance, seen in the overlay);
  `PlayerStats=1` applied strength and cold resistance on the server, removed them on leaving the map and the game, and
  a Save and Exit inside the map left the test character unchanged; a second run showed strength and cold resistance on the
  character sheet inside the map and back to normal after leaving (single player, one run, test character). **Not seen in game**: the monsters' own damage and life bar as played, natural (non-cheat) spawns, and
  guards failing open (no mismatching build to test).
- Seen in game: `core-maps` (pd2-resurrected) hooks the same item function, so it fails to install its own hook when
  loaded next to Cartographer; run one or the other (see Requirements).
- Expected, unit tested only and not verified in game: the spell-16 use with no matching check, whose code names a map, is
  dropped (it opens nothing and is not passed to the game). The game's only direct caller of the use handler runs it only
  after the check passed (static reading), so this case is defensive and no in-game test is planned.
- `MapNames=1` in game (2026-11-12, build 93847, D2RLoader 1.3.1-beta, test mod and test save, Normal, single player, two games; log lines and screenshots checked by hand): the map's own name showed on the portal tooltip, the automap title and the entry banner, with no "Missing String", and again in the second game. Details and limits in `docs/reverse-engineering/cartographer/10-portal-tooltip.md`. The log lines quoted there were not kept as evidence. Not checked: TCP/IP, other languages, a second map in the same game, a long name. Later runs saw the Cave, Jungle and Snow entry banners and the Jungle automap title ([items-check](checks/2026-11-12-items-check.md), [outdoor-gaps-check](checks/2026-11-12-outdoor-gaps-check.md)).
- Map sync part 1: unit tests of the message, its validation, the client's check, the client list and the connect schedule (`mapsync_tests`). No game run: the connection, the delivery, the late join and the thread each callback runs on are the checks in `docs/reverse-engineering/cartographer/12-map-sync.md` ("Needs game").
- Not yet in game for `cartographer open <Name>`: repeating with an aura that reserves mana; leaving the game with the portal open and using the console in the next game; as a TCP/IP client the command refuses.

## Staging a game check (`stage_check.ps1`)

`plugins\cartographer\tools\stage_check.ps1 -Slot N -Owner <lane> -Dll <cartographer.dll> [-Tables <dir>] [-Saves <dir>] [-AutoCheckMap <name>] [-MapItems]`
sets up one test slot for a check. Only slots 5, 6, 7 and 11 are accepted (the ones approved for Cartographer checks); slot 1 and every other slot are refused.
Before it locks anything it checks the inputs, `install.ps1`, the vanilla tables, that no game runs in the slot, that every write target is outside the owner's own install,
and that `-Saves` is not inside the owner's real save folders (the vanilla `Saved Games\Diablo II Resurrected` root and every mod folder under it, junctions resolved): use a scratch folder of test saves.
Then it locks the slot (d2rAI-toolkit `game_slots.py lock`, never `--force`), checks again that no game runs, writes the slot's test mod `modinfo.json` (with the slot's own `savepath`) and seeds its excel folder
with copies of the vanilla tables `install.ps1` needs (`levels`, `lvlmaze`, `superuniques`, `monstats`, `monumod`, and `itemstatcost`, without which the map stats on monsters are skipped; `-VanillaExcel` names the source, which is only read; the copies get the read-only attribute cleared, as does every file the script places),
seeds `d2rloader\core-ui\automap.txt` (the PD2 `core-ui.dll` plugin reads it and the game asserts at startup without it; the seed only matters with `-KeepPlugins`, since the default parks that plugin, and it is harmless otherwise; the seed is the header plus one row for level type 36, since Cartographer adds no automap level type; a header-only file made the game assert at startup (issue #219) and is not seeded. A seeded file with no data row, or any file this script placed earlier when `-CoreUiAutomap <file>` is given, is replaced; a file the slot has that the script did not place stays, with a warning if it has no row. `-CoreUiAutomap <file>` copies a file you name instead, never one from slot 1 or another mod), copies the DLL, the Cartographer tables and the test saves, runs `install.ps1` on the test mod's excel folder and, with `-AutoCheckMap`, writes `AutoCheck=1` and `AutoCheckMap` into the slot's `cartographer.txt`
(the settings of #176). Later runs copy only the files whose SHA-256 differs.
The script keeps a manifest, `stage_check.json` in the slot's test-mod folder: every file it placed (path and SHA-256, taken at the end of staging), the settings rows it wrote, the JSON entries it added to files the slot already had and the slot's old `default_mod`.
The slot's `d2rloader\config\d2rloader.toml` may say `default_mod = "PD2Resurrected"`, which makes the game load another mod: the script sets it to the test mod and `-Restore` writes the old value back (only if the line still names the test mod).
A seeded `levels.txt` is vanilla; one that is already in the slot may not be: Slot11's has an extra row, `Act 5 - Cartographer Spike` (id 138, from #168), so the Maze level gets id 139 there.
The script warns, naming the row, when `levels.txt` has a row past the vanilla rows that is not a Cartographer row.
If a step fails after the lock, the error is printed with the exact `-Restore` command (the script's absolute path) and a note that the slot is still locked.
Every other `.dll` in the slot's `d2rloader\plugins` (all but the staged DLL and `test-harness.dll`) is moved to `plugins-off\staged-<owner>\` and put back by `-Restore`; `-KeepPlugins` skips it. A leftover PD2 `core-rules`/`core-maps`/`core-ui` makes every character save fail (`encoding-error=yes`, after `BC_ASSERT nStat` at load); see [2026-11-12-save-encoding](checks/2026-11-12-save-encoding.md). `test-harness.dll` (the game_ctl click and console commands) is copied from `-HarnessDll` (default `..\d2r-test-harness\build\Release\test-harness.dll`) when the slot has none, recorded in the manifest and removed by `-Restore`; a slot's own copy is left alone.
It prints the launch command and the log path (`<slot>\d2rloader\logs\cartographer.log`) and never starts the game. `-WhatIf` prints every action and changes nothing.
`-Restore` first checks that `-Owner` holds the slot's lock and refuses otherwise (and with no manifest), before it touches anything. Then it runs the toolkit's `release_install.py restore` when the slot holds a release install,
removes only the files whose SHA-256 still matches the manifest (a changed file stays, with a warning) and only the `AutoCheck` rows it wrote, deletes the manifest and unlocks the slot.
Changes `install.ps1` made inside a table that was already in the slot (not seeded by this script) stay.
It reads paths from the toolkit config (`MODKIT_TESTING_CONFIG`, default `C:\D2RTestSlots\modkit-testing.toml`). Use a different `-Owner` per lane: a second run with the same owner counts as "already ours".

## Map items in a test slot (`-MapItems`)

A map item that has no icon or tooltip, does nothing on right-click and makes the save fail ("Character save failed ... encoding-error=yes") was seen in the outdoor check (items `cm0` to `cm2`, cloned from the Rejuvenation Potion `rvs` with spell 16). Read statically, against the vanilla 3.3.0 tables and the toolkit's extracted data (nothing was run in a game), the causes are:

- **Two of the three codes were vanilla items.** `cm1` and `cm2` are the Small and Large Charm in `misc.txt`; a second row with the same code is not a new item. Pick codes that no `misc.txt`, `weapons.txt` or `armor.txt` row uses; `t20`, `t21` and `t22` are free in all three.
- **No picture entry.** The HD client looks an item's icon up in `data\hd\items\items.json` by code. A new code with no entry has no icon, and the toolkit's notes (`knowledge/hd-graphics.md`) record the client asking for `[default_unit]` every frame and then crashing. The mod's `items.json` replaces the game's whole file, so it has to be a full copy plus the new entries.
- **The wrong base item.** `rvs` is a potion (type `rpot`, `autobelt` 1, `stat1`/`stat2` hitpoints and mana, `spelldesc` 4). Spell 16 on top of that gives a row that is half a potion. The first example rows cloned the Scroll of Identify (`isc`, type `scro`, no stats), which fixed that but not the name (next point); they keep that base but with `type` `cmap` (the Map item type `install.ps1` adds to `itemtypes.txt`), `belt` 0, with `spawnable` 0 and the vendor columns blank so no NPC sells them.
- **No name string.** `namestr` is a key in `item-names.json` (also replaced as a whole file, in `strings` and `strings-legacy`). The `rvs` clones borrowed the potion's key, so they had a name, but a row with its own `namestr` needs its own record.
- **The game names a scroll or tome by its item spell (seen in game, 2026-11-12, slot 6, Hell).** Rows cloned from `isc` with `pSpell` 16 showed "Scroll of Town Portal" on the tooltip (issue #218), with `MapNames` on or off; the same row with `pSpell` 1 showed "Scroll of Identify". The same row with `type` changed to `herb` (an existing non-quest type) showed its own `namestr` ("Shorelines Map", "Tunneling Depths Map", "Overgrown Reach Map"). Opening needs no code: the plugin acts on `pSpell` 16 and the item code, not on the item type. The owner chose a type of its own (`cmap`, see [tables.md](tables.md#map-item-type-itemtypestxt)) over `herb`.
- **The save failure was not (only) these.** A slot holding leftover PD2 `core-*` plugins failed every save with no Cartographer and no items in it, and saved fine without them ([2026-11-12-save-encoding](checks/2026-11-12-save-encoding.md)); `stage_check.ps1` now parks them. The log gives only `native-result=1 encoding-error=yes` for the character writer. The duplicate charm codes are the likeliest cause (an item whose code resolves to a row of another type is written with the wrong extra bits), but this was not shown. Cheat-spawned items can also be unsaveable (`savecheatitems`). The verification below separates the two.

The supported path is `stage_check.ps1 -MapItems` with the overrides in `plugins\cartographer\examples\first-map\map-items.txt` (`t20`, `t21`, `t22`, matching the example `maps.txt`; the file holds only `code`, `name`, `base` and the columns to change, no game data). The switch stages, in the slot's test mod only (a player's install gets none of these files; `-Tables` skips `map-items.txt`, which is not a game table, so it never lands in the Cartographer folder):

- `misc.txt` (seeded from the vanilla table if the slot lacks it) plus the three rows. Each row is the `base` row (`isc`) read from the slot's `misc.txt` (or the vanilla one) with `type`, `pSpell`, `rarity`, `spawnable`, `PermStoreItem`, `multibuy`, `namestr` and `name` set by column name and every vendor `Min`/`Max` column blank, so any column order works; a missing base item stops the script, and a code that is already there is left alone;
- `data\hd\items\items.json`, seeded from `-VanillaData` (default `<toolkit>\d2r-data\93847\data`), plus, per code, the picture entry of its base item, for example `{ "t20": { "asset": "scroll/identify_scroll" } }` and the same for `t21`, `t22`;
- `data\local\lng\strings\item-names.json` and `strings-legacy\item-names.json`, likewise seeded and extended with a record per code (ids as the toolkit's `knowledge/strings.md` says: counting up from 28106 (or the vanilla `next_string_id.txt`), skipping every low-16 value already used in any `*.json` of the mod's and the vanilla `strings` and `strings-legacy` folders, and never above 65535, where the script stops with an error; with the vanilla data that is 28106 to 28108; the `name` text in every language: 13 for `strings`, the 8 that vanilla legacy records carry for `strings-legacy`). An empty `[]` file is handled, and `-WhatIf` lists the entries it would add even when the slot lacks the files.

The files it creates are in the manifest, so `-Restore` removes them. For an `items.json` or `item-names.json` the slot already had, the script records each entry it added in the manifest and `-Restore` takes exactly those entries out again. Staging was exercised twice on uniquely named scratch folders (a fresh mod, and a mod with a scrambled-column `misc.txt`, empty `[]` JSON files and a higher id in another string file): `-WhatIf` listing the entries, a real run, and a second run that changed nothing; JSON valid, BOM and line endings kept, 172 fields per row. It was not run against a slot for real.

To get an item into the test character: start the game, open the console, `debug_mode`, then `cheat spawn t20,inv 0` (`cheat spawn <code>,inv 0` puts it in the inventory; without `inv 0` or room it drops on the ground; see the toolkit's `knowledge/cheat-commands.md`). Spawn it again after every reload, since cheat-spawned items may not be kept in the save.

Exact game steps to verify (needs a game, test slot and test save only):

1. Stage with `-MapItems -Tables <the example pack> -AutoCheckMap` as needed, start the game, `cartographer check`: expect no "item ... is not in the item tables" and no "needs item spell 16" for `t20`, `t21`, `t22`.
2. `debug_mode`, `cheat spawn t20,inv 0`. Expect a scroll icon in the inventory, the tooltip "Shorelines Map", and no `[default_unit]` lines in `blz-log.txt`.
3. In Harrogath on a Hell character, right-click the item: the portal opens, or a refusal with the reason (log and "I can't use this yet"), and the item stays on a refusal.
4. Save and Exit, then read `d2rloader.log` for "Character save failed". If it appears with the codes now unique and the scroll base, the cause is the cheat-spawned item; repeat once with `cheat savecheatitems 1` (or the Cheat Settings toggle) and record both results.

Verified in game (2026-11-12, slot 6, Hell, single player, test character, `-MapItems` rows with `type` `cmap`, the `Map` type added by `install.ps1`): each of `t20`, `t21`, `t22` showed its own name on hover before use ("Shorelines Map", "Tunneling Depths Map", "Overgrown Reach Map"). A right-click on the Shorelines item removed it and opened a red portal, with the log lines `opening 'Shorelines' (Cave) through level 138` and `Shorelines is open`. After Save and Exit and a reload the Overgrown Reach item answered with its own name ("Overgrown Reach cannot be opened: a map has already been opened in this game"), the once-per-game rule; the Tunneling Depths and Overgrown Reach portals were not opened in this run. Save and Exit worked with no `Character save failed` and no `BC_ASSERT`, and the character reloaded, with the PD2 `core-maps`, `core-rules` and `core-ui` DLLs moved out of the slot's plugins folder for the run (they cause the save failure, not these rows). Later in game ([map-item-type-check](checks/2026-11-12-map-item-type-check.md)): a map item could not be placed in the belt (the base 4-slot belt), Malah's three tabs sold none, and a Shorelines Map sold to him for 40 gold with no plugin error; Save and Exit with a map item in the inventory kept it. Not checked: a belt with more slots, the other vendors, and the map items' own inventory art (they show the scroll picture).
