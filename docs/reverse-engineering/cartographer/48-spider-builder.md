## 48. The maze builder's case for LevelType 23 (Act 3 Spider Cavern, issue #284)

Build: D2R 3.3 build 93847, image stamp `0x6ab3782c`. Addresses are RVAs of that build. Evidence: **static** (`dump_dis.py` on the dump, as note 44: the jump table `0x3e76d8`, the picker `0x3eb960` and its table), then **in game** (slot, Hell, the `SpiderTest` row).

| Fact | Evidence | Level |
|---|---|---|
| LevelType 23 is jump-table index 20 of the builder `0x3e69e0`; its entry is `0x3e6ed6`, the same case as LevelType 14 and 15 (Harem, Basement): `0x3e9820(level, 2)` then the common tail `0x3e75bc`. No walk, no special rooms. So the level is a bare 2x2 ring, as Harem. | dword at `0x3e76d8 + 20 * 4` | static |
| The picker `0x3eb960` takes LevelType 23 through its jump table (`0x3ebbfc`, entry `0x3ebab6`): piece = the dword at `0x1d0ebf8 + mask * 12`. Masks 5, 6, 9, 10 give 659 (SW), 660 (SE), 661 (NW), 662 (NE); every other mask gives 0. | table bytes | static |
| After the lookup the picker swaps by level id: on level 84 (Spider 1) piece 662 becomes 664 (Chest NE), on level 85 (Spider 2) piece 661 becomes 663 (Chest NW). A slot level has neither id, so the two Chest pieces (663, 664) are never picked on a map: no chest room, no vanilla chest logic, and no chest in place of the Boss room. | `0x3ebab6..0x3ebaff` | static |
| So the row is the Harem recipe with other ids: `ringPieces` NE, SE, SW, NW = 662, 660, 659, 661, `pieceBase` 0, one fixed ring (`fixedRingLayout`), Shape and Size ignored with the console and log note. | | static |

Offset: x 4000, y 500 (the next free x in the second band; the band's x 0..3500 is taken by Jail, Lair, Barracks, Kurast Sewers, Harem, Palace Cellar and the rows of #243 and #275). It is inside the automap range and overlaps no row.
