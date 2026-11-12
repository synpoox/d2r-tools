# Act 3 spider cavern (issue #284)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 84 Spider 1 (Spider Cavern) and 85 Spider 2 (Arachnid Lair) |
| LevelType / DrlgType | LevelType 23 "Act 3 - Spider" (one DT1 file); DrlgType 1; palette 2 |
| `lvlmaze.txt` | Rooms 4 in all difficulties, SizeX = SizeY = 16, Merge 500 |
| `lvlprest.txt` room pieces | Six rows only: Def 659-662 are the corners SW, SE, NW, NE (16 x 16, 3 files each); Def 663-664 are "Chest NW" and "Chest NE" (treasure variants of two corners) |
| Entrance, exit, stairs | None. No Prev, Next, Down, Theme or waypoint rows. The levels have warp 52 (Spider to Jungle) and a Vis link to level 76 |
| Dead ends | None: there is no one-exit and no straight piece |

## Does the Cave/Crypt path work unchanged?

No. The maze model is "piece = `pieceBase` + exit mask" with 15 mask pieces and a one-exit Prev family. Spider has four corner pieces, and they are not in base + mask order (masks 5, 6, 9, 10 would need Def 659, 660, 663, 664 from one base, but the corners are 659-662). With Rooms 4 and only corners, the only closed layout is a 2 x 2 ring of four rooms (32 x 32 tiles); anything longer needs straight pieces that do not exist. The builder case for LevelType 23 is not recorded, so how it places the four rooms and the chests is unknown.

A tileset entry would have to be a fixed one-shape tileset (always the 2 x 2 ring, a Chest variant as the event or boss room). That needs a new path in `planMaze` (no dead-end entrance, only four piece masks, piece ids listed instead of base + mask) and a layout rule for it.

## Risk, effort, checks

- Risk: high for the value: a fixed four-room map with no path from a start to an end. Also the Act 3 tiles in an Act 5 slot level (cross-act preload).
- Effort: L for a fixed ring (new piece mapping, new shape, new checks). Recommend deciding first whether a four-room map is worth a tileset, or closing as low value.
- Game checks: static read of the LevelType 23 case of `0x3e69e0`; a slot built as the ring; whether the Chest pieces work without the vanilla chest logic; arrival; Act 3 tiles in the Act 5 slot.
