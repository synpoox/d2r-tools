# Act 4 lava fields, with the Chaos Sanctuary preset (issues #239, #287)

Research only. Vanilla tables of build 3.3.0 were read, nothing copied.

| Item | Finding |
|---|---|
| Levels | 107 Lava 1 (maze, DrlgType 1, 200 x 200); 108 Diablo 1 (DrlgType 3, 120 x 120). Both LevelType 28 |
| LevelType | 28 "Act 4 - Lava". The name "Lava" is already taken by Cartographer's maze tileset for this LevelType, so the outdoor tileset needs another name (for example "Lava fields") |
| `lvlprest.txt` | Def 836-862 (27 rows), all 24 x 24. 836 closed cell; 837-851 fifteen exit-mask pieces named by their open sides (W, E, EW, S, SW, SE, SEW, N, NW, NE, NEW, NS, NSW, NSE, NSEW), 2 files each; 852 warp (N); 853-854 Forge W and E; 855-856 bridges (1 and 4 files). 857-862 the Chaos Sanctuary: Entry, four Arms, Heart |
| `lvlsub.txt` | None (SubType -1) |
| Waypoint | Level 107 only: number 29 |
| Slot row | New: LevelType 28 (the maze tileset has its own slot in another table) |

## What the planners would need

The lava pieces have Jungle's shape: an exit-mask family. Only the cell size (24 x 24) and the ids differ.

- **Planner.** The Jungle layout-plus-picker. The rows are ordered by direction name, not by W/E/S/N bit order, so the mask-to-row mapping is an explicit lookup, not base plus mask. A 120 x 120 level is 5 x 5 cells, so a map is one screen of lava with branches.
- **Borders and void.** No border ring: the mask pieces close their own sides, as Jungle's do. Unstamped cells need void marks.
- **Lava edges.** Lava is encoded as blocked in the DT1 flags, so the edge blocks natively. A scan of the DS1 files shows two pieces whose edges do not match their names: the south side of NW (845) is about 7 percent open, and the north side of EW (839) about 36 percent. The picker should take exits from the walkable-edge scan, not from the name.
- **Bridges.** 855-856 are crossing pieces; a first version leaves them out.
- **One-off cells.** The Forge (853, 854) and the warp (852) can serve as boss or arrival cells.
- **Chaos Sanctuary (#287).** A 5 x 5 arrangement of Entry (857), Arms (858-861) and Heart (862), with the seals as objects. As a plan it is a fixed stamp list, but the seals depend on the Diablo script and on engine code that was not read here (`0x3fdc00`). See `a4-chaos-sanctuary.md`.

## Risk, effort, checks

- Risk: medium. The edge anomalies mean exits must come from the data; the Chaos Sanctuary seals are unproven.
- Effort: M for the lava fields (a Jungle-like picker); L for the Chaos Sanctuary, possibly not feasible without reading that code.
- Game checks: fill mode of LevelType 28 for an unstamped cell; whether 24-tile cells give `chooseCellRoom` its 8 x 8 rooms (3 x 3 per cell); the 839 and 845 anomalies against the exit masks; that the lava edge is impassable with no extra block; the Chaos seals and filler cells.

Status (#239): River of Flame is built as the maze row `Lava` (`MazeLava`, LevelType 28, pieces 836 + mask, no entrance family); see `docs/reverse-engineering/cartographer/23-maze-lava.md`. The outdoor Lava fields idea above is not built. Not tested in game.
