# Cartographer: Outdoor check 2 (2026-11-12)

Live in game: the plugin built from origin/main `eea4a93` (PR #179 and PR #181), D2R 3.3.0 build 93847, slot 6, Hell,
single player, the example `Overgrown` Jungle map with Boss "Test Warden" and a Horde event, AutoCheck on. Every open
was made by AutoCheck. 7 games. The earlier check is [outdoor-check](2026-11-12-outdoor-check.md).
The logs and screenshots are kept outside the repo.

| Check | Result |
|---|---|
| Map Boss (#190) | PASS: spawned as a superunique in the plan's boss room, once per game, in the 3 of 7 games where the player went there. Two layouts were seen (boss rooms (6168,3200) and (6072,3008)). The other 4 games were not visited: the script teleported to the fixed coordinates of layout 1. A script artifact, not a plugin finding. |
| Arrival in the entrance cell (#143) | PASS: 9 of 9 arrivals logged the player in the entrance cell (the main log keeps 8 of these lines). |
| Horde event in the event room | PASS: spawned 4 of 4 in the event room. The log plans `x6` against 4 spots; not investigated. |
| Automap and edges (#142) | Inconclusive; #142 stays open. No seam artifacts seen and the void edges look like forest wall, but that they are unwalkable was not proven. |
| Stability | PASS: 7 games, 19 `built as planned` lines, 0 `[ERROR]`, 0 `[FAULT]`, 4 layouts of 480 to 784 void cells. |

Log lines:

```
eventprobe: the map Boss 'Test Warden' goes in the plan's boss room (6168,3200) 8 x 8
map Boss 'Test Warden': spawned in the boss room (30840,16000) 40 x 40 (as a superunique)
event 'The Horde' (Horde): spawned 4 of 4 in the event room (30040,15680) 40 x 40 (0 as a superunique, 0 refused by the game)
```

## Findings

- AutoCheck's boss step reports FAIL on Jungle maps (`no boss room to go to`): it does not know outdoor boss rooms. Being
  fixed separately; the Boss itself spawned.
- AutoCheck's arrival step is N/A on Jungle maps (`a Jungle map has no maze entrance cell to compare`). The #143 result
  above comes from the outdoor probe's arrival lines.
- AutoCheck's stat-changes step FAILED because the slot lacked `mapstats.txt` and `itemstatcost.txt`: a staging gap, not
  a plugin finding.
- A later `cartographer open` is refused after AutoCheck already opened the map. Expected: one Outdoor build per game.
- `stage_check.ps1` does not seed `d2rloader\core-ui\automap.txt`; without it the game asserts at startup.
- A fresh slot save needs the first-run screens clicked before the check can start.
- `core-maps.dll` is present in the slot's plugins and logs a warning; behaviour was normal.

## Not tested

- Multiplayer, Nightmare and Normal.
- Opening from the item (opens were by AutoCheck).
- The `Wilds` map.
- A Treasure event outdoors.
