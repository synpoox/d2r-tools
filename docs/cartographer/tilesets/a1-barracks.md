# Act 1 Barracks (#272)

Research only, nothing run in a game. Sources: vanilla 3.3.0 `levels`, `lvltypes`, `lvlmaze`, `lvlprest` (read only); engine facts from `docs/reverse-engineering/cartographer/05-maze-slot.md` and the DRLG steering research in pd2-resurrected (`docs/research/drlg-steering.md`).

One install-wide limit applies to every maze tileset: each maze tileset has a slot level of its own (a row of `MazeTilesets` in `src/maze/maze_tilesets.h`, installed by `install.ps1`; see `docs/cartographer/adding-a-tileset.md`), because a level's LevelType fixes its tileset.

| | |
|---|---|
| Vanilla level | 28 Barracks (the only maze level of its type) |
| DrlgType / LevelType | 1 (maze) / 7 Act 1 - Barracks (9 DT1 files in `lvltypes`) |
| `lvlmaze` row (level 28) | Rooms 10 (all difficulties), **SizeX 10, SizeY 14**, Merge 500 |
| Room pieces | `lvlprest` Def 168-182: Barracks W..NSEW, all 15 masks, 4 DS1 each. Def 167 "Court Connect" is the base slot (mask 0), the link to the Courtyard, not a room |
| Variants | 183-197 Theme (the engine's "+15" swap targets these) |
| Special rooms | 198-201 Next (stairs down), 202-205 Forge (Smith quest room). **No Prev family**: the vanilla entrance is the Court Connect, not a dead-end piece |
| Warps | level 28 Warp1 = 14 (Jail Down); Vis0 = Courtyard 1 |

## Does the Cave/Crypt path work unchanged?

No. Needed beyond a catalogue row:

1. **Non-square room.** `maze::Tileset` has one `roomTiles` (`LvlMaze SizeX = SizeY`), and `planCellRoom`, `capacityAt`, `levelTilesFor` and the plan's box check (`widthRooms * unit`) use one `unit`. Barracks is 10 x 14. Split `roomTiles` into width and height through `maze.*` and `steer.*` (unit tests pin the square case).
2. **Ring grower.** Level type 7 lays a 2 x 2 ring with `0x3e9820` and then walks. `RingHook` and `Tileset::callsRingGrower` exist; set it true (the hook is unproven on any tileset beyond Cave and Crypt).
3. **No Prev family.** `Tileset::prev.present = false` is supported (`planMaze` makes the entrance an ordinary room, and the arrival mark moves the level entry there). The portal room then has no stairs, which is fine for a map with no exit.
4. Pieces: `pieceBase` 167, all 15 mask pieces present, default `maskPieces`.
5. The engine uses its own room-normalise function for level 28 (`0x3ec2c0`, tested on the level id); a slot id should take the generic one (`0x327880`). Needs a game check.

## Risk, effort

Risk **medium-high**: first non-square room and first ring tileset together; 10 and 14 are not multiples of the 8-tile engine rooms, so the built-room box may not equal the plan's box (the arrival, Boss and event then refuse and fall back to the distance rule). Effort **M-L** (core change for the non-square room, installer value, `tables.cpp` ready list, tests, docs).

## Game checks

- Ring no-op: the built level has exactly the planned rooms (`MazeProbe 1`), no extra 2 x 2 ring.
- Built-room box equals the plan box for 10 x 14 cells (a Corridor, a Winding and a Loop shape).
- No Forge or Next piece appears; the "+15" Theme swap keeps exits.
- Arrival in the entrance cell, the Boss in its room, one event; the level 28 normalise path not taken.
