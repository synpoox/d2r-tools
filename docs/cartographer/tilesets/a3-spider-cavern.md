# Act 3 spider cavern (issue #284)

Built (note 48), checked in game. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 84 Spider 1 (Spider Cavern) and 85 Spider 2 (Arachnid Lair) |
| LevelType / DrlgType | LevelType 23 "Act 3 - Spider" (one DT1 file); DrlgType 1; palette 2 |
| `lvlmaze.txt` | Rooms 4 in all difficulties, SizeX = SizeY = 16, Merge 500 |
| `lvlprest.txt` room pieces | Six rows only: Def 659-662 are the corners SW, SE, NW, NE (16 x 16, 3 files each); Def 663-664 are "Chest NW" and "Chest NE" (treasure variants of two corners) |
| Entrance, exit, stairs | None. No Prev, Next, Down, Theme or waypoint rows. The levels have warp 52 (Spider to Jungle) and a Vis link to level 76 |
| Dead ends | None: there is no one-exit and no straight piece |

## Does the Cave/Crypt path work unchanged?

No. The maze model is "piece = `pieceBase` + exit mask" with 15 mask pieces and a one-exit Prev family. Spider has four corner pieces, and they are not in base + mask order (masks 5, 6, 9, 10 would need Def 659, 660, 663, 664 from one base, but the corners are 659-662). With Rooms 4 and only corners, the only closed layout is a 2 x 2 ring of four rooms (32 x 32 tiles); anything longer needs straight pieces that do not exist. The LevelType 23 case is the Harem case (note 48): one fixed ring of four rooms, corners by exit mask (SW 659, SE 660, NW 661, NE 662). The Chest pieces 663/664 are only picked on levels 84/85, never on a slot level.

A tileset entry would have to be a fixed one-shape tileset (always the 2 x 2 ring, a Chest variant as the event or boss room). That needs a new path in `planMaze` (no dead-end entrance, only four piece masks, piece ids listed instead of base + mask) and a layout rule for it.

## Risk, effort, checks

- Risk: high for the value: a fixed four-room map with no path from a start to an end. Also the Act 3 tiles in an Act 5 slot level (cross-act preload).
- Effort: L for a fixed ring (new piece mapping, new shape, new checks).
- Game checks: static read of the LevelType 23 case of `0x3e69e0`; a slot built as the ring; whether the Chest pieces work without the vanilla chest logic; arrival; Act 3 tiles in the Act 5 slot.
