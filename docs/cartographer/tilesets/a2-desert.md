# Act 2 desert (issue #241)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 41-44 Desert 1-4, 80 x 80; 45 Desert 5, 32 x 32 (no substitutions); 46 Valley of the Kings, 80 x 80. All DrlgType 3 |
| LevelType | 16 "Act 2 - Desert" (ground, rocks, dune, big cliff and mesa files). The palette is Act 2 |
| `lvlprest.txt` | Def 362-413 (52 rows). 362-363 transitions (8 x 56 and 56 x 8); 364-375 border pieces (12, 8 x 8); 376-387 cliff walls, ends, paths and corners (16 x 8, 8 x 16, 16 x 16; the King-tomb variants are for the Valley); 388-391 tomb and lair mouths; 392-394 Valley ruins and warp; 395-397 oasis (8, 16, 16 tiles); 398-407 8 x 8 fills (mesa, head, bone, wagon, berms, up to 5 files each); 408-413 ruins (16 x 16, 16 x 8, 8 x 16, 8 x 8, sewer, Dark Elder) |
| `lvlsub.txt` | Rows 16-27 (SubType 9, SubTheme 0-4): scatter, waypoint and shrine, `TileSub` driven |
| Waypoints | Levels 42-44 and 46: numbers 11, 13, 14, 17 |
| Slot row | New: LevelType 16 |

## What the planners would need

The same family as Wilderness: border ring plus open fills. Desert has the largest all-8 x 8 fill set (398-407), so packing is simple; cliff and Valley pieces are optional.

- **Planner.** Share the Wilderness "frame plus fill" planner as a data table (border ids, fill ids and sizes). Only the catalogue differs.
- **Border.** The engine border pass lists LevelType 16, but the Act 5 builder skips it: route B as for Wilderness. All 12 border pieces are 8 x 8.
- **Cliffs.** Cliff walls (376-382) are straight 16-long runs with end and path pieces and block natively. A first version leaves them out; a later one may use them as an inner wall with a path piece as the gap.
- **Void.** Unstamped cells need void marks. The fill mode of LevelType 16 has an entry in the builder's mode lookup; confirm in game.
- **Left out.** Tomb and lair mouths (388-391) and the sewer ruin (412) lead into dungeon levels.

## Risk, effort, checks

- Risk: medium-low. The same new code as Wilderness, fewer pieces and no river.
- Effort: M-L; M if built after Wilderness on the shared planner.
- Game checks: fill mode of LevelType 16; whether the 8 x 8 border files meet cleanly at corners when picked by file index; whether DS1-embedded Valley and Lost City props spawn in the slot level; the void edge against an Act 2 palette.


## As built (#505)

Built on the frame-plus-fill planner: slot `OutdoorDesert`, LevelType 16, 128 x 112 tiles (14 x 12 fill cells plus the one-cell ring), ring files 364-371 (S, W, N, E, SW, NW, NE, SE) and one `Desert` fill set of the ten 8 x 8 pieces 398-407. Cliffs, tomb and lair mouths are left out.

Game check, Hell, level 99 Barbarian, PD2 Resurrected slot 3 (HD):

- Fill mode of LevelType 16: unstamped cells are not an issue; the plan covers every cell (`120 stamps, 0 void cells`, grid 120 of 120 covered, 0 void, 0 neither) and no black cell showed.
- Ring: the ring files meet at all four corners and along all four sides with no gap (rock walls closed on every side in a map and in an arena).
- Act 2 palette in an Act 5 level: right in HD (sand, Act 2 monsters spawn). Legacy graphics (`g`) show the Act 5 palette over the Act 2 tiles, red and green noise: HD only for now.
- Not checked: DS1-embedded Valley and Lost City props.
- Map plan `6674b179b8a1562b` (seed 7598453245661758433). Arena `Desert Test`, 3 x 3, 25 stamps, plan `e4e02db7a382dfea`.
- The installer's default Desert spot (6000, 4400) overlaps pd2-resurrected's Rathma stage 1 (6000, 4500), so the installer skips the level there; pass `-DesertOffsetY` (the check used 4350).
