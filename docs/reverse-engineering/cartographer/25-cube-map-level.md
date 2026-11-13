## 25. The cube sets a map's level (issue #330)

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
- `CubeExec`: `40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 08 EA FF FF B8 F8 16 00 00 E8 ?? ?? ?? ??` (31 bytes, unique); hook size 21 (the push sequence and `lea rbp,[rsp-0x15f8]`, no relative operand inside the 21 bytes; the first, a call, is at `+26`). The byte guard is the 21 hooked bytes, pinned in the signature, so a build whose prologue differs leaves the part off and the hook is not installed.
- The three walkers the cube step uses to find the cube's items are only called:
  - `UnitInventory` `0x34a360`: `48 89 5C 24 18 56 48 83 EC 20 48 8B F1 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B 9E A0 00 00 00` (unit to its inventory).
  - `InventoryFirst` `0x388c10`: `40 53 48 83 EC 20 48 8B D9 48 85 C9 74 ?? 81 39 04 03 02 01 74 ??` (inventory to its first item).
  - `InventoryNext` `0x38aba0`: the 130-byte signature in `part_signatures.h` (item to the next item).
- All of them are checked by `tools/check_portal_signatures.py` against the memory-layout image and, when one is missing, the hook is not installed and the game's own apply runs unchanged (ItemPropApply still hooked: the stat is then only the game's).
- `SetListStat` is the signature of section 7.

### Cube executor details (seen in game)

- `CubeExec`'s second argument is the **player**, not the cube. The plugin walks the player's inventory and keeps the items whose item-data page byte (`[[unit + 0x10] + 0x55]`) is 3, the cube page.
- The game frees the input items during the output step, so the input map's level is read when `CubeExec` is entered (the second hook, 21 bytes).
- The cube reuses the input map's unit for a map-keeping output, so the game's own apply **adds** to the stat that is already there (10 then 20). The hook confirms that and only sets the stat when the result differs from "old level plus the recipe's value".
- The `properties.txt` row `map-level` is game property id **284** (the row number the game counts from the table, not 285).
- The item's stat list is read through `ItemListFind` and `ItemListCopy` with the layout guard of section 16 (`decodeItemStatList`): 16-byte entries `{layer, stat, value, 0}` (D2RLoader's replacement writes the value; section 16 corrects the earlier 8-byte pair reading).

### Behaviour

- **Fail open:** a read of the map's level that is not trustworthy (a part missing, the list layout not recognised, more stats than the plugin reads and the level not among them, the input map's level not found) leaves the game's own result as it is; a 0 that was read (a fresh map without the stat) counts, an unread level does not (`cubeMapLevelTarget` takes `levelKnown`, unit tested). A part off, or `properties.txt` / `itemstatcost.txt` without the rows, leaves the hook installed but idle (the property id is `-1`), and the game's own apply runs unchanged. The hook is installed only when `ItemListFind`, `ItemListCopy`, `ItemListMake`, `SetListStat` and `ItemPropApply` are all found.
- **What it does:** for a call whose `Property` id is the `map-level` row, it reads the map level stat from the item's base magic list, lets the game apply, reads it again and, when the result is not "the old level plus the recipe's value", sets it (`statplan::cubeMapLevelTarget`, unit tested). The stat is cut to 1 to 511 (9 bits). With a wide range (`min < max`) a game result that already lies in the range counts as the game's own roll and stands, so such a recipe can add less than a fixed one (never twice). The recipe's input level is added to any output with a `map-level` mod, a new map or the Hand Axe from `useitem,mod`.

### Evidence

Run 1: slot 6, Hell, test save, a build with extra logging, `usetype,nor,mod` and `t22,mod` recipes (map with a Town Portal scroll). Both hooks logged `hooked` at load. First transmute: the stat went to 10. Second transmute: stat before 10, after 20, cube input 10, target 20, read back 20. Using the map logged `map level 20 (from the item's stat)` and opened a portal (CaveB). The test slot's `maps.txt` was edited for the run (three Cave maps) and restored. Record: `docs/cartographer/checks/2026-11-12-cube-map-level.md`.

Run 2 (the final DLL of this head, slot 5, Hell, god mode, test save; recipe `t21` + `tsc`, `usetype,nor,mod`, `map-level` 10): a fresh map, first transmute: `stat before 0 after 10, cube input 0, target 10, now 10`; second: `stat before 10 after 20, cube input 10, target 20, now 20`; the map used in Harrogath logged `map level 20 (from the item's stat)`, the map monsters took Level 20 and CaveB opened.

### Not checked

A `Map Level` line on the hover of a map item (map-class items show no property lines; it shows on non-map outputs, section 15). Recipe forms other than `usetype,nor,mod` with the final DLL, and a multiplayer or client-side view.
