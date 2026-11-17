## 39. The Jail case of the maze builder (issue #273)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. **Evidence level: static** (Capstone over the dump); the live result is in the PR of #273.

| Field | Content |
|---|---|
| Date | 2026-11-17 |
| Consumer | Cartographer, the `Jail` row of `MazeTilesets` (`src/maze/maze_tilesets.h`) |
| Module | `D2R.exe` |
| Location | maze builder `0x3e69e0` (section 5); its jump table at `0x3e76d8` (index = LevelType - 3); the LevelType 8 case at `0x3e6c3e`; the call table it reads at `0x1d0f0b0` |
| Guard | None new: the builder, the ring grower `0x3e9820`, the walk `0x3e96d0` and the special-room call `0x3eb850` are already signed (section 1a) |
| Evidence | `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e6c3e 60`; the jump table and call table read from the dump bytes (`struct.unpack`) |
| Failure mode | A wrong `callsRingGrower` leaves the ring grower running over the plan; the build then differs from the plan and `cartographer check` logs "built differently". Nothing new is written to the game |

### The case

Entry 5 of the jump table is `0x3e6c3e`:

1. Calls the ring grower `0x3e9820(level, 2)`, then the walk `0x3e96d0(level)`: `callsRingGrower = true`, `walkFollowsRing` stays true (as Ice, note 24).
2. Draws `rot` (`& 3`) and calls `0x3eb850(level, table + rot*16, &rot)`: the Prev group.
3. Only for the game's own levels 29, 30 and 31 (level id at `[level+0x1f8]`) it adds further calls (row `rot+12` for 29, `rot+16` for 30, `rot+4` for 31). A slot level is none of them.
4. Then, for every level, one more call with row `rot+8`: the Next group. Then the common tail `0x3e75bc`.

So a slot level gets two special-room calls, Prev and Next; both are answered with a suppression.

### The call table at `0x1d0f0b0` (16 bytes a row: from, to, file, dir)

| Rows | Group | `from` -> `to` (dir) |
|---|---|---|
| 0..3 | Prev | 213 -> 239 (3), 207 -> 237 (0), 209 -> 238 (1), 206 -> 236 (2) |
| 4..7 | Cath (Cathedral link) | 213 -> 247, 207 -> 245, 209 -> 246, 206 -> 244 |
| 8..11 | Next | 213 -> 243, 207 -> 241, 209 -> 242, 206 -> 240 |
| 12..15 | Waypoint | 213 -> 251, 207 -> 249, 209 -> 250, 206 -> 248 |

`from` is the one-exit mask piece (base 205 + mask: 206 W, 207 E, 209 S, 213 N). Pitspawn (252..255) has no row in these 16. Prev holds the Up stairs (note 35), so the row has no entrance family; Cath, Waypoint and Pitspawn are not asked for by a slot level's calls.
