# Act 5 red portal temples, Hell 1-3 (issue #291)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 125, 126, 127 Hell 1, 2, 3 (Abaddon, Pit of Acheron, Infernal Pit). Level 135 Pandemonium 3 shares the LevelType |
| LevelType / DrlgType | LevelType 35, named "Act 5 - Lava" in LvlTypes (it reuses the Act 4 lava floor DT1 and has 12 tile files); DrlgType 1; palette 3. The Act 4 "Lava" tileset is a different LevelType (28) |
| Levels row facts | `Position` 1 in all three (and 135), no Warp or Vis links (entered by red portal). The slot row needs Position 0 (05-maze-slot.md Q3 note), so the slot copy differs from the vanilla row here |
| `lvlmaze.txt` | Rooms 3 in all difficulties, SizeX = SizeY = 24, Merge 500 |
| `lvlprest.txt` room pieces | Six rows, Def 1053-1058, 24 x 24, 2 files each, flagged Outdoors: Lava N, S, E, W, EW, NS (listed in the order N, S, E, W, EW, NS). That is four dead ends and two straights only |
| Entrance, exit, stairs | No Prev, Next or Down rows; no waypoint |
| Dead ends | The four one-exit pieces |

## Does the Cave/Crypt path work unchanged?

No.

- **Piece set.** Only masks 1, 2, 3, 4, 8, 12 exist, in an order that is not base + mask (N is Def 1053 but mask 8). So the plan can only be a straight line, with no turns, branches or loops, and `pieceBase` + mask cannot name the pieces. The vanilla level has Rooms 3, which fits a three-room line (dead end, straight, dead end), an inference, not read from the engine.
- **No entrance piece.** The entrance would be an ordinary dead-end room; the arrival finder then picks the centre room or a random one, so arrival in the entrance cell is not given.
- **Builder case.** LevelType 35's case in `0x3e69e0` is not recorded; the outdoors flag on the pieces and Position 1 suggest it is not the Cave-style walk.
- **Size.** A line of up to 15 rooms (24 tiles each) fits in 360. The vanilla levels are three rooms, so a long Lava line has never been built by the game.

## Risk, effort, checks

- Risk: high. Little layout freedom (a straight line only), odd piece order, no entrance piece, Position 1 vanilla rows, unknown builder case.
- Effort: L; low payoff. Recommend closing or deferring unless the straight-line map is wanted.
- Game checks: the LevelType 35 case of `0x3e69e0`; a slot with Position 0 building a line longer than three rooms; arrival; the Boss and event at the two ends; Hell run.
