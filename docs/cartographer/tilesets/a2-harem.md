# Act 2 Harem (#277)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 50 Harem (a fixed preset), 51 Corrupt Harem 1 (the maze) |
| DrlgType / LevelType | 1 (maze, level 51) / 14 Act 2 - Harem (7 DT1 files) |
| `lvlmaze` row (51) | Rooms 4, SizeX = SizeY = 16, Merge 1000 |
| Room pieces | Only **four**: Def 354-357 "Corrupt Harem NE, SE, SW, NW", corner pieces, 2 DS1 each, 16 x 16 |
| Special rooms | none in `lvlprest` |
| Warps | 25 (to Town), 26-29 Up and Down |

## Does the Cave/Crypt path work unchanged?

No. The Harem case does not walk or grow: it lays only the 2 x 2 ring (`0x3e9820`) and picks pieces from a small fixed table, not `base + mask`. With four corner pieces the only layout a plan can express is the 2 x 2 loop; Corridor, Winding, Branching and Sprawl need straight or T pieces that do not exist (`planMaze` would refuse them: "no room piece with exits ..."). So a harem "map" is one four-room layout.

Needed: a ring-only plan (`Tileset::maskPieces` limited to the four corner masks, `walkFollowsRing = false` so the graph is built inside the ring hook, per `Plan::ring.graphInRing`), a rule that the layout generator offers only that shape for this tileset, and an installer value. The entrance, Boss and event each need a room of the four: there is no far boss room, only corner rooms of one 32 x 32 tile area.

## Risk, effort

Risk **medium-high** (a tiny map, and the small piece table is unread). Effort **L** for what it delivers; the code is shared with the palace cellar (#278), so do them together. Decide with the owner whether a fixed four-room map meets the issue's Done-when before starting.

## Game checks

- The ring hook with the graph built inside it yields four rooms and no growth.
- The corner table gives the planned piece in each cell; entrance, Boss and event rooms are distinct.
- 16-tile cell box check; the 32 x 32 tile level the plan asks for is accepted.
