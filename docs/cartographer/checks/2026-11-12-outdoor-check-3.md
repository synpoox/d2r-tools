# Cartographer: Outdoor check 3 (2026-11-12)

Live in game: the plugin built from origin/main `2accd9f` (before PR #202), slot 6, Hell, single player, test save
`cartoh`, staged with `stage_check.ps1` (owner `carto-outdoor3`). Two sessions: session 1 `AutoCheckMap=Overgrown` with a
Treasure-only `mapevents.txt`, session 2 `AutoCheckMap=Wilds` with a Horde-only one. `OutdoorBuild=1`, `OutdoorProbe=1`,
`EventChance=100`, `Properties=6`, `PlayerStats=1` (test character), the example `mapstats.txt`. Every open was made by
AutoCheck. The earlier checks are [outdoor-check](2026-11-12-outdoor-check.md) and
[outdoor-check-2](2026-11-12-outdoor-check-2.md). The logs and screenshots are kept outside the repo.

| Check | Result |
|---|---|
| Treasure event outdoors | PASS: 'Sunken Hoard' spawned 1 of 1 in the event room, in the event cell. |
| `Wilds` map | PASS: opened, 23 stamps, 784 void cells, built as planned, arrival logged in the entrance cell, Boss 'Wilds Lord' spawned as a superunique in the plan's boss room (6008,3136). AutoCheck's boss step still printed FAIL: this build predates #202 (fixed since). |
| Horde event | Spawned 4 of 4 although x6 was planned: bug #203. The outdoor event room is one 8 x 8 room, so it has only 4 spots. |
| Map stats (monsters and player) | PASS: 10 monsters (Overgrown) and 6 (Wilds) took 2 drawn properties each. Overgrown player: fireresist -24, hpregen -22. A hovered zombie showed Fire Resist 37%, matching the drawn +37. |
| Boss name | PASS: "TEST WARDEN" shown on hover (Overgrown). |
| Stability | PASS: 2 games, 0 `[ERROR]`, 0 `[FAULT]`. |

Log lines:

```
outdoor slot level 139: built as planned (23 stamps, 784 void cells; grid 1152 cells: 23 pieces, 368 covered, 784 void, 0 neither), 368 rooms
event 'Sunken Hoard' (Treasure): spawned 1 of 1 in the event room (31160,15360) 40 x 40 (0 as a superunique, 0 refused by the game)
map Boss 'Wilds Lord': spawned in the boss room (30040,15680) 40 x 40 (as a superunique)
event for 'Wilds' (seed 866270693896107723, chance 100%): Horde 'The Horde' (zombie1) x6 at (0,0)
event 'The Horde' (Horde): spawned 4 of 4 in the event room (30520,15200) 40 x 40 (0 as a superunique, 0 refused by the game)
AUTOCHECK PASS stat-changes: 10 monster(s) took 2 drawn properties
```

## Findings

- #203: an outdoor event room is a single 8 x 8 room, so a Horde of 6 gets 4 spots; the count needs to follow the room.
- AutoCheck's boss step printed `no boss room to go to` in both sessions; the Bosses spawned once the player went there.
  #202 teaches AutoCheck outdoor boss rooms.

## Staging gaps (not plugin findings)

- The slot `d2rloader.toml` `default_mod` pointed at another mod and had to be corrected.
- `d2rloader\core-ui\automap.txt` and `itemstatcost.txt` were staged by hand; a `stage_check.ps1` fix is in progress.

## Not verified

- Saving a character that has player stats.
- The `Wilds` Boss name on hover.
- A Horde count on a larger room.
- Opening from the item (opens were by AutoCheck).
- Multiplayer, Nightmare and Normal.
