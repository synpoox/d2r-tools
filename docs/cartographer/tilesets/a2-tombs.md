# Act 2 Tombs (#242)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 55 Tomb 1 A, 56-57 Tomb 2 A/B (57 has a waypoint), 58 Tomb 3 A, 59-61 Tomb 1-3 Treasure, 66-72 Tomb Tal 1-7 (Tal Rasha's). 73 Duriel's Lair is a preset |
| DrlgType / LevelType | 1 (maze) / 17 Act 2 - Tomb (11 DT1 files) |
| `lvlmaze` rows | Rooms 4, 6, 6, 12 (55-58), 6 for the treasure and Tal tombs; SizeX = SizeY = 16, Merge 500 |
| Room pieces | Def 414-428: Tomb W..NSEW, 15 masks, 3-4 DS1 each (`pieceBase` 413) |
| Variants | 429-443 Theme |
| Special rooms | Prev **SEW 444, NEW 445, NSW 446, NSE 447** (three exits, not dead ends); Next 448-451; Treasure 452-455; Cube 456-459; Tal Rasha 460-463; Leatherarm 464-467; Kaa 468-471; Chest 472-475; Waypoint 476-479; Tainted Sun 480 |
| Warps | 45 Up, 46 Down |

## Does the Cave/Crypt path work unchanged?

No. The tomb cases run **their own growers (`0x3ea0e0`, `0x3eb610`)**. Also `0x3e96d0` triples the room count for the act's chosen tomb (the level id at `drlg+0x120`, drawn at act start-up) and doubles it for another (`drlg+0x874`); a slot id matches neither. The growers must be hooked as for the Catacombs. The quest pieces (Tal Rasha, Kaa, Leatherarm, Cube) are chosen by level id and a slot never gets them; the Treasure and Chest pieces are not reachable from a plan unless the tileset adds them as event rooms. The research marks the tombs "not yet steerable".

The entrance is an ordinary room (the three-exit Prev pieces are not a one-exit family).

Tileset name: `Tombs` (not yet in `TilesetNames`).

## Risk, effort

Risk **high**: two growers, act-level draws, and 16-tile cells (the largest of the A1-A2 mazes; a 360-tile level holds 21 across). Effort **L**.

## Game checks

- Grower hooks give exactly the planned rooms; no Tal Rasha or Duriel piece appears.
- Cell box equals the plan box at 16 tiles; arrival, Boss, event.
- The level is stable across a restart; waypoint pieces absent unless planned.
