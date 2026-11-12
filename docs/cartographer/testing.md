# Cartographer: testing log and developer notes

This is the developer-facing detail that used to be in `plugins/cartographer/README.md`: what was unit tested, what
was run in game (build 93847, D2RLoader 1.3.1-beta), what was not, and the long per-feature behaviour notes. Addresses
and signatures are in [`../reverse-engineering/cartographer.md`](../reverse-engineering/cartographer.md). The
player-facing summary is the plugin README. Relative links below were adjusted for this folder.

---

A D2RLoader plugin for Diablo II: Resurrected that opens a map from the map's own item: in Harrogath, in Hell, a red portal
leads to a level built from your `maps.txt`. MIT licensed.

**Status: phase 1 (not released); the maze steering for Cave and Crypt maps is written: its steering decisions are unit tested, and the hooks were run in game for Cave and Crypt maps (see Tested).** The table loader, the gates, the items and the red portal
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
- **Map properties** (monsters checked in game on cheat-spawned ones only; the player's stats showed on the character sheet in single player, from one run on the test character): when a map opens, `Properties` random properties are drawn
  from `mapstats.txt` (or the built-in list) and applied through the game's own stat lists: `Monsters` rows to every
  monster that spawns in the map level. `Players` rows go on the player only if you set `PlayerStats` to `1` in
  `cartographer.txt`; it is `0` by default and then nothing is attached to the player. **Turn `PlayerStats` on only on a
  test character**: the stats apply and are removed when the player leaves the map level, and a Save and Exit inside the
  map saved a test character without them (an autosave inside the map with `PlayerStats=1` is untested), and the character screen showed them inside the map and back to normal on leaving (single player, one run, test character; `cartographer check` prints the state it would use or, if none, why the closest rows were refused). Not checked: joining clients and multiplayer, more than 16 player stats, death and curses, and a save with the stats attached where the save writer is confirmed to run. Leaving the game removes nothing itself; the plugin forgets them and the game frees them with the
  character. Stats the game writes back into the saved character (`gold`, `hitpoints`, `level`, ...) are refused on players
  (on monsters `level`, `hitpoints`, `mana` and `stamina`; stats that need a skill, state or monster type number, such as `state`, `item_aura` and `item_skillon...`, are refused on both; the list is the Save Param Bits column of the vanilla 3.3.0 `itemstatcost.txt`); `cartographer check` names such rows. Each part is found by byte signature at load; a part that is not found
  once leaves that half off and `cartographer check` says which. The player half needs the `PlayerTick` part. Area columns
  (`Level`, `Density`, ...) and the panel are not applied yet. `ItemStats=1` (the item's own stats, unit tested, not run in a game) is read at use time. The plugin log gets `statprobe:` lines
  that record what the game did, for the checks listed in `docs/reverse-engineering/cartographer.md` section 7.
- **Map events** (Boss and Horde spawns and the game-table names checked in game, with the event distance lowered to 12 in a scratch build for the small test map, the scaled distance itself not yet run in game; the on-screen banner text was not seen; see Tested): when a map opens, `EventChance` and `mapevents.txt` decide whether it has an event
  (the row is drawn by `Weight`). On arrival in the map level a banner shows the map's `Name` and, for an event that will
  spawn, the event's `Name` and `Message` (on screen through D2RLoader's overlay, else in the console). A `Boss` event
  spawns its `Spawn` (a `monstats.txt` `Id` or a `superuniques.txt` name) at the centre of the event room with `Count`
  minions of its class; a `Horde` spawns `Count` of it. In a Cave or Crypt map the event room is the layout's reserved
  event cell; in other maps it is the first room the game fills that is a third of the level's larger side (at most 24 tiles) from the first room, or the farthest room of the level if that is nearer, so a small map always gets its event and a player who walks the map meets it (if the room list cannot be read, only the third-of-the-side rule is used; written, not verified in game; #115, #121). The banner names the event only once it has spawned, so it can follow the map's name by a moment. A superunique
  can only exist once per game, so a second one spawns as its plain class. The `Spawn` name is looked up in the game's own loaded tables when the map opens (checked in game with a superunique name, Bishibosh; a plain `monstats.txt` Id was not looked up live); if that route fails its self-check, the plugin log says why and the mod's `monstats.txt` and `superuniques.txt` in its excel folder are used instead. With that fallback, a missing `monstats.txt` means no Boss or Horde events and a missing `superuniques.txt` means no superunique names (plain monsters still spawn); `cartographer check` and the report say so. A Boss `Drop` (a treasure class name) is rolled once more when the event's boss dies, and a `Shrine` places the objects.txt row named in `Spawn` (its `Class`, such as `ShrineHealth`; or a `Name` used by one line only; or a row number) at a free spot in the event room; both are unit tested; in game the Boss `Drop` ran and the Shrine was not placed (see Tested). An `Ambush` is announced when its first wave spawns, and spawns `Waves` waves of `Count` x `Spawn` one after another once a player is in the event room (a wave ends when all its monsters died or after a time-out). A `Treasure` whose `Spawn` is a monster spawns it and makes it flee, and its death rolls `Drop`; any other `Spawn` is an objects.txt chest, placed like a Shrine, that rolls `Drop` when opened; all unit tested; in game an Ambush and the flee call ran, the chest was not placed (see Tested). The room populator and the two spawn
  functions are found by byte signature at load; a part that is not found once turns the spawns off (`cartographer check`
  says which) while the map and its entry message still work. The log gets `eventprobe:` lines for the checks in
  `docs/reverse-engineering/cartographer.md` section 9 (the Boss and Horde spawns) and section 13 (Shrine, Boss `Drop`, Ambush and Treasure).
- **Console open**: `cartographer open <Name>` opens the named map's portal itself, with no item, through the same gates. It needs
  the `PlayerTick` part (`cartographer check` shows it); without it, or in a game you joined over TCP/IP, the command
  refuses and says so. Opening in town was seen in a game; the TCP/IP refusal was not.
- **Multiplayer map sync, part 1** (written and unit tested; not verified in game, needs a second game instance; #125): when the host opens a map, it tells every joined TCP/IP client which map it is (the `maps.txt` row, the slot level and, for a Cave or Crypt map, the layout seed) over D2RLoader's plugin network. A client connects to the host a few seconds after it enters a game, says hello, checks the row and the map name against its own `maps.txt` and keeps the answer (a host and client with different `maps.txt` files are noticed and logged). Part 2 (#130, also written and unit tested, not verified in game): the client then shows the map's name on its own portal tooltip, automap and banner (it needs `MapNames=1` on the client as well) and, for a Cave or Crypt map, builds the slot level from the host's layout seed with its own `maps.txt` row, so both sides get the same maze; each half is skipped, with the reason in the log, when the client's own `maps.txt`, slots or maze steering do not fit (the name half also needs the maze parts hooked, as it takes its data flag from the level build; otherwise it logs that reason), and the layout is not applied if the client built the slot level before the host's map arrived. Map stats and events stay with the host; the portal hold comes later. It uses no engine address and no hook; if D2RLoader has no network service of the expected version the sync is off and the game runs as before. Single player and a host with no client send nothing, and a process that has the server never connects. A client needs the `PlayerTick` part for the game events it starts from, and the same Cartographer version as the host (the channel token carries the message version). `cartographer check` prints one `map sync` line. Not available on Battle.net.

## Your first map

New to Cartographer? [Your first map in 5 minutes](./first-map.md) walks through the item, the
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
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`. It adds only what the slot level needs: one level
   ("Cartographer Preset", a copy of Colossal Summit that keeps its name string, so a map portal's tooltip reads
   "Colossal Summit" instead of "Missing String" (checked in game)) and its level preset to the mod's `levels.txt` and
   `lvlprest.txt` (backing each up once as `.bak`), and adds the `Preset` row to `slots.txt` (other rows are kept). It adds no
   items. Running it again changes nothing once the tables are up to date; over an older install it repairs the slot level's
   name string. In game the automap title reads "Colossal Summit" and the entry banner "Entering The Colossal Summit". D2RLoader compiles the
   mod's `.txt` tables itself at start (no `-txt` needed). If the mod ships
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
| Tileset | `Preset`, `Cave` or `Crypt` (maze, needs the `Maze` slot below); the other tileset names are accepted but refused until they are built | required |
| Shape | `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl`, or the name of a `shapes.txt` row (checked only; not applied in game yet). Not used by `Preset`. | required |
| Size | `Small`, `Medium` or `Large` | Medium |
| Level | Monster level, 1 to 255 | 85 |
| Item | Item code that opens this map, and only this map: required, unique across maps, item spell 16 | required |
| Boss, Monsters | A monstats `Id` or superunique name; up to 10 monster ids | none |
| MinCharLevel | Character level needed, 1 to 99 | 1 |
| Properties, EventChance, Density, Champions, Uniques | Numbers; `Properties` is used when a map opens (checked in game for monsters), the rest are kept for later phases | cartographer.txt |

`cartographer.txt` has the columns `Setting` and `Value`: `Properties` (6), `EventChance` (20), `ItemStats` (0), `PlayerStats` (0; test slot only, see above), `MapNames` (0, see below) and
`Town` (`Harrogath`, the only town supported) and `MazeProbe` (0; 1 logs `[maze probe]` lines about the maze the game builds). Phase 1 reads and checks the map columns beyond the table above, but a
`Preset` map opens through the one map level (the `Preset` slot) and a `Cave` or `Crypt` map through the `Maze` slot (below); nothing opens without a mod and a `maps.txt`.

`MapNames=1` shows a map's own `Name` on its portal, the automap and the entry banner. It is off by default and not verified in game. A second map opened in the same game shows its name on the first map's portal too.

Engine signatures that a new game build needs can be given in `signatures.txt` (`Part`, `Signature`) beside `maps.txt`
without a rebuild; see `docs/reverse-engineering/cartographer.md`. A hooked part (the two item-spell functions) needs at least the hook size in exact bytes (no `??`) and a function start, or it stays off; a part the plugin only calls needs a unique match only.

## Maze slot (Cave and Crypt, manual setup)

A `Cave` or `Crypt` map opens through a second level, the `Maze` slot, and the plugin builds that level from the map's layout
instead of the game's random maze. **One Maze slot is one tileset**: its level has one `LevelType` in `levels.txt`, 3 (Cave)
or 4 (Crypt), and only maps of that tileset can use it. A map of the other tileset is refused with a plain message when you
try to open it (and named in `cartographer check` and the report); this check never lets a vanilla maze open in its place. A plan
refused later, at build time (for example the slot is too small for the map's size), or `.bin` tables that differ from the
`.txt` files the plugin read, can still give a game-built maze; the log says why. `tools\install.ps1`
does not add the Maze level; add it by hand:

- `slots.txt` is in the mod's tables at `<Mod>.mpq\data\global\excel\d2rloader\cartographer\slots.txt` (the installer creates
  it). It is tab separated with the header `Slot` and `Level`, one row per slot, the level being the `Id` in `levels.txt`:

  ```
  Slot	Level
  Preset	138
  Maze	139
  ```

  Add the `Maze` row yourself with your new level's id, and keep a `LevelName` that has a string (for example the copied level's) so a map portal's tooltip does not read "Missing String". Running `tools\install.ps1` again keeps it: the installer only adds or
  updates the `Preset` row and leaves every other row alone.
- `levels.txt`: a new level in Act 5 with `DrlgType` 1, `LevelType` 3 (Cave) or 4 (Crypt), `SizeX` and `SizeY` up to 360 in
  all three difficulty columns, `Depend` 0, `Position` 0, `OffsetX` about 6000, and a matching `lvlmaze.txt` row. Cartographer
  reads this `LevelType` from the mod's `levels.txt` at load; if it cannot, Cave and Crypt maps are refused.
- Restart the game. If a part or the slot is missing, only Cave and Crypt maps are refused; the rest keep working. If the
  plan cannot be built or the engine refuses a step, the game's own walk finishes that maze (with the slot's own size and
  `lvlmaze.txt` row restored first) and the reason is logged.
- The Maze level is built once per game. Opening a second Cave or Crypt map in the same game is refused ("leave the game and
  start a new one").

Run in game for Cave maps and, in a second run, for a Crypt slot (see Tested): the plan, the steered build, the tileset refusal, the fallback and new games. The player did not arrive in the planned entrance cell in the 5 games with an arrival line; the build now marks the planned entrance cell as the level entry; that room gets none of the level's random objects (chests, shrines and the like), which is accepted (written and unit tested, not verified in game; #127). Not run in game: Nightmare and Hell, and a second map in the same game on the Crypt slot.

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
  - map events (2026-11-12, one operator run, Normal, 100 percent chance, the minimum event distance lowered to 12 for the test map): a Boss spawned 5 of 5 in the event room (one superunique, once-per-game bit set), a Horde of 3 spawned, their names come from the game's own tables, and the map's stats reach them; the banner is logged as drawn but its text was not seen on screen; nothing carries over to a new game. Not run: the txt fallback, a repeated `Id`, a second superunique in one game, the maze event room.
  - phase 6b events (2026-11-12, Normal, one game per event (the Shrine twice), the build of the earlier run but with the #121 event room rule and no lowered event distance; details in `docs/reverse-engineering/cartographer.md` section 13): a Boss with a `Drop` spawned and its `Drop` rolled once when it died (item quality not seen); an Ambush ran 2 waves of 3 and ended each wave when its monsters died; a Treasure monster spawned and its flee call ran (that it fled is not confirmed). Not working in that run: a Shrine and a Treasure chest were not placed (the spot found was outside the small event room), and an `objects.txt` name was refused on the stock file (588 lines against the game's 587 rows; its `Expansion` marker line has no row). Both are fixed in the code (the spot is tried from several points inside the room, the marker line is skipped), written and unit tested, **not verified in game** (#135). Boss and Horde names read from `monstats.txt` and `superuniques.txt` when the game's tables are not used no longer pick the next row after the stock files' `Expansion` line (unit tested, not verified in game). The on-screen banner text was not seen.
  - (2026-11-12, Normal) the portal tooltip in Harrogath reads "Colossal Summit" (not "Missing String"), with no owner suffix in single player; the entry banner reads "Entering The Colossal Summit", the automap title "Colossal Summit", the return portal "Harrogath"; leaving and entering again works. The map's own `Name` is not shown on the portal tooltip, the game's entry banner or the automap (issue #90); Cartographer's own overlay banner was not looked for in this run; details in `docs/reverse-engineering/cartographer.md` section 10.
- In game, the maze slot (Cave or Crypt level 139, 360 x 360, OffsetX 6000, same build and tools; log lines checked by hand with `MazeProbe` on, results in `docs/reverse-engineering/cartographer.md` section 5):
  - 46 games in a row, each with a new game and a Cave map (`Corridor`, `Winding` and `Loop` in turn): the slot built the planned shape (built room grid equal to the plan), no crash, no error line;
  - a map of the other tileset (`Crypt` on a Cave slot) is refused when opened, and a second map in the same game is refused;
  - a slot too small for the plan (60 x 60) logs the reason and the game's own maze is built, no crash;
  - entering the slot with no map opened builds the game's own maze;
  - (second run, 2026-11-12) 3 Crypt maps (`Branching`, `Corridor`, `Loop`) built as planned with no crash, and a Cave map on a Crypt slot is refused; in 5 games with an arrival line the player did not arrive in the planned entrance cell; every game logged WARN lines that monster map stats were skipped (no `itemstatcost.txt` in the test mod) (tracked in an issue, not fixed);
  - (arrival check, 2026-11-12, build 93847, D2RLoader 1.3.1-beta) in a Cave Corridor, a Cave Loop and a Crypt Branching game the player arrived in the planned entrance cell, and no chest or shrine was seen in that room (details in `docs/reverse-engineering/cartographer.md` section 5); Nightmare, Hell and clients were not run;
  - (Hell check, 2026-11-12, origin/main da9deab, D2R build 93847, D2RLoader 1.3.1-beta, one slot, test mod and two test saves, single player, live in game; for Refs #152) a Cave Corridor map with the plain monstats Boss `zombie1` and a second with the superunique `Bishibosh`, no `Difficulty` column, `MazeProbe=1`:
    - (a) opening in Hell passed: using the map item in Harrogath in Hell showed "Cartographer: HellCave is open." and a red portal (log `opening 'HellCave' (Cave) through level 139`, `maze slot level 139: built as planned (16 rooms, entrance placed)`);
    - (b) arrival passed: the player arrived in the entrance cell (`maze arrival: room 139 of 144 (at 6008,8) marked as the level entry, in plan cell (0,0); 0 other room(s) lost the mark`, probe `ARRIVAL IS IN THE ENTRANCE CELL`); the same held for a second map;
    - (c) the map Boss passed: `map Boss 'zombie1': spawned in the boss room (30480,720) 40 x 40 (as a plain monster)` appeared once, when the player went near the far room; one monster, no minions (the code spawns none); the monsters around it were killed with a console command, and which corpse was the boss was not confirmed on screen;
    - superunique `Bishibosh` as the map Boss: in the first game the spawn was refused by the game (`superunique row 0 ... returned 0000000000000000`, `map Boss 'Bishibosh': refused by the game ... (as a plain monster)`, nothing appeared); in a second, fresh game it spawned (`returned 000000097EFBBC20`, `spawned in the boss room (30000,960) 40 x 40 (as a superunique)`) with the game's own Bishibosh minions around it, and it was killed. The cause of the first refusal was not found (open, one of two games);
    - (d) the Normal refusal passed: the same item on a Normal character logged "Cartographer: HellCave can only be opened in Hell." and showed "I can't use this yet.";
    - not checked: Nightmare, a second map in the same game, TCP/IP, and a natural (non-console) walk to the boss room.
- Not reproduced in game: the "no free spot" refusal (tried with 100 zombies around the player; the spot search still
  succeeded; the collision-bit reading is in `docs/reverse-engineering/cartographer.md`, section 3, and a refusal needs a
  forced null or a fully blocked area), and the TCP/IP client refusal (needs a second game instance).
- Map properties: unit tests of the draw and the apply plan (`statplan_tests`), and the six stat parts' signatures, each
  unique and resolving to its recorded address in a dump of build 93847 (`check_portal_signatures.py`). In game (same
  build and test setup, 2026-11-12; details in `docs/reverse-engineering/cartographer.md` section 7): monster stats
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
- `MapNames=1` in game (2026-11-12, build 93847, D2RLoader 1.3.1-beta, test mod and test save, Normal, single player, two games; log lines and screenshots checked by hand): the map's own name showed on the portal tooltip, the automap title and the entry banner, with no "Missing String", and again in the second game. Details and limits in `docs/reverse-engineering/cartographer.md` section 10. The log lines quoted there were not kept as evidence. Not checked: TCP/IP, other languages, a second map in the same game, a long name.
- Map sync part 1: unit tests of the message, its validation, the client's check, the client list and the connect schedule (`mapsync_tests`). No game run: the connection, the delivery, the late join and the thread each callback runs on are the checks in `docs/reverse-engineering/cartographer.md` section 12 ("Needs game").
- Not yet in game for `cartographer open <Name>`: repeating with an aura that reserves mana; leaving the game with the portal open and using the console in the next game; as a TCP/IP client the command refuses.
