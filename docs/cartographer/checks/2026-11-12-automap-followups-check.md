# Cartographer: automap re-entry follow-up check (2026-11-12, issue #354)

Live in game, partial: plugin built from the `cartographer/automap-followups` branch (skip state in `automap_skip.h`), D2R build 93847,
D2RLoader 1.3.1-beta, slot 5, Hell, single player, test save `cartoh` (restored afterwards), `debug_mode` and `cheat god 1` set first,
staged with `stage_check.ps1 -MapItems` and the example tables. Logs and screenshots are not kept in the repository.

Passed:

- **Cave map, entry:** Tunneling Depths opened and entered; one log line, "the revealed cells of an earlier map in layer 2 were not read back for this new map (level 138)".
- **Cave map, town trip:** after warps inside the map, `cheat goto 109` and the red portal back, a warp to a spot revealed before the trip drew the same
  walls as before, including the trail of an earlier warp 100 units away that was not revisited. No second skip line. The reveal survives the town trip.

## Waypoint trip (second session, slot 7)

Plugin built from the merge of `origin/main` into this branch (`ed28d36`), staged with `stage_check.ps1` (owner `carto-wp`, `-MapItems`), D2R build 93847,
D2RLoader 1.3.1-beta, slot 7, Hell, single player, test save `cartoh` (a level 40 Barbarian; its save files in the slot stay changed), `debug_mode`, `cheat waypoints`
and `cheat god 1` set first. Maps opened with `cartographer open Tunneling Depths` (Cave, slot level 138). Judged from the plugin log only; no automap line was looked at.
Logs and screenshots are not kept in the repository.

How the trip was made: the real Harrogath waypoint was not walked to. `cheat ospawn 429` (the Objects.txt `Id` of `WaypointExp`, the row the stock Act 5 waypoint
uses; the name `WaypointExp` and a row number off by one spawned nothing visible) puts a working waypoint object next to the player. Clicking it opened the waypoint
panel, and Cold Plains (level 3, Act 1) was picked in it: a real waypoint transfer, arriving on the Cold Plains waypoint. The way back to town was
`cheat goto 109` (no town portal scroll was used), then the map's red portal in Harrogath. A spawned waypoint is gone after a level change.

Passed (log lines):

- **Game 1, trip after the entry skip.** Map opened in Harrogath; entered: one line, "the revealed cells of an earlier map in layer 10 were not read back for this new map (level 138)".
  Then `cheat goto 109` (town), waypoint to Cold Plains, `cheat goto 109`, back through the map's portal: the map was rebuilt from the same plan, and no further
  automap line appeared (no second skip, no "read kept" line, since the skip had disarmed itself).
- **Game 2, trip before entering (the case the hardening is for).** Map opened, then, before entering: waypoint to Cold Plains, `cheat goto 109`, portal. The log shows
  "read of layer 0 kept" / "read of layer 79 kept (the player is in level 3, the new map is level 138)" while loading Cold Plains and "... in level 109" on the return to
  town, and only then, on entering, one "were not read back for this new map (level 138)". So the skip waited for the player to be in the slot level, as designed.

Observations:

- The "read kept" line is written for every read while a skip is armed: 74 lines in game 2, 68 of them within a second of loading Cold Plains (layers 0 and 79 alternating).
  Harmless but noisy; the line could be written once per layer.
- The slot map's layer here is 10 (a Crypt layer) for a Cave map; the earlier session's Cave check and note 22 say layer 2. Not investigated.

Not done:

- The order guard (`SkipTick`, a skip missed because the read came before the tick) cannot be forced in game; unit tests only.
- The reveal of an area after a waypoint trip was not looked at (owner's check).
- Multiplayer and Act 5 levels 111 and 112 against Snow at 1500,0 were not checked. Crypt was checked in #332.
