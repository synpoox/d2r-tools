# Cartographer: Ice caves check (2026-11-12)

Live in game: the plugin built from the `cartographer/tileset-ice-caves` branch (on `cartographer/maze-slot-table`),
slot 11, Hell, single player, test save `cartos` (level 1 Barbarian, Hell opened, god mode, test mod, no real saves),
staged with `stage_check.ps1` (owner `carto-ice`). Map `Glacial Veins` (Tileset `Ice`, Shape `Winding`, Size `Medium`),
Boss `Shoreline Warden`, an Ambush event, `EventChance=100`, the example `mapstats.txt`. One game, opened from the map item
(`cheat spawn t20`, right-click in Harrogath). Logs and screenshots are kept outside the repo.

| Check | Result |
|---|---|
| Nightmare refusal | PASS: the item in a Nightmare game logged "can only be opened in Hell" and said "I can't use this yet". |
| Opens from its item in Hell | PASS: "Glacial Veins is open", red portal, log `opening 'Glacial Veins' (Ice) through level 143`. |
| Built as planned | PASS: `maze slot level 143: built as planned (19 rooms, entrance placed)`, level 144 x 80 tiles, 0 refits. The map is the Ice cave tileset (Act 5 ice walls seen on arrival). |
| Arrival in the entrance cell | PASS: `room 75 of 76 (at 6400,64) marked as the level entry, in plan cell (0,4)`; the player stood by the cave wall of the entry room. |
| Boss room | PARTLY: first game (slot 11) log only. Second game (slot 5, Hell, level 143, 24 rooms, arrival in plan cell (8,8)): after `cheat goto` to the boss room (plan cell (0,7), rect (6400,112)) the log read `map Boss 'Shoreline Warden': spawned in the boss room (32000,560) 40 x 40 (as a superunique)` and a pack of zombies (the Boss's minions) stood in the room, one hovered as "Zombie, Undead, Minion". The Boss itself was not identified on screen: NOT VERIFIED. |
| Event room | PASS: slot 5, Ambush armed in the event room (plan cell (4,1), rect (6464,16)): `armed in the event room (32320,80) 40 x 40: 5 per wave`, then on standing in it `wave 1 of 2: the game made 5 of 5 units in the event room`; the zombies were seen on screen. |
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
  corner), as a 24-tile Cave cell splits into three: the assumption in RE section 24 holds for arrival, the Boss room and the event room.
- Not verified: the named Boss seen on screen (only its spawn log line and its minions), multiplayer, a second Ice map in
  one game, the other shapes in game.
- `cheat goto <level>,<x>,<y>` takes absolute subtile coordinates (tile x 5 + 2), not tile coordinates.
- Automap range: this check ran with Ice installed at 6400,0 (the rects above are at that offset), which the engine's signed 16-bit automap cells cannot hold (x about 51200).
  Ice is now at 2000,0 (in range, clear of Cave 3500, Crypt 3000 and the reserved Lava 2500; RE note 22). The layout and spawns are the same at either spot; the new offset has not been run in game yet.
- The staging slot's Hell had to be chosen in the Select Difficulty box; the character the earlier checks left in Normal does not open maps.
