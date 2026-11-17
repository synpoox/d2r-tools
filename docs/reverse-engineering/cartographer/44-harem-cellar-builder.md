## 44. The maze builder's case for LevelType 14 and 15 (Act 2 Harem and Palace Cellar, issues #277 and #278)

Build: D2R 3.3 build 93847. Addresses are RVAs of that build. Evidence: **static** (the builder `0x3e69e0` and its jump table, as in note 39; the picker `0x3eb960`; `lvlprest.txt`), then **in game** (slot 10, Hell, the `HaremTest` and `CellarTest` rows).

| Fact | Evidence | Level |
|---|---|---|
| LevelType 14 (Harem) and 15 (Basement) share one case. It calls only the ring grower `0x3e9820(level, 2)` and then the common tail: no walk, no special rooms. So the level is a bare 2x2 ring. | the case in the jump table | static |
| The ring has four cells with exit masks 5 (W+S), 6 (E+S), 9 (W+N) and 10 (E+N). The picker `0x3eb960` takes the piece from a LevelType table by mask: Harem 356 SW, 355 SE, 357 NW, 354 NE; Basement 360 SW, 359 SE, 361 NW, 358 NE. Other masks give nothing. | `0x3eb960` | static |
| The piece is a pure function of the exit mask, so a plan that sets the right mask per cell gives each cell its planned corner piece. | picker | static, in game |
| `lvlprest.txt` `Files` is 2 for all eight corner presets, so a random pick never chooses the waypoint DS1 (File3). Level 52 pins it on piece 361, and level 54 pins File4 (CelSE3/CelNW3, not a waypoint) on 359 and 361; a slot level gets file -1 (random). So no waypoint appears in a map and no waypoint hook is needed. The issue text and `a2-palace-cellar.md` ("waypoint variants in the rotation") were wrong. | `lvlprest.txt`, picker | static, in game |
| A 16-tile cell splits into 2x2 engine rooms of 8 tiles, as for Ice (the log says "room 11 of 16"). | log | in game |

Consequence: the rows set `ringPieces` (NE, SE, SW, NW) and `pieceBase` 0. The plan is one fixed ring (`fixedRingLayout`): the seed picks the entrance cell and which neighbour is the event room, the Boss is the opposite cell, so entrance, event and Boss are three distinct rooms. The ring grower's hook (`RingHook`) builds the steered plan itself (`graphInRing`) and never resumes the original after a partial build. Shape and Size are ignored (one shape) and the loader says so on the console and in the log. Offsets: Harem x 2000, y 500; Basement x 2500, y 500.
