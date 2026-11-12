# Act 2 Palace Cellar (#278)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 52 Basement 1 (waypoint), 53 Basement 2, 54 Basement 3 |
| DrlgType / LevelType | 1 (maze) / 15 Act 2 - Basement (3 DT1 files) |
| `lvlmaze` rows | Rooms 4, SizeX = SizeY = 16, Merge 1000 |
| Room pieces | Only **four**: Def 358-361 "Basement NE, SE, SW, NW", corner pieces, 16 x 16; 3-4 DS1 each, **including waypoint variants** |
| Special rooms | none in `lvlprest` |
| Warps | 30-31 Up, 32 Down |

## Does the Cave/Crypt path work unchanged?

No, for the reasons in `a2-harem.md`: the Basement case lays only the 2 x 2 ring and picks from a small table, not `base + mask`. Four corner pieces mean one expressible layout, a four-room loop. The same code serves both: a ring-only plan, `maskPieces` limited to the four corner masks, a one-shape rule, an installer value. Do both in one change.

Extra for this tileset: the corner pieces have waypoint files in their per-piece file rotation. A map built from them may place a waypoint in one of its four rooms (a waypoint inside a map). Decide whether that is wanted; the rotation is seeded by the level, so Cartographer cannot choose files without another hook.

## Risk, effort

Risk **medium-high**. Effort **L** alone, **M** on top of the harem work.

## Game checks

- As the harem: four rooms, planned corner pieces, distinct entrance, Boss and event rooms.
- Whether a waypoint appears and opens; Boss and event spawns with a waypoint room present.
