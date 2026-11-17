## 41. The maze builder's case for LevelType 18 (Act 2 Maggot Lair, issue #279)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e6de8 40`, the jump tables `0x3e76d8` and `0x3ec264` and the call table read from the dump bytes). No new hook or signature.

| Fact | Evidence | Level |
|---|---|---|
| LevelType 18 is jump-table index 15 (`18 - 3`) of the builder `0x3e69e0`; the entry is `0x3e6de8`. | dword at `0x3e76d8 + 15 * 4` | static |
| The case calls the ring grower `0x3e9820(level, 2)`, then the walk `0x3e96d0`. So `callsRingGrower` is true and the walk follows the ring, as for Ice and Dungeon. | `0x3e6de8..0x3e6df8` | static |
| It then draws `rot = rng & 3` and, for a level id other than `0x40` (level 64, Lair Treasure; `[level + 0x1f8]`), makes **two** `0x3eb850(level, row, &rot)` calls: the Next row `rot + 4` of the table at `0x1d0f660`, then the Prev row `rot`. Level 64 makes its own two calls with the Treasure (482 -> 505) and Tight Spot (485 -> 509) rows. A slot level has its own id, so it takes the first path. | `0x3e6e27..0x3e6e79`, `0x3e6b31..0x3e6b44` | static |
| Table rows (from, to, file, dir): 0..3 are 489 -> 500, 483 -> 498, 485 -> 499, 482 -> 497 (the Prev pieces 497..500, N E S W); rows 4..7 are the same sources to 504, 502, 503, 501 (Next 501..504). The Prev pieces carry the level's Up stairs, so the row has no Prev family and the special-room hook suppresses both calls (#455). | table bytes | static |
| The Theme swap `0x3ebd90` (called from the common tail, `0x3e769b`) reads its own jump table at `0x3ec264` (index `LevelType - 3`). LevelType 18 maps to `0x3ec233`, the skip: no plan room is swapped. The Lair has no Theme family. | dword at `0x3ec264 + 15 * 4` | static |

Consequence: the row is `{"Lair", 18, ..., 10, 481, {0,0,0,0}, true, 500, 500, 0}`. The ring grower is a no-op for a steered plan (`RingHook`), no filler ring is planned (`ringCells` 0) (the game check showed cave walls, no void, at every edge). The engine keeps each 10-tile cell as one 10 x 10 room; it is not split.
