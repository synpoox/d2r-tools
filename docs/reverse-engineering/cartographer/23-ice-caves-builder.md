## 23. The Ice caves case of the maze builder (issue #245)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. **Evidence level: static** (Capstone over the dump); not run in a game by this section.

| Field | Content |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer, the `Ice` row of `MazeTilesets` (`src/maze/maze_tilesets.h`) |
| Module | `D2R.exe` |
| Location | maze builder `0x3e69e0` (section 5); its jump table at `0x3e76d8`; the LevelType 33 case at `0x3e7218`; the call table that case reads at `0x1d0fb00` |
| Guard | None new: the builder, the ring grower `0x3e9820`, the walk `0x3e96d0` and the special-room call `0x3eb850` are already signed (section 1a) |
| Evidence | `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e7218 130`; the jump table and call table read from the dump bytes (`struct.unpack` over `0x3e76d8` and `0x1d0fb00`) |
| Failure mode | A wrong `callsRingGrower` would leave the ring grower running over the plan (rooms the plan does not hold); the build then differs from the plan and the hook falls back or the check in `cartographer check` logs "built differently". Nothing new is written to the game |

### Which case LevelType 33 takes

The builder switches on the slot level's LevelType (`[level+0x1e0] - 3`, range 0..0x20, a jump table of 33 entries at `0x3e76d8`). LevelType 33 jumps to `0x3e7218`; Cave (3) to `0x3e6a75`, Crypt (4) to `0x3e6b49`, 28 (River of Flame) to `0x3e6c31`, 32 to `0x3e73c1`, 34 to `0x3e7478`, 35 to `0x3e74ee`. Ice is not the Cave case.

The case at `0x3e7218`, for a level id that is none of the game's own Ice levels (the id is at `[level+0x1f8]`; the case special-cases 113 to 116, 118 and 119 only, and a slot level is none of them):

1. **Calls the ring grower `0x3e9820(level, 2)`** first (`0x3e7239..0x3e7241`). Cave and Crypt do not.
2. Calls the walk `0x3e96d0(level)`.
3. Draws `rot` from the level's random state (`& 3`) and calls `0x3eb850(level, table + rot*16, &rot)`, then `table + (rot+4)*16` and `table + (rot+8)*16` (`0x3e7294`, `0x3e72b0`, `0x3e72cc`): three special-room calls. For the game's levels 115 and 116 a fourth call (`+0xc`) is added; a slot level gets three.
4. Jumps to the common tail `0x3e75bc` (`0x327880`, which moves the rooms so the box starts at the origin), as Cave does.

So the walk follows the ring grower (`walkFollowsRing` stays true) and the ring grower is a no-op for a steered plan: `callsRingGrower = true`. The existing `RingHook` (section 1a) already skips it when the plan says so.

### The call table at `0x1d0fb00` (16 bytes a row: from, to, file, dir)

| Rows | Group | rows in order: `from` -> `to` (dir) |
|---|---|---|
| 0..3 | Prev (entrance) | 1010 -> 1021 (3), 1004 -> 1019 (0), 1006 -> 1020 (1), 1003 -> 1018 (2) |
| 4..7 | Next | 1010 -> 1025, 1004 -> 1023, 1006 -> 1024, 1003 -> 1022 |
| 8..11 | Down | 1010 -> 1029, 1004 -> 1027, 1006 -> 1028, 1003 -> 1026 |
| 12..15 | Theme | 1010 -> 1033, 1004 -> 1031, 1006 -> 1032, 1003 -> 1030 |

`from` is the one-exit mask piece the call looks for (base 1002 + mask: 1003 W, 1004 E, 1006 S, 1010 N); `to` is the special piece it turns that room into. By the side the `from` room opens to, the Prev group is `{1018 W, 1019 E, 1020 S, 1021 N}`, the same order as Cave's `{83, 84, 85, 86}` (rows at `0x1d0ee30`: 60 -> 86, 54 -> 84, 56 -> 85, 53 -> 83). That is the entrance family of the `Ice` row. The Next and Down pieces are never placed: Cartographer answers every call whose `to` is not a Prev piece, and every Prev call after the first, with a suppression (`steerSpecial`).

### Not covered

- Whether a 16-tile cell is split by the engine into 2 x 2 rooms of 8 (as a Cave cell of 24 is 3 x 3): see the check record `docs/cartographer/checks/` for Ice.
- The waypoint piece of the game's Ice levels (1034..1037) is placed by the special-room calls for the game's levels only; none of the three calls above asks for it.

### Live check (2026-11-12, slot 11, Hell)

A `Glacial Veins` Ice map built as planned (19 rooms, 0 refits), arrival logged in plan cell (0,4) at (6400,64), the boss room (6496,0) and event room (6496,48) found as 8 x 8 DRLG rooms, so a 16-tile cell is the expected 2 x 2 engine rooms. No fault. Record: `docs/cartographer/checks/2026-11-12-ice-caves-check.md`.
