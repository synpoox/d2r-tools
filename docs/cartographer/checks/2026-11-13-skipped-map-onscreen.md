# Cartographer: skipped map row message on screen (2026-11-13)

Live in game for #393 (follow-up to [2026-11-13-skipped-map-check](2026-11-13-skipped-map-check.md)). Plugin built from this
branch (`tools\build.ps1 -Plugin cartographer -Test`, 21 of 21 tests pass), staged with `stage_check.ps1 -Slot 7 -MapItems`
(slots 5 and 11 were busy), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, Hell, single player, level 99 test Barbarian `cartos`
(a copy of a test save staged into the slot; no real save touched). `debug_mode` and `cheat god 1` were on. The same check also
passed once on slot 11 with an earlier build (before the local-player check). The game window and PID (52660) were confirmed
by `game_ctl.py` before any input. Evidence is kept outside the repository
(`C:\D2RTestSlots\runs\skipmsg2\onscreen-banner.png`).

Setup: the example `maps.txt` with row 3 (`Tunneling Depths`, item `t21`) changed to tileset `Snow`, shape `Open` (refused).

| Check | Result |
|---|---|
| Right-click the item of the skipped row in Harrogath | PASS: a banner near the top of the screen shows "Map Tunneling Depths is not set up." and "maps.txt row 3 was skipped (see cartographer-report.txt)."; the console and log lines are still written; the game still says "I can't use this yet"; the item stays in the inventory. |

Log lines:

```
the item 't21' opens nothing: its maps.txt row 3 ('Tunneling Depths') was skipped at load: the tileset 'Snow' cannot build the Open shape: ...
Map Tunneling Depths is not set up: maps.txt row 3 was skipped (see cartographer-report.txt).
entry banner: panel opened
entry banner: text set (2 line(s))
```

Not checked: a row with an empty Name or a duplicate Item in game (unit tested), a client in TCP/IP, how long the banner stays
(it is the existing entry banner, about 6 s).
