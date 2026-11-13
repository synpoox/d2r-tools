## 23. The cube sets a map's level (issue #330)

A cube recipe with `mod 1` = `map-level` (the `properties.txt` row `tools\install.ps1` adds) set the `cartographer_map_level` stat in the cube's own test but
never put it on a map output (`docs/cartographer/tables.md`, issue #310). The plugin hooks the one function every item property goes through and corrects the
stat after the game's own apply (`src/engine/item_stats.cpp`, pure decision in `src/statplan`).

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #330) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Method | Static disassembly of the memory-layout image (`dump_dis.py`), then a logging hook in slot 6 |

### The chain (static)

| Step | RVA | What |
|---|---|---|
| Cube executor | `0x5269c0` | Builds the output items. Its mod loop (`0x527970` to `0x527a02`) walks the recipe's mod entries (stride `0xc`: property id dword at `-0xa`, param, min, max as words at `-6/-4/-2`, chance byte at `0`). An entry is skipped when its enabled bit (`0x3d40a0`, bit 15 of the first word) is clear or the chance roll fails. Each entry becomes a `Property` of four dwords `{id, param, min, max}` on the executor's stack |
| `ApplyCubeMod` | `0x3d3420` | `(item, Property*, flag)`: fixed arguments `(dataFlag, 7, 0, item, Property*, 0, 0, 0, Property*, 0, 0x40, 0)` into `ItemPropApply`. A short `je` sits inside its first 15 bytes, so it is not hooked |
| `ItemPropApply` | `0x3d5060` | The property applier, 12 parameters (four in registers, eight on the stack; the `Property*` is the fifth and the ninth). Hooked: 14 bytes, no relative operand inside them. It reads a word from the item's table row (`0x3730d0`, row `+0x40`); a value of 1 or more goes to `0x3d4620` (the `properties.txt` row, its seven function and stat columns), else it dispatches by property id through a 16-byte table |
| `ItemListMake` | `0x3d5230` | `(owner, item, state, flags)`: finds the item's list with those flags, else makes it. Called with flags `0x40` (the base magic list) |
| `SetListStat` | `0x2f7c00` | The D2RLoader thunk the map stats already use `(dataFlag, list, stat, value, layer)` |

Signatures (offset 0 = function entry; unique in the image, checked by `tools/check_portal_signatures.py`):

- `ItemPropApply`: `48 89 6C 24 20 56 41 56 41 57 48 83 EC 60 0F B6 F1 49 8B E9 49 8B C9 4D 8B F0 44 8B FA E8 ?? ?? ?? ?? 66 83 F8 01 72`; hook size 14, first relative operand (the call) at `+29`.
- `ItemListMake`: `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 45 8B F8 41 8B D9 48 8B F2 44 8B C3 41 8B D7 48 8B F9 48 85 C9 74 ?? E8 ?? ?? ?? ??`; only called.
- `SetListStat` is the signature of section 7.

### Cube executor details (seen in game)

- `CubeExec`'s second argument is the **player**, not the cube. The plugin walks the player's inventory and keeps the items whose item-data page byte (`[[unit + 0x10] + 0x55]`) is 3, the cube page.
- The game frees the input items during the output step, so the input map's level is read when `CubeExec` is entered (the second hook, 21 bytes).
- The cube reuses the input map's unit for a map-keeping output, so the game's own apply **adds** to the stat that is already there (10 then 20). The hook confirms that and only sets the stat when the result differs from "old level plus the recipe's value".
- The `properties.txt` row `map-level` is game property id **284** (the row number the game counts from the table, not 285).
- An item's stat list (`ItemListFind(item, 0, 0x40)`) holds its entries at `[list + 0x30]` with the count at `[list + 0x38]`. Each entry is 16 bytes `{int layer, int stat, int value, int pad}`. `ItemListCopy` returns only `{int, int}` pairs and no value, so the plugin reads the raw entries (this corrects the pair format guessed in section 14: the pair has no value).

### Behaviour

- **Fail open:** a part off, or `properties.txt` / `itemstatcost.txt` without the rows, leaves the hook installed but idle (the property id is `-1`), and the game's own apply runs unchanged. The hook is installed only when `ItemListFind`, `ItemListMake`, `SetListStat` and `ItemPropApply` are all found.
- **What it does:** for a call whose `Property` id is the `map-level` row, it reads the map level stat from the item's base magic list, lets the game apply, reads it again and, when the result is not "the old level plus the recipe's value", sets it (`statplan::cubeMapLevelTarget`, unit tested). The stat is cut to 1 to 511 (9 bits).

### Evidence

Slot 6, Hell, test save, a build with extra logging, `usetype,nor,mod` and `t22,mod` recipes (map with a Town Portal scroll). Both hooks logged `hooked` at load. First transmute: the stat went to 10. Second transmute: stat before 10, after 20, cube input 10, target 20, read back 20. Using the map logged `map level 20 (from the item's stat)` and opened a portal (CaveB). The test slot's `maps.txt` was edited for the run (three Cave maps) and restored. Record: `docs/cartographer/checks/2026-11-12-cube-map-level.md`.

### Not checked

A `Map Level` line on the hover of a map item (map-class items show no property lines; it shows on non-map outputs, section 15). A transmute with the final cleaned DLL (the runs used the same logic with extra logging; the final DLL loaded and hooked).
