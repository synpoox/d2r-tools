# Cartographer: one Maze slot per maze tileset (PR #329), Cave then Crypt on one install, refusals, upgraded old install (2026-11-12)

Live in game: the plugin built from `cartographer/maze-slot-table` at `9b8b9a9` (the two review wording fixes on top of `e02f894`; the
merge of `origin/main` that followed, `5fde7fb`, changed only `docs/cartographer/tables.md`), copied into the slot by `stage_check.ps1 -MapItems`.
D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 6, single player, Hell, test save `cartos` (a Barbarian set to level 99,
experience 3520485254, 490 stat points, 98 skill points, Hell opened; the slot's save folder was put back from a copy afterwards).
`debug_mode` and `cheat god 1` were on before every map was opened. Maps were opened with `cartographer open <Name>` from the console (not from
the map item), so the item path was not exercised. Maps: `CaveA` (Cave, Corridor, Small, Boss, `t20`), `CryptA` (Crypt, Loop, Small, Boss, `t21`),
`CaveB` (Cave, Loop, Small, `t22`), all `EventChance` 100. The logs and screenshots are not kept in the repository.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Clean install (installer from this branch): a Cave map in game 1, a Crypt map in game 2, one process, one install | PASS: Cave through level 138, Crypt through level 139, each with its own tileset |
| 2 | Refusal while a maze map is open (Cave open and not entered, then a Crypt map and a second Cave map) | PASS: both refused with the "already built" message; the Cave portal stayed. The unbuilt-slot branch of this refusal (a Crypt open that finds a pending Cave job) was not reached in game: see 2 below |
| 3 | Old `-Tileset Crypt` install (Maze level 138 a Crypt level), new plugin, before rerunning the installer | PASS: Cave and Crypt maps refused with plain messages, nothing opened |
| 4 | Same install after rerunning this branch's `install.ps1` | PASS: Maze set back to Cave (LevelType 3), a Crypt level added (141), a Cave map in game 1 and a Crypt map in game 2 |
| 5 | Save and Exit after a map in runs 1 and 3 | No "Character save failed" in those two loader logs (run 2 opened no map, its log was not checked) |

One game assertion was logged in each process that entered a Cave map, see Notes. Skipped: the map item path, Normal and Nightmare, multiplayer,
walking through a whole layout (the arrival and the boss and event rooms were reached with `cheat goto`).

## Setup notes

- The slot's test mod was seeded from the vanilla tables by `stage_check.ps1`; the PD2 `core-maps`, `core-rules` and `core-ui` DLLs were
  parked and put back by `-Restore`. The slot's own `test-harness.dll` was used.
- The slot's save folder held two older test characters; it was copied aside first and put back afterwards (`-Restore` keeps a changed
  save, so the three `cartos` files it kept were deleted by hand).
- The old-install state for checks 3 and 4 was made in the same test mod: `levels.txt` and `lvlmaze.txt` back to the vanilla copies,
  `slots.txt` deleted, then the `install.ps1` from `origin/main` (`4da479c`) with `-Tileset Crypt` (Maze 138 a Crypt level, Outdoor 139,
  Snow 140), then this branch's installer. `debug_mode` switches the debug mode off when it is already on (it stays on across games in one
  process), so in later games of a process only `cheat god 1` was sent.

## 1. Cave then Crypt on one install (clean install)

Installer output: Maze 138 (Cave, LevelType 3), Crypt 139 (LevelType 4), `slots.txt` with `Maze 138` and `MazeCrypt 139`.

Game 1 (Hell, Harrogath), `cartographer open CaveA`:

    maze slot level 138: steering 'CaveA' (Cave Corridor Small, ...): 8 rooms, level 72 x 168 tiles ... 0 refits
    maze slot level 138: built as planned (8 rooms, entrance placed)
    maze arrival: room 67 of 72 (at 6032,128) marked as the level entry, in plan cell (1,5); 0 other room(s) lost the mark
    Cartographer: CaveA is open.

- Walking into the portal put the player beside the red portal in a cave (rock walls, torch), with the map's name and stats on screen.
- `cheat goto 138,30180,60` (the plan's boss room) gave `map Boss 'Shoreline Warden': spawned in the boss room (30160,40) 40 x 40`; the
  event room gave `event 'The Drowned King' (Boss): spawned 7 of 7 in the event room (30040,400)`.

Game 2 (Save and Exit, Play, Hell, same process), `cartographer open CryptA`: `opening ... level 139`, `built as planned (11 rooms, entrance an
ordinary room)`, `maze arrival: room 3 of 11 (at 6000,4216) marked as the level entry, in plan cell (0,2)`, `Cartographer: CryptA is open.`
In the portal the floor and walls were Crypt (stone floor, pillars). `cheat goto 139,30100,21140` gave `map Boss 'Shoreline Warden': spawned
in the boss room (30080,21120)`.

## 2. Refusal while a maze map is open

In game 1, before walking into the Cave portal, `cartographer open CryptA` and then `cartographer open CaveB` both gave:

    Cartographer: a maze level was already built (or is being built) in this game, so CryptA cannot be opened now. Leave the game and start a new one.

The message names the map that was refused. The slot was already marked built when the portal opened, so the check that stopped these two opens was the
"already built" one in front of `MazeOpenConflict`, not the `MazeOpenConflict` branch itself; that branch is covered by the unit test
(`a maze open is refused while a map of another maze tileset is open and unbuilt`) only.

## 3. Old install, new plugin

`levels.txt` had `Cartographer Maze` (138) as a Crypt level. At load the report and the log said, for `CaveA` and `CaveB`:

    maps.txt row 2: the Maze slot level is a Crypt level (LevelType 4), so it cannot build the Cave map 'CaveA' (needs LevelType 3). Each maze tileset has a slot level of its own: run tools\install.ps1 again, ...

In game, `cartographer open CaveA` gave that message and `cartographer open CryptA` gave "the MazeCrypt slot is not installed or was refused.
Run tools\install.ps1 ..."; no portal opened.

## 4. Upgrade by rerunning the installer

    levels.txt: level 138 had LevelType 4, not 3 (Cave): set it back to 3.
    levels.txt: repaired level 138 (...)
    levels.txt: added 'Cartographer Crypt' as level 141 (Crypt, copied from level 18).
    lvlmaze.txt: added the maze row for level 141 (MazeCrypt).
    slots.txt: Maze is level 138, MazeCrypt is level 141, Outdoor is level 139, OutdoorSnow is level 140 (other rows kept).

Level 138 then had LevelType 3 and the name string "Cave Level 1". Game 1: `CaveA` opened through 138 (`built as planned (10 rooms, entrance placed)`,
a second build line when the portal was entered). Game 2: `CryptA` opened through 141 (`built as planned (14 rooms, entrance an ordinary room)`),
and the portal room was a Crypt room.

## Notes (not looked into)

- The loader log has one `BC_ASSERT: i != dataTable->warpDefs.end()` ("Error in DRLG related to stairs that connect levels",
  `D2Common\src\DataTbls\LvlTbls.cpp:817`) in each of the two processes that entered a map (runs 1 and 3), at the moment the Cave map was entered.
  It was logged once per process, the game continued, and the Crypt games after it logged none. The earlier Crypt check (2026-11-12-crypt-difficulty-check.md) logged a
  similar `ptWarp` assertion; whether this one also happens on `main` was not checked.
- The `Open` shape added on `main` was not exercised.
