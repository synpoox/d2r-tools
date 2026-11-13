# Cartographer: Boss room fallback check (2026-11-12)

PR #369 (issue #364), build of the `cartographer/maze-boss-room` branch merged with `origin/main` (Ice caves, #335).
Slot 6, Hell, single player, test save `cartoh` (god mode, `debug_mode`), staged with `stage_check.ps1` (owner `bossroom`).
Map `IceXS` (Tileset `Ice`, Shape `Winding`, Size `Small`), Boss `Shoreline Warden`, `EventChance=0`, opened with
`cartographer open IceXS` in Harrogath, entered with `cheat goto 141`, then `cheat goto 141,<x>,<y>` into the boss cell.
Logs and screenshots are kept outside the repo.

Setup: the Ice cell pieces `IceE01.ds1` and `IceS01.ds1` have almost no floor in their top-left 8 x 8 quadrant (read from
the DS1 floor layer), so the slot's test mod got a `lvlprest.txt` copy with the Ice E and Ice S rows pinned to those
variants (removed after the check). It makes a dead-end boss cell that opens east or south likely to have a void top-left
sub-room; it does not guarantee one.

| Check | Result |
|---|---|
| Boss cell with a void top-left sub-room | PASS (one game): boss cell (0,0), top edge, first room (2000,0) 8 x 8, server rect (10000,0) 40 x 40: all 9 spots refused as a superunique and all 9 as a plain monster. |
| Fallback to the next room of the cell | PASS: `trying the next room of the boss cell (2008,0) 8 x 8`, then `spawned in the boss room (10040,0) 40 x 40 (as a superunique)`. |
| Named Boss on screen | PASS: banner "Shoreline Warden", Undead, Extra Strong, Extra Fast, Spectral Hit, Mana Burn, Immune to Cold, with its minions, in the next room of the cell. |
| Stability | PASS: 0 `[ERROR]`, 0 `[FAULT]`. |

Log lines:

```
eventprobe: the map Boss 'Shoreline Warden' goes in the plan's boss room (2000,0) 8 x 8 (3 room(s) in the cell to fall back on)
map Boss 'Shoreline Warden': refused by the game in the boss room (10000,0) 40 x 40 (as a plain monster)
map Boss 'Shoreline Warden': trying the next room of the boss cell (2008,0) 8 x 8
map Boss 'Shoreline Warden': spawned in the boss room (10040,0) 40 x 40 (as a superunique)
```

Also seen: about 14 games of Ice Corridor and Winding maps; with no variant pinned, and with the pin on boss cells that
were not a pinned dead end, the first room took the Boss every time (including top-edge cells), so the fallback did not run.
An earlier game of the same Small map (log lost) also showed a first room refused and a spawn in the cell's next room (2024,0).

Not verified: all four rooms of a cell refused (the give-up line), other tilesets, multiplayer.
