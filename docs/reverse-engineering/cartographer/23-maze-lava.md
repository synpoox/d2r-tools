## 23. The maze builder's case for LevelType 28 (Lava, River of Flame, issue #239)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. All of it is static (the dump read with
`plugins/cartographer/tools/dump_dis.py`); nothing here was run in game. No new hook or signature: the maze hooks of section 5 are reused.

| Fact | Evidence | Level |
|---|---|---|
| The maze builder `0x3e69e0` switches on `level+0x1e0` (LevelType) minus 3 through a 33-entry jump table of dword RVAs at `0x3e76d8`. LevelType 28 (index 25) goes to `0x3e6c31`. | `0x3e6a4d..0x3e6a73`; table read from the dump | static |
| `0x3e6c31` is `mov rcx, rbx; call 0x3e96d0; jmp 0x3e75bc`: the maze walk, then the common tail. It does not call the ring grower `0x3e9820` (LevelTypes 7 and 8 do, at `0x3e6c24`/`0x3e6c3e`, and 8 also walks and places). So `callsRingGrower` is false. | `0x3e6c24..0x3e6c39` | static |
| The Lava case makes no call to the entrance and exit placer `0x3eb850`, so no "Prev", "Next" or "Down" piece is placed: the entrance is an ordinary room, as for a tileset with no special entrance (`MazeTileset.prevPiece` all 0). Cave (index 0, `0x3e6a75`) and Crypt (`0x3e6b49`) do call it. | same code | static |
| The tail `0x3e75bc` runs the normaliser `0x327880` for every level except ids `0x1c` and `0x6b`. | `0x3e75bc..0x3e75e4` | static |
| The piece picker `0x3eb960` builds an exit mask (W 1, E 2, S 4, N 8, the same convention as Cave and Crypt) and, for LevelType 28 (jump table `0x3ebbfc`, entry `0x3ebb1a`), adds `0x344` = 836. So a room's piece is `836 + mask`. | `0x3eb973..0x3ebb1a` | static |
| That matches the `lvlprest.txt` rows: 836 is the closed cell, 837..851 are the fifteen mask pieces in W, E, EW, S, SW, SE, SEW, N, NW, NE, NEW, NS, NSW, NSE, NSEW order, which is mask order 1..15. So `pieceBase` is 836 and the mask-to-row lookup table that `a4-lava.md` feared is not needed on the maze path. | `docs/cartographer/tilesets/a4-lava.md` | static |

Consequences for the table row `{"Lava", 28, "MazeLava", "Cartographer Lava", 24, 836, {0,0,0,0}, false}`:

- Room size 24 (the vanilla Lava 1 `lvlmaze` rows are 24 x 24 pieces); like a Cave cell, one plan cell is a 3 x 3 block of 8-tile engine rooms, so the arrival and event rooms use the middle room (`third` in `maze_hooks.cpp`).
- No `Prev` piece is placed, so the arrival has no special room: `MarkArrival` marks the room, as for any ordinary entrance.
- The Forge (853, 854) and the bridges (855, 856) are not in the mask family and the maze never picks them: the Boss stands in an ordinary far room, not the Forge.

Open (game check): the DS1 edge scan of `a4-lava.md` says piece 839 (EW) is about 36 percent open on its north side and 845 (NW) about 7 percent on its south side. In a maze the neighbour is absent
on those sides, so they should open onto lava or void, not onto a room; to confirm in game. Also: that the lava edge blocks movement with no extra block, and that the entrance room (no Prev piece) places the player in the planned room.
