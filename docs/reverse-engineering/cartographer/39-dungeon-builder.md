## 39. The maze builder's case for LevelType 24 (Act 3 Dungeon, issue #244)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e6fa5 25`, the jump table `0x3e76d8` and the call table read from the dump bytes). No new hook or signature.

| Fact | Evidence | Level |
|---|---|---|
| LevelType 24 is jump-table index 21 (`24 - 3`); the entry is `0x3e6fa5`. | dword at `0x3e76d8 + 21 * 4` | static |
| The case calls the ring grower `0x3e9820(level, 2)`, then the walk `0x3e96d0`. So `callsRingGrower` is true, and the walk follows the ring (`walkFollowsRing`), as for Ice. | `0x3e6fa5..0x3e6fb5` | static |
| It then draws `rot = rng & 3` from the level's random state (`+0x1e4`/`+0x1e8`) and makes **two** `0x3eb850(level, 0x1d0f770 + row * 16, &rot)` calls: row `rot` (`0x3e7000`), then row `rot + 4` (`0x3e6b3f`, reached by `jmp 0x3e6b2e`), then the common tail `0x3e75bc`. | `0x3e6fba..0x3e700b`, `0x3e6b2e..0x3e6b44` | static |
| The table rows at `0x1d0f770` (from, to, file, dir): rows 0..3 are 672 -> 698, 666 -> 696, 668 -> 697, 665 -> 695 (the Prev pieces 695..698, W E S N); rows 4..7 are 672 -> 702, 666 -> 700, 668 -> 701, 665 -> 699 (the Next pieces 699..702). The Prev pieces carry the level's Up stairs, so the row has no Prev family and the special-room hook suppresses both calls (#455). | table bytes | static |

Consequence: the row is `{"Dungeon", 24, ..., 14, 664, {0,0,0,0}, true, 0, 0, 0}`. The ring grower is a no-op for a steered plan (`RingHook`), no filler ring is planned (`ringCells` 0) until the game check shows void at the edges. 14 is not a multiple of 8: see the game check recorded in the pull request for how the engine splits a 14-tile cell.
