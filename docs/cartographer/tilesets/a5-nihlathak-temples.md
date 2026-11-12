# Act 5 Nihlathak temples (issue #290)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 122 Temple 1 and 123 Temple 2 (Halls of Anguish and Pain). 124 Temple Boss (Halls of Vaught) is a fixed preset (DrlgType 2, 84 x 84). 121 Temple Entrance is a preset too |
| LevelType / DrlgType | LevelType 32 "Act 5 - Temple" (one DT1 file); DrlgType 1 for 122 and 123; palette 4 |
| `lvlmaze.txt` | Rooms 4 in all difficulties, SizeX = SizeY = 40, Merge 500; the level is 120 x 120 |
| `lvlprest.txt` room pieces | Def 1042-1045: Temple NE, NW, SE (named "SE up"), SW, 40 x 40, 2 files each. Def 1046-1048: NE, NW, SW "Down" variants (stairs). Def 1049-1052: NE, NW, SE, SW "Waypoint" variants. No straight, T, cross or one-exit pieces, no Prev group |
| Entrance, exit | The "up" corners and the Down corners stand for the in-and-out stairs; there is no separate entrance piece |
| Dead ends | None |

## Does the Cave/Crypt path work unchanged?

No. Like the spider cavern, the set is four corner pieces, so with Rooms 4 the only closed layout is a 2 x 2 ring of four 40 x 40 rooms (80 x 80 tiles), and the specials (stairs, waypoint) are variants of the corners, not extra rooms. It does not follow base + mask (the four corners are Def 1042-1045 in the order NE, NW, SE, SW, masks 10, 9, 6, 5). The Cave model of `pieceBase` + mask, a one-exit entrance and a path from start to end cannot be expressed. The `0x3e69e0` case for LevelType 32 is unrecorded.

A tileset entry would be a fixed 2 x 2 ring needing a new path in `planMaze` (as the spider cavern), plus a choice of which corner variant carries the entrance. Each room is 40 tiles, so a plan cell would be 5 x 5 engine rooms of 8 (a size the probe has not seen).

## Risk, effort, checks

- Risk: high for the value: a fixed four-room ring, and the Boss's own preset (level 124) is not part of it.
- Effort: L (same blocker as the spider cavern); recommend deciding with it whether fixed-ring tilesets are in scope at all.
- Game checks: the LevelType 32 case of `0x3e69e0`; a slot built as the ring; the entrance via a stair variant; Boss and event placement in a ring; Hell run.
