# Cartographer: automap re-entry follow-up check (2026-11-12, issue #354)

Live in game, partial: plugin built from the `cartographer/automap-followups` branch (skip state in `automap_skip.h`), D2R build 93847,
D2RLoader 1.3.1-beta, slot 5, Hell, single player, test save `cartoh` (restored afterwards), `debug_mode` and `cheat god 1` set first,
staged with `stage_check.ps1 -MapItems` and the example tables. Logs and screenshots are not kept in the repository.

Passed:

- **Cave map, entry:** Tunneling Depths opened and entered; one log line, "the revealed cells of an earlier map in layer 2 were not read back for this new map (level 138)".
- **Cave map, town trip:** after warps inside the map, `cheat goto 109` and the red portal back, a warp to a spot revealed before the trip drew the same
  walls as before, including the trail of an earlier warp 100 units away that was not revisited. No second skip line. The reveal survives the town trip.

Not done (the session ended):

- **A real waypoint trip checked by eye.** A second game with a fresh Tunneling Depths portal was started and `cheat waypoints` run; the Harrogath
  waypoint was not found (`cheat ospawn` of rows 432 and `WaypointExp` showed nothing), so no waypoint trip was made.
- The order guard (`SkipTick`) was exercised by unit tests only, not in game.
- Crypt was checked in #332. Multiplayer and Act 5 levels 111 and 112 against Snow at 1500,0 were not checked.
