# Cartographer: Siege check (2026-11-12)

Live in game: the `Siege` (Bloody Foothills) outdoor tileset (#288), slot 5, Hell, single player, test save `cartos`
(slot 5's saves, mod tables and plugin backed up first and restored afterwards; the slot was locked with a named lane and
unlocked at the end). D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, test mod `D2RToolsTest`. The installer was run against
the slot's mod and added the 4th level ("Cartographer Siege", level 141, 240 x 48 at (6000, 3900)) and the `OutdoorSiege`
slot row. The test map was named 'Foothills Test' (Tileset `Siege`, Boss 'Shoreline Warden', Medium). Two games; game 2 had
seed 1901899604470768226. The character used `debug_mode`, `cheat god 1`, and `cheat goto` to reach rooms. Logs and
screenshots are kept outside the repo.

| Check | Result |
|---|---|
| 1. Opens from its item in Hell | PASS: the map opened through slot level 141. |
| 2. Built as planned | PASS: 168 rooms built, 0 void rooms (log). |
| 3. Arrival in the entrance cell | PASS (position in the log matched the planned entrance cell). |
| 4. Map panel | PASS: 'Foothills Test', monster level 85, the map properties on the right of the screen. |
| 5. Boss in the boss room | PASS: 'Shoreline Warden' spawned in the boss room (30080,19580) 40 x 40 as a superunique (log line below); screenshot of the Boss among its minions. |
| 6. Event in the event room | PASS: the Treasure event 'Sunken Hoard' spawned 1 of 1 in the event room (30800,19580) 40 x 40. |
| 7. Seams | Not settled: strip centres 2..11 were visited with `cheat goto` and a few screenshots looked plausible; no walk across every seam. |
| 8. North and south edge | Not settled. |
| Stability | PASS: 0 `[ERROR]` and 0 `[FAULT]` in `cartographer.log`. |

Log lines:

```
eventprobe: the map Boss 'Shoreline Warden' goes in the plan's boss room (6016,3916) 8 x 8
eventprobe: room #72 of level 141 is the boss room (DRLG (6016,3916) 8 x 8)
map Boss 'Shoreline Warden': spawned in the boss room (30080,19580) 40 x 40 (as a superunique)
event 'Sunken Hoard' (Treasure): spawned 1 of 1 in the event room (30800,19580) 40 x 40 (0 as a superunique, 0 refused by the game)
```

Other:

- The installer's `Shape` blank refusal and a BadInventory on a hand-made save character were worked around in the test setup (a `Winding` shape in the test row, the "areas" test character); neither is a Siege fault.
- Menu clicks sent before the menu was ready were ignored; a second `debug_mode` toggled it off in game 1.
- The `cheat goto` to the map's edge rows returned the character to the entry.

Not tested: walking across the seams, the north and south edges, a Horde or Ambush event on Siege, multiplayer, Size other than Medium, the Boss kill and drop.
