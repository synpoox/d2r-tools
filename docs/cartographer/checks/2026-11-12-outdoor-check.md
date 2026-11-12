# Outdoor map check, 2026-11-12

Game check of the Outdoor slot at PR #179's head `a47478d` (open from the item or portal, second-open guard, plan check,
`OutdoorSlotBuilt`, `PlanForJob`). Test slot 6, Hell, single player, `OutdoorBuild = 1`, `OutdoorProbe` off, the example
`Overgrown` Jungle map. D2R 3.3.0 build 93847. The raw log and screenshots are kept out of the repo.

| Check | Result |
|---|---|
| Opens | PASS: 16 opens in 15 games, each logged `built as planned` (the log has 32 such lines, two per open), 0 `[ERROR]` lines, no crash. |
| Portal via `cartographer open` | PASS |
| Opening from the item | Untested: the fixture items had no icon. Not a plugin finding. |
| Walking across piece seams | PASS. Targets in void cells were refused. |
| Automap edges | Inconclusive; #142 stays open. |
| Map Boss | FAIL: never spawned on an outdoor map (#190). 9 of the logged builds say `the map Boss 'Test Warden' is left off: the plan has no boss room I can read`: nothing recorded a boss room for the Outdoor level. PR #181 records it on the Outdoor level after a build that matched the plan and should fix it; it needs a game re-check. |
| Treasure event | PASS: spawned and fled. |
| Map stats | PASS: monster life doubled (`item_maxhp_percent(76)=+100`, max life 788992 to 1577984). |

Log lines (`cartographer-run.log`):

```
outdoor slot level 139: built as planned (18 stamps, 864 void cells; grid 1152 cells: 18 pieces, 288 covered, 864 void, 0 neither), 288 rooms
outdoor slot level 139: entrance cell at tile (6256,3000) 32 x 32; event planned; boss planned
map Boss for 'Overgrown': 'Test Warden', its own superuniques row
the map Boss 'Test Warden' is left off: the plan has no boss room I can read (the layout has BossRoom off, or the level was not built from a maze plan)
statprobe: monster #1 id 8 class 261 level 139: ... maxhp 788992 -> 1577984 ... list on chain: yes (3 lists)
```
