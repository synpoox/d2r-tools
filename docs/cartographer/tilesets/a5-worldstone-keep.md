# Act 5 Worldstone Keep and Throne of Destruction (issues #292 and #293)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied. #292 and #293 share LevelType 34.

## #292 Worldstone Keep (levels 128-130)

| Item | Finding |
|---|---|
| Levels | 128, 129, 130 Baal Temple 1, 2, 3 (Worldstone Keep levels 1-3) |
| LevelType / DrlgType | LevelType 34 "Act 5 - Baal" (six DT1 files); DrlgType 1; palette 4 |
| `lvlmaze.txt` | Rooms 9 in all difficulties, SizeX = SizeY = 16, Merge 500 |
| `lvlprest.txt` room pieces | Def 1059-1073: the 15 exit-mask pieces, 16 x 16, 3 files each (`pieceBase` 1058, base + mask holds) |
| Entrance (Prev) | Def 1074-1077: Prev NEW, NSE, NSW, SEW, three-exit T pieces (2 files each), not one-exit |
| Exit (Next) | Def 1078-1081 Next N, S, E, W: one-exit (2 files each). Not placed |
| Dead ends, specials | One-exit mask pieces; Waypoint N, S, E, W (Def 1082-1085, one-exit) |
| Warps | Up L/R (81), Down L/R (82) |

**Cave/Crypt path.** Piece set: yes. Entrance: no, the same problem as the Kurast sewers, but with three exits. `Tileset::prev` is keyed by a single open side; the Baal Prev pieces are keyed by the closed side (the missing exit), so the family needs a mask-keyed lookup and the layout generator needs an entrance cell with three exits (a change to `layout.cpp`, which means pinned layout tests move). The alternative (no Prev piece, ordinary entrance) loses the arrival in the entrance cell (arrival falls back to the centre or a random room). Also the shared work: a slot per tileset and the unrecorded `0x3e69e0` case for LevelType 34 (ring grower, walk). At 360 tiles 21 rooms fit across.

- Risk: medium. Piece set is proven in kind; the T-shaped entrance is a core change.
- Effort: M.
- Game checks: the LevelType 34 case of `0x3e69e0`; entrance arrival with a T Prev; built grid equals plan; Boss and event rooms; Hell run.

## #293 Throne of Destruction (levels 131 and 132)

| Item | Finding |
|---|---|
| Levels | 131 Throne Room (DrlgType 2, 40 x 52) and 132 World Stone Chamber (DrlgType 2, 55 x 55, Position 1) |
| LevelType | 34, shared with #292 |
| `lvlmaze.txt` | No rows (presets) |
| `lvlprest.txt` | One row each: Def 1086 (ThroneRoom, level 131) and Def 1087 (WorldStone, level 132), one DS1 each, size 0 (the file sets it) |

**Verdict: not feasible on the maze path.** Each is one fixed DS1, with no mask pieces, no Prev or Next family and no dead ends, so there is nothing to generate or steer. The only route is to build the preset as a whole fixed map, but the plugin no longer has preset maps (`core/tables.cpp`: "Preset maps are gone"), and the plan lists Baal under fixed areas. Recommend closing #293 as not feasible, or reopening it with the preset route if that is ever restored.

- Risk: n/a. Effort: not applicable (close), or L if preset maps return.
