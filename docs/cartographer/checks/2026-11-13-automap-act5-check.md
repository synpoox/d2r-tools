# Cartographer: Act 5 levels 111 and 112 against Snow (2026-11-13, issue #354)

Live in game, one session: plugin built from `e8dba5d` (`tools\build.ps1 -Plugin cartographer -Test`, 21 of 21 pass), staged with
`stage_check.ps1 -Slot 11 -MapItems` and the example tables, D2R build 93847, D2RLoader 1.3.1-beta, slot 11, Hell, single player,
test save `cartos` (level 99 Barbarian; the save folder was backed up first and put back byte-identical afterwards), `debug_mode`
and `cheat god 1` set first. The example `maps.txt` row 4 was changed to `Frostline`, `Snow`, no Shape, item `t22`. Logs and
screenshots are not kept in the repository.

Check: Snow's slot level (141, tiles at 1500, 0) and the real Act 5 levels 111 (Frigid Highlands) and 112 (Arreat Plateau) in one game.
`cartographer open Frostline`, then `cheat goto 141`, `cheat goto 111`, `cheat goto 112`, `cheat goto 141`, `cheat goto 109`.

| Step | Result |
|---|---|
| Snow entered | PASS: the map was built as planned (24 stamps, 0 void cells), one automap line, "the revealed cells of an earlier map in layer 84 were not read back for this new map (level 141)". |
| Level 111, then 112 | PASS: both loaded and drew normally (screenshot of the Arreat Plateau waypoint); no automap line from the plugin, no plugin error. |
| Snow again, then Harrogath | PASS: Snow rebuilt from the same plan, no second skip line; town loaded. |
| Stability | `cartographer.log` has 0 `[ERROR]` and 0 `[FAULT]`. `d2rloader.log` has one `BC_ASSERT: nGameX && nGameY` (`SUnitInactive.cpp:1937`, "continuing because assertion dialogs are disabled") at the first Snow entry; not traced here, nothing visibly wrong. |

Not done: what the reveal looks like in 111 and 112 or in Snow after the trip (judged from logs and screenshots of the world only; the owner checks automap lines by eye),
multiplayer (needs a second game instance), the order guard in game (unit tests only), the waypoint trip's reveal by eye.
