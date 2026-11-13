# Cartographer: Map Level line on map items, slot 7 check (2026-11-13)

Issue #422. Plugin built from `cartographer/map-level-line`, D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7, Hell, single player, test save `cartos` (restored afterwards), `debug_mode` then `cheat god 1` before any map. Staged with `stage_check.ps1 -Slot 7 -MapItems -Tables examples\first-map`. The slot's `misc.txt` rows for `t20` to `t22` had `compactsave` 1 and were set to 0 for the run, and the `cmap` affix tables (`magicprefix`, `magicsuffix`, `rareprefix`, `raresuffix`) were copied in from another slot's test mod; both were put back. Maps came from `cheat spawn` (`t20`; `t21,typ m`; `t22,typ r`; `t21,typ r`; `t22,typ m`). Screenshots and logs are not kept.

| # | Check | Result |
|---|---|---|
| 1 | Normal map: hover shows `Map Level: 90` (Shorelines) or `85` below the name | PASS |
| 2 | Magic map (forced): `Map Level: 85` first, then `+25% Monster Damage`, `All Resistances +7` | PASS |
| 3 | Rare map: `Map Level: 85` first, then the monster and player lines | PASS (`Viper Eye Tunneling Depths Map`, `Plague Eagle Overgrown Reach Map`) |
| 4 | Save and Exit, restart: the line is still there on a normal, a magic and a rare map | PASS |
| 5 | The rare map opens at the line's level | PASS: `map level 85 (from the item's stat)`, monster row Level 85, the panel read `Monster level 85` |
| 6 | Same maps made with `compactsave` 1 | Not checked (the line shows nothing for items that keep no stats; rows were set to 0 first) |
| 7 | A map from a monster drop | Not checked: the test rows have `rarity` 0, so nothing drops them. `cheat spawn` goes through the same call site as drops (RE note 26) |

Older maps in the save (made before this build) showed no line and still opened at their `maps.txt` Level. Unit tests: `statplan_tests` `mapLevelToStamp`. Multiplayer and a client view were not checked.
