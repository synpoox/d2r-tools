# Cartographer: Ice caves check (2026-11-12)

Live in game: the plugin built from the `cartographer/tileset-ice-caves` branch (on `cartographer/maze-slot-table`),
slot 11, Hell, single player, test save `cartos` (level 1 Barbarian, Hell opened, god mode, test mod, no real saves),
staged with `stage_check.ps1` (owner `carto-ice`). Map `Glacial Veins` (Tileset `Ice`, Shape `Winding`, Size `Medium`),
Boss `Shoreline Warden`, an Ambush event, `EventChance=100`, the example `mapstats.txt`. One game, opened from the map item
(`cheat spawn t20`, right-click in Harrogath). Logs and screenshots are kept outside the repo.

| Check | Result |
|---|---|
| Normal/Nightmare refusal | PASS: the item in a Nightmare game logged "can only be opened in Hell" and said "I can't use this yet". |
| Opens from its item in Hell | PASS: "Glacial Veins is open", red portal, log `opening 'Glacial Veins' (Ice) through level 143`. |
| Built as planned | PASS: `maze slot level 143: built as planned (19 rooms, entrance placed)`, level 144 x 80 tiles, 0 refits. The map is the Ice cave tileset (Act 5 ice walls seen on arrival). |
| Arrival in the entrance cell | PASS: `room 75 of 76 (at 6400,64) marked as the level entry, in plan cell (0,4)`; the player stood by the cave wall of the entry room. |
| Boss room | PASS by log: `the map Boss 'Shoreline Warden' goes in the plan's boss room (6496,0) 8 x 8` (plan cell 6,0). The Boss was not seen on screen. |
| Event room | PASS by log: the Ambush spawned 6 `zombie1` in the plan's event room, rooms #1-#5 (6400-6408, 56-72), plan cell (6,3) rect (6496,48). The positions logged are the Ambush's own rooms, not the event cell: not compared further. |
| Map stats and panel | PASS: panel showed Monster level 85 and the two monster and two player properties. |
| Stability | PASS: 0 `[ERROR]`, 0 `[FAULT]`. |

Log lines:

```
maze slot level 143: steering 'Glacial Veins' (Ice Winding Medium, ...): 19 rooms, level 144 x 80 tiles, 0 refits
maze slot level 143: built as planned (19 rooms, entrance placed)
maze arrival: room 75 of 76 (at 6400,64) marked as the level entry, in plan cell (0,4); 0 other room(s) lost the mark
eventprobe: maze boss room (plan room 17, cell 6,0) is the DRLG room at list index 71, level 143, rect (6496,0) 8 x 8
eventprobe: the map Boss 'Shoreline Warden' goes in the plan's boss room (6496,0) 8 x 8
```

## Findings

- A 16-tile cell is two engine rooms of 8 by 8 on each side (the log rects are 8 x 8 and the entry room sits at the cell's
  corner), as a 24-tile Cave cell splits into three: the assumption in RE section 23 holds for arrival, the Boss room and the event room.
- Not verified: the Boss and the event monsters seen in their rooms on screen (a warp to the boss room did not clearly
  move the character), Nightmare-free multiplayer, a second Ice map in one game, the other shapes in game.
- The staging slot's Hell had to be chosen in the Select Difficulty box; the character the earlier checks left in Normal does not open maps.
