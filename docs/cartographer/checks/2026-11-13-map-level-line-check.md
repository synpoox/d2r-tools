# Cartographer: Area Level line on map items, in-game checks (2026-11-13)

Issue #422. Plugin built from `cartographer/map-level-line`, D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, Hell, single player, test save `cartos` (restored afterwards), `debug_mode` then `cheat god 1` before any map. Staged with `stage_check.ps1 -MapItems -Tables examples\first-map`. Slot 7 (first pass, text then read `Map Level`) and slot 5 (second pass, text `Area Level`). The slots' `misc.txt` rows for `t20` to `t22` had `compactsave` 1 and were set to 0; slot 5 also got `rarity` 100 and `spawnable` 1 on them, three H2H treasure class rows pointing at `t20`, three extra `cubemain.txt` rows and a unique row (`map-level` 95); all put back. Screenshots and logs are not kept.

| # | Check | Result |
|---|---|---|
| 1 | Normal map, `cheat spawn`: hover shows `Area Level: 90` (Shorelines) or `85` first | PASS |
| 2 | Magic and rare maps (slot 7): the line first, then the affix lines; survives Save and Exit | PASS |
| 3 | The map opens at the line's level (`map level 85 (from the item's stat)`) | PASS (slot 7) |
| 4 | Cube `usetype,nor,mod` with `map-level` 10: the same map, 85 to 95 to 105 | PASS |
| 5 | Cube `t22,mod` from a Level 90 map: Level 100 | PASS |
| 6 | Cube plain `t22` output: shows `Area Level: 85` | PASS, but stamped by the roll hook; the post-cube stamp was not proven |
| 7 | Monster drop (`testtreasure mon=19,num=40` with a map treasure class): 40 maps | Stamped (log lines), named on the ground; the pickup hover was not read |
| 8 | Unique with `map-level` 95 on a Level 85 map | Log: replaced, Level 95; the item was unidentified, so no hover |
| 9 | Magic or rare affix with `map-level` | Not checked |
| 10 | Vendor, chest and gamble maps; `compactsave` 1; multiplayer | Not checked |

Older maps in the save showed no line and still opened at their `maps.txt` Level. Unit tests: `statplan_tests` `mapLevelToStamp`, `mapname_tests` (the text). `stage_check.ps1 -MapItems` now warns when an existing `maps.txt`-linked row's override column differs from `map-items.txt`.
