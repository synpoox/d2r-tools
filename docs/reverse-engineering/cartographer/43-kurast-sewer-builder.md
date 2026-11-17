## 43. The maze builder's case for LevelType 25 (Act 3 Kurast Sewers, issue #285)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis 0x3e7010 40` and `dis 0x3e71fa 8`, the jump table `0x3e76d8` and the call table read from the dump bytes). No new hook or signature.

| Fact | Evidence | Level |
|---|---|---|
| LevelType 25 is jump-table index 22 (`25 - 3`); the entry is `0x3e7010`. | dword at `0x3e76d8 + 22 * 4` | static |
| The case first compares the level id (`+0x1f8`) with `0x5c` (92, Sewer 1). Only level 92 takes the vanilla path (ring grower `(level, 5)`, a unit search for object `0x2c5`, `0x3e6880`). Any other level, so every Cartographer slot level, jumps to `0x3e71fa`. | `0x3e7010..0x3e701a` | static |
| `0x3e71fa` calls the ring grower `0x3e9820(level, 2)`, then the walk `0x3e96d0`, loads the table `0x1d0f7f0` and jumps into the same tail as the Dungeon case (`0x3e6fc1`): `rot = rng & 3` from the level's random state (`+0x1e4`/`+0x1e8`), a `0x3eb850(level, 0x1d0f7f0 + rot * 16, &rot)` call, then row `rot + 4` (`0x3e6b2e`), then the common tail `0x3e75bc`. So `callsRingGrower` is true and the walk follows the ring, as for Dungeon and Ice. | `0x3e71fa..0x3e7213`, `0x3e6fc1..0x3e700b`, `0x3e6b2e..0x3e6b44` | static |
| The rows at `0x1d0f7f0` (from, to, file, dir): rows 0..3 are 712 -> 742, 706 -> 740, 708 -> 741, 705 -> 739 (the Drain pieces 739..742, replacing the one-exit rooms N, E, S, W); rows 4..7 are 712 -> 746, 706 -> 744, 708 -> 745, 705 -> 743 (the Chest pieces 743..746). The Prev pieces 735..738 are never called by this case. | table bytes | static |

Consequence: the row is `{"KurastSewer", 25, ..., 12, 704, {0,0,0,0}, true, 1500, 500, 0}`. The special-room hook suppresses both calls (no Drain, no Chest piece is placed), the entrance is an ordinary mask room, and the ring grower is a no-op for a steered plan (`RingHook`). No filler ring (`ringCells` 0): piece 704 is not a closed Sewer piece (the catalogue has none), so there is nothing to ring with.
