## 6. Map stats and events engine checks (issue #72)

Answers four static questions for plan phases 5 and 6 (applying map stats, density and level, events, entry text). Everything is read from the build 93847 dump (Capstone disassembly; Ghidra decompiles were a reading aid only). Nothing was run in a game. Each claim carries a level: **static** (read from the dump), **inference** (follows from static facts, not seen running) or **needs game** (with the exact check). "Left for the game" at the end lists what stays open.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer map stats (plan section 3.2), events (3.3, 3.3a) and entry text (4); no hook exists yet |
| Module | `D2R.exe` (the stat, text and attach thunks forward into `D2RCore.dll`) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the memory-layout dump `D2RLoader_6ab3782c_dump.exe` (file offset equals RVA) |
| Location | RVAs in the tables below. Region record (one per level, `0x9f0` bytes): `+0x00` byte, `+0x08` rooms populated, `+0x0c` group lookup (`-1` until first use), `+0x10` group count, `+0x14` groups (stride `0x34`, at most 13), `+0x2b8` MonDen, `+0x2bc` / `+0x2bd` MonUMin / MonUMax, `+0x2be`, `+0x2c0` level id, `+0x2c8` unique count, `+0x2d4` (-1), `+0x2d8`, `+0x2dc` / `+0x2e0` MonLvl. Region array: `*(game + 0x1d0)`, indexed by level id through `0x508740(array, level)` (no bounds check). Spawn block (`0x492cf0`): `+0` game, `+8` room, `+0x18` monstats id, `+0x1c` mode, `+0x24` x, `+0x28` y, `+0x2c` p7, `+0x30` u16 flags (`0x02` skips `0x4947d0`, `0x40` skips the D2RCore spawn hook). Stat list: `+0x1c` flags, `+0x20` state, `+0x30` entries, `+0x38` count, `+0x40` capacity, `+0x68` next; entry 16 bytes `{layer, stat, value, extra}` (entry layout from the toolkit: inference) |
| Guard | The masked prefix of each function is in the tables below; each matched exactly once on the dump unless the row says otherwise. The rule: a 14-byte patch at the entry (rounded up to a whole instruction) must contain no relative operand (RIP-relative memory, or a relative call or jump); "hookable" means it passes, "call-only" means the first relative operand is inside the patch. A "call" row is only called, with no hook planned, and was not rated. Thunk prefixes (`FF 25` and padding) that run on into the next function match only because of that neighbour, so they are call-only and must not be used as hook guards. Where a per-part guard does not match, log it and leave that part off |
| Contract | See Q1 to Q4 |
| Evidence | Capstone listings over the dump. Reproduce: `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]`, and a prefix check with `... pat "<hex with ??>"`. Prior art read, not copied: pd2-resurrected `plugin/common/pd2_engine.h` and `plugin/core-maps/map_stats.cpp`, d2rAI-toolkit `knowledge/runeword-stat-list.md`. Their addresses were re-checked here, not taken on trust |
| Validation | Static only. No game, no hook, not checked on Steam build 94995 |
| Failure mode | Fail open: a part whose signature or guard fails is logged and left off (stats, density, events or text), never replaced by a guess |
| Coexistence | `0x42e350` (PlayerTick) is already hooked by Cartographer and Better Auras hooks `0x42e600`; `0x44db20` is hooked by Monster Count; the D2RCore spawn hook (flag `0x40`) and `0x508f80`'s Terror Zone tail jump are D2RCore's. A new hook must chain, not replace |

### Stat functions (Q1)

The `FF 25` rows are D2RLoader thunks whose slots hold `D2RCore.dll` pointers in the dump. Call them; never hook them.

| RVA | Role | Unique masked prefix (offset 0) | Use |
|---|---|---|---|
| `0x2f7300` | NewStatList `(flags, expire, type, id)` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 89 4C 24 08 55` | call |
| `0x2f72f0` | SetListState `(list, state)`, writes list `+0x20` | `48 85 C9 74 ?? 89 51 20 C3 CC CC CC CC CC` | call |
| `0x2f7c00` | SetListStat `(dataFlag, list, stat, value, layer)`, thunk, slot `0x3e2a230` | `FF 25 ?? ?? ?? ?? 90 90 90 90 66 89 44 24 28` | call |
| `0x2f2cb0` | AttachStatList `(unit, list, 1)`, thunk, slot `0x3e2a270` | `FF 25 ?? ?? ?? ?? 90 90 90 90 90 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 A0 00 00 00` | call |
| `0x2f5df0` | List stat value, thunk, slot `0x3e2a208` | `FF 25 ?? ?? ?? ?? 90 90 90 90 57 48 83 EC 20 41 0F B7 F1 41 8B D8 48 8B FA 48 85 D2 75 ?? 33 C0` | call |
| `0x2f5020` | Unit stat `(unit, stat, layer)`, thunk, slot `0x3e2a218` (already in section 3) | `FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA` | call |
| `0x2f7d10` | SetUnitStat, thunk, slot `0x3e2a248` | `FF 25 ?? ?? ?? ?? 90 90 90 90 56 57 41 54` | call |
| `0x2f5b80` | Find a list `(unit, state, flags)` on the owner's chain (`+0x90`, or `+0x98` with flags bit 13, both on the unit's owner list `[unit+0x88]` and not on the unit (live, section 7; static: `0x2f5bb2 call 0x34b870`, `0x34b870 mov rax,[rcx+0x88]`, and the remove path `0x2f7920` -> `0x2f6e50` reads the same list through `[list+0x78]`); next `+0x68`; match `(list+0x1c & flags & 0xffffdfff) != 0`) | `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B F8 8B F2 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 8B CB E8 ?? ?? ?? ??` | call |
| `0x2f5940` | Unit state list lookup `(unit, state)` | `48 89 5C 24 10 57 48 83 EC 20 8B DA 48 8B F9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 8B CF` | call |
| `0x3354c0` | SetStateBit `(unit, state, on)` | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 20 41 8B E8 8B DA 48 8B F1` | call |
| `0x3355a0` | SetStateGfxFlag | `48 89 5C 24 10 55 56 41 56 48 83 EC 30 41 8B E8` | call |
| `0x2f7920` | Detach a list from a unit `(unit, list)` | `40 53 48 83 EC 20 48 8B DA E8 ?? ?? ?? ?? 0F B6 C8 48 8B D3` | call |
| `0x2f4180` | Free a list `(dataFlag, list)` | `48 85 D2 74 ?? 83 7A 1C 00 0F 8D ?? ?? ?? ??` | call |
| `0x3d5230` | Get or create an item's stat list | `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 45 8B F8` | call |
| `0x34b9d0` / `0x34a330` / `0x34a0e0` | Unit type / unit id `(unit, file, line)` / data flag `(unit)`: the argument sources for NewStatList | not signed (short helpers; `0x34b440` matches 10 sites at 54 bytes, `0x300a90` 4, `0x34a040` 4); call by address after a caller-side check | call |
| `0x2dc4b0` | Client stat text builder (D2Client `mods.cpp:0x968`): temporary list, merge with `0x2f6b00`, then the text thunk | `48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 C0 FB FF FF` | call |
| `0x2da760` | Stat text renderer, thunk, slot `0x3e2a168` | `FF 25 ?? ?? ?? ?? 90 41 57 48 8D AC 24 98 ED FF FF` | call |

### Q1. Applying stats to monsters, the player and the map item

**A. Monsters at spawn.**
- The spawn core `0x493cc0(&block)` (prefix `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 F0 48 81 EC 10 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 F0 48 8B 01`; first relative operand at `+0x24`, so hookable by the rule, but no hook is planned) does, in order (static): `0x490410(1, monstats id, ...)` creates the unit; `0x509520` counts it in the region; alignment is set (`0x48e600`); `0x4947d0(params, monster)` runs when flag `0x02` is clear; `0x494a60(params, monster)` runs; the D2RCore hook `0x143e2b454` runs when flag `0x40` is clear.
- **Hook point: `0x494a60(params, monster)`**, prefix `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 8B 69 18` (21 bytes, unique). Its 14-byte prefix matches 11 sites, so use the 21-byte one. It is 14 whole instructions with no relative operand, so it is hookable (static). It has one caller (`0x4943f2`, in the core). It is a per-class switch on the monstats row and does not set the level. Calling the original first and then attaching the list to `monster` is the least invasive point (inference: the unit already exists and is in its room).
- The monster level is set in `0x49dd70(game, monster)` (prefix `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 60 48 8B E9`): `level = max(1, 0x508f80(game, levelId, diff, exFlag))` written with SetUnitStat to stat `0xc`; stat `0x13` is set to `min(level + 0x32, 0x5a)`; special cases follow for classes `0x146` to `0x171` (static). It has no direct caller; a pointer to it sits in a table at data `0x23964b0` (next entry `0x4a1380` at `0x23964e8`, so 0x38-byte rows). When that callback runs relative to `0x494a60` is **needs game**: log both for one spawn.
- **Recipe (static; the engine's own idioms, not pd2's code):**
  1. `list = NewStatList(flags, expire, type, id)` with `type = 0x34b9d0(unit)` and `id = 0x34a330(unit, file, line)`.
  2. `SetListState(list, state)` for a state that has a stat-list meaning.
  3. `SetListStat(dataFlag, list, stat, value, layer)` for each stat, `dataFlag = 0x34a0e0(unit)`.
  4. `Attach(unit, list, 1)`.
  5. If the state needs its visible bit: `SetStateBit(unit, state, 1)` and optionally `SetStateGfxFlag`.

  Engine uses seen: `0x339d70` (NewStatList(2, expire, ...), SetListState `0x79`, a `0x2f7a50` callback, Attach, SetStateBit `0x79`, `0x2f7ab0`); `0x49bef0` (MonsterUnique.cpp:`0x11a7`: look up `0x2f5940(unit, 0xbd)`, NewStatList(0, 0, type, id), SetListState `0xbd`, Attach, SetListStat `0x16f`; no SetStateBit); `0x3d5230` (item list: NewStatList, Attach, SetListState). Attach does not set the state bit (also pd2 note #630: a second source, inference).
- Remove with `SetStateBit(unit, state, 0)`, `0x2f7920(unit, list)` then `0x2f4180(dataFlag, list)`. That the unit's death frees its lists is inference.
- **Needs game:** (1) a monster given `SetListStat` for a life percent stat shows the effect (percent stats on monsters; pd2 scaled maxlife with SetUnitStat directly); (2) the order against `0x49dd70` (does a later level write reset hp?); (3) the client sees the stat (replication of an attached list). Check: spawn one monster at the hook, read `0x2f5020(monster, stat, 0)` on the server, and watch its life bar on the client.

**B. The player while in a map.**
- Apply from the existing PlayerTick hook `0x42e350` (prefix `48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA`, 21 bytes, unique on the dump; only caller `0x42e74d`). Look up the list with `0x2f5940(player, state)` first, create and attach only when none exists, and remove it on leaving the map with the remove recipe above. Using a state the player does not otherwise have avoids a clash with game lists (inference).
- **Needs game:** whether the list survives a save and reload inside a map (check: apply, save and exit, reload, read the stat), and whether the stat reaches the client's character sheet.

**C. The map item's own stats (`ItemStats=1`).**
- Item stat lists, per the engine's client packet writer `0x41ab60` (static): it walks states 165 to 169 (table at `0x1d17878` holds `[165,166,167,168,169]`) with flags `0x2040`, plus the runeword list `0x2f5b80(item, 0xab, 0x40)`. The base magic list is `0x2f5b80(item, 0, 0x40)` (created at `Items.cpp:0x1900` by `NewStatList(0x40, 0, 4, id)` and attach). `0x3d5230` is the engine's get-or-create for an item list.
- Reading the map's mods from the item: walk the list from `0x2f5b80(item, 0, 0x40)` over its entries (layout above, inference), or ask `0x2f5020`/`0x2f5df0` for each stat id the plugin owns.
- The toolkit notes that the client copy of an edited item list stays stale until the item is moved (second-hand: inference). **Needs game** if the plugin changes an item after it is made; the plugin should write its own stats at item creation only.

**D. The text.**
- The stat text comes from the client builder `0x2dc4b0` (merges the unit's lists into a temporary list and calls the D2RCore renderer `0x2da760`; callers `0x2dc7d3` and `0x2d9342`). The renderer body is in `D2RCore.dll`, so its behaviour is not read here (**needs game**).
- Plugin-side alternative (SDK, not engine): format the lines in the plugin from ItemStatCost (data table id 35 through `DataTableService`) and strings from `localization.h` `GetStringByKey`; there is no stat-text service in the SDK. Show them through Q4's route.

### Region and level functions (Q2)

| RVA | Role | Unique masked prefix (offset 0) | Use |
|---|---|---|---|
| `0x5098b0` | Builds the region array for a game; no direct caller (so once per game is inference) | `48 89 5C 24 10 57 48 83 EC 30 48 8B F9 48 8D 4C 24 40` | call |
| `0x508750` | Region builder `(difficulty, array, rng, seed, game+0x104, flag)`: levels 1 up to the level count (`tables+0x1468`), `0x9f0` bytes each | `40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 88 F9 FF FF` | call |
| `0x508740` | Region lookup `array[levelId]` | `48 63 C2 48 8B 04 C1 C3 CC CC CC CC CC CC` | call |
| `0x509940` | Fills a region's groups from the levels row | `40 53 55 56 57 41 55 41 56 41 57 48 81 EC D0 00 00 00` | call |
| `0x508f80` | Monster level for `(game, level, diffIdx, exFlag)` | `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 30 48 63 FA 48 8B F1` | hookable |
| `0x32c700` | Levels row MonLvl: short at `+0x18 + diff*2`, or `+0x1e + diff*2` when the flag is set; returns 1 when out of range or diff >= 3 | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 20 57 41 56 41 57 48 83 EC 40 41 0F B6 E8` | call |
| `0x50acf0` | Room populator `(game, room)` | `4C 8B DC 53 56 48 81 EC B8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 80 00 00 00 83 3D ?? ?? ?? ?? 00` | call-only (RIP-relative at `+0xc`) |
| `0x419d20` | Pack kind for `(region, room)` | `48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 0F B6 B9 C8 02 00 00` | call |
| `0x419e80` | Pick group | `48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 48 8B F1 4D 8B F1` | call |
| `0x50b7e0` | Normal pack | `4C 8B DC 49 89 5B 08 49 89 6B 10 49 89 73 18 57 41 54 41 55 41 56 41 57 48 83 EC 60` | call |
| `0x50b4d0` | Champion pack | `40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC C8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 B0 00 00 00 48 8B 84 24 30 01 00 00` | hookable by the rule (first relative operand at `+0x14`); no hook planned |
| `0x50b1b0` | Unique or boss pack | `48 89 6C 24 18 57 41 56 41 57 48 83 EC 40 48 8B EA 48 8B F9` | call |

### Q2. Overriding monster density and level for the slot level only

- **Region fields (static):** `0x508750` writes, per level id `n` (1 up to the count): `+0x2b8` = levels row `+0x24 + diff*4` (MonDen), `+0x2bc` / `+0x2bd` = row `+0x30 + diff` / `+0x33 + diff` (MonUMin / MonUMax), `+0x2be` = row `+0x36`, `+0x2d8` = row `+0x38`, `+0x2dc` and `+0x2e0` = the MonLvl short (`+0x18`, or `+0x1e` when the flag is set), `+0x2c0` = level id, `+0x2d4` = -1, `+0x0c` = -1. Groups are filled by `0x509940` (count at `+0x10`, up to 13, stride `0x34`, from `+0x14`).
- **Density (static):** `0x50acf0` gates on global `DAT_14238d5b8`, a non-null region and the D2RCore check `0x143e2b44e(game, room)`; it requires `+0x2b8 != 0`, clamps density to 10000, counts 3x3-tile cells in the room rect and rolls `rand % 100000 <= +0x2b8` per cell. A hit calls `0x419e80` then `0x419d20(region, room)`, whose result picks the pack: 0 unique or boss (`0x499880`, `0x50b1b0`), 1 normal (`0x50b7e0`), 2 champion (`0x50b4d0`, `0x492e70`). **The 0 / 1 / 2 mapping is inference.** Anything that spawns increments region `+8`. In `0x419d20`: if `+0x2c8 < +0x2bc` and `+0xc != 0` it can return 0 with chance `(+4 * 100) / +0xc` percent; else if `+0x2c8 < +0x2bd` a roll below 6 returns 0; else `(rand % 100 > 35) + 1` (inference from the code).
- **Slot-only density, recommended: edit the slot's own region.** `region = 0x508740(*(game + 0x1d0), slotLevel)`; set `+0x2b8` (and `+0x2bc` / `+0x2bd`, the groups) before the first room of the slot activates. It touches only that level (static: one record per level). Whether the array is built once per game or again on a level change is inference. Do it from a point both room-activation paths reach (`0x48f190`, Q3), before calling the original; a `0x4042a0` hook alone misses rooms activated through `0x408670`. **Needs game:** set `+0x2b8` to 0 and to 10000 in a slot level and count monsters; read `+0x2b8` back after a second visit.
- **Level (static):** `0x508f80` is the one place the engine asks for a monster level. It passes `(game, level, r8 diffIdx, r9 exFlag)` through to `0x32c700` with r8 and r9 untouched, so treat it as a 4-argument function. It has 13 callers, including `0x49dd70` at `0x49dde3` (`r8` = byte 0 of `0x48ff40(...)`, `r9` = (byte 1 != 1)). If `0x489b70` (Terror Zone) is true it calls `0x489a60`, and a true result tail-jumps into D2RCore (`0x3e2ddc0`), so a hook must call the original for every level but the slot. **Hook `0x508f80`, return the plan's level when `level == slotLevel`, else call the original.** The other 12 callers were not all read, so what else follows the same number is inference.
- **`+0x2dc` / `+0x2e0` have no reader outside the builder** (a displacement scan of the code range found none; it agrees with pd2's note), so editing them alone changes nothing (inference). Set the level with the hook, not the field.
- **Needs game:** log `0x508f80`'s arguments and result on a slot level and a vanilla level; confirm a spawned monster's stat `0xc` equals the hook value.

### Event functions (Q3)

| RVA | Role | Unique masked prefix (offset 0) | Use |
|---|---|---|---|
| `0x492cf0` | Spawn request `(game, room, x, y, monstats id, mode, p7, u16 flags)`; fills the block for `0x493cc0` | `4C 8B DC 48 83 EC 68 8B 84 24 90 00 00 00 45 33 D2 4D 89 53 F0 45 89 53 D8 89 44 24 38 8B 84 24 98 00 00 00 89 44 24 3C 8B 84 24 A0 00 00 00 49 89 53 C0 89 44 24 4C` | call |
| `0x494a60` | After-create monster fixup (Q1) | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 8B 69 18` | hookable |
| `0x4926b0` | Preset spawn `(game, room, preset)` | `40 53 56 57 48 81 EC 80 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 70 48 8B DA 48 8B F1` | call-only |
| `0x490410` | Unit spawn `(unitType, row, x, y, game, room, u16, ...)`; for objects it runs the InitFn | `40 55 53 57 48 8D AC 24 70 FC FF FF 48 81 EC 90 04 00 00` | hookable |
| `0x50a050` | Missile-hit spawn `(game, room, id, x, y, mode, u16)`; calls `0x499880`, `0x50b1b0`, `0x499ba0` | `48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 44 89 4C 24 20 55 41 54 41 55 41 56 41 57 48 8B EC` | call |
| `0x499ba0` | Superunique `(game, room, x, y, SuperUniques row, u16)`; once-per-game bitmap at `game + 0x1d8` unless row `+0x29` or global `DAT_142aa6a74`; spawns via `0x49e980` and `0x492cf0` | `40 55 53 56 57 41 56 41 57 48 8D AC 24 48 FF FF FF` | call |
| `0x499880` | Unique or champion pack leader; calls `0x49e980` and the unique-mod applier `0x49ead0` | `48 89 54 24 10 55 41 54 41 55 41 57 48 8D 6C 24 E9` | call |
| `0x49ead0` | Unique-mod applier | `4C 89 44 24 18 48 89 4C 24 08 55 56 41 56` | call |
| `0x49e980` | Unique leader maker | `4C 8B DC 49 89 5B 10 45 89 4B 20 55 56 41 56` | call |
| `0x441300` | TC roll `(game, unit, ctx, u16 tcIndex, +6 stack args)`: monster level via `0x2f5020(unit, 0xc)`, compiled TC via `0x397290(dataFlag, tcIndex, level)`, roller `0x4404f0` | `40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 98 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 88 00 00 00 4C 8B BC 24 18 01 00 00` | hookable by the rule (first relative operand at `+0x14`); no hook planned |
| `0x4404f0` | TC roller | `4C 8B DC 55 53 49 8D AB 08 F7 FF FF 48 81 EC E8 09 00 00` | call |
| `0x397290` | Compiled TC lookup `(dataFlag, tcIndex, level)` | `48 89 5C 24 08 55 56 57 48 83 EC 30 41 8B D8` | call |
| `0x444680` | Drop one item `(game, unit, items row, quality, index, flags)` | `4C 8B DC 55 53 56 57 41 56 48 8D 6C 24 A0 48 81 EC 60 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 30` | hookable by the rule (first relative operand at `+0x15`); no hook planned |
| `0x43cd50` | Inner item maker of the drop | `40 55 53 56 41 54 41 57 48 8D 6C 24 C9 48 81 EC 90 00 00 00` | call |
| `0x447b80` | Monster TC drop | `48 8B C4 44 89 40 18 53 55 56 57 41 54 41 56` | call |
| `0xa121f0` | TC name to index (name map at `tables+0xd70`, via `0x300a90`) | `40 55 56 57 41 54 48 8B EC 48 83 EC 78 48 8B 05 ?? ?? ?? ??` | call-only |
| `0x592c40` | Object operate dispatcher `(game, ?, object)`: `OperateFn` = byte at objects record `+0x15e` (record via `0x34ad90`), `< 0x4b`; slot table at `0x23a51c0` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 48 8B F1 49 8B D8` | hookable |
| `0x58e2f0` | Slot 4: chest operate | `48 89 4C 24 08 55 48 8B EC 48 81 EC 80 00 00 00` | call |
| `0x4042a0` | Room first-activation `(game, room)` | `48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ??` | hookable (first 16 bytes are 5 whole instructions, no relative operand; misses `0x408670`) |
| `0x408670` | Per-game activation loop (one caller, `0x40678e`); runs the `0x4042a0` steps inline, calls `0x50acf0` at `0x40881c`, never calls `0x4042a0` | `40 55 57 41 55 48 8B EC 48 83 EC 70 48 8B F9` | call |
| `0x48f190` | Preset loop `(game, room)`, called on both activation paths (`0x4042c7`, `0x4087fb`) | `40 55 56 41 56 48 8B EC 48 81 EC 80 00 00 00 4C 8B F2 48 8B F1` | hookable (the first 15 bytes are 5 whole instructions, first relative operand at `+0x18`) |
| `0x38dfb0` | Room containing `(room, x, y)` | `48 89 5C 24 10 55 56 57 48 83 EC 20 41 8B F0 8B EA 48 8B F9` | call |
| `0x2efc10` | Room level id `(room)` | `48 85 C9 75 ?? 33 C0 C3 48 8B 49 18 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 85 C9` (35 bytes, unique; the first 18 bytes match 4 wrappers, see section 3) | call (a frameless wrapper, relative operand at `+0x3`; call it by address or with the 35-byte prefix) |
| `0x44db20` | Death `(game, victim, killer, flag)`; Monster Count hooks it, the SDK also has a MonsterDeath listener | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50 48 8B E9` | hookable |

### Q3. Entry points per event kind

- **Spawn a monster:** `0x492cf0(game, room, x, y, monstats id, mode, p7, flags)`; the unit is made through `0x493cc0`. Flags `0x42` skip `0x4947d0` and the D2RCore spawn hook, and p7 is a free-spot search radius (`-1` disables it; the superunique code passes `0xa`; see section 9). **Needs game:** one spawn on a floor tile, then read back the unit.
- **Superunique and boss:** `0x499ba0`. The once-per-game bitmap (`game + 0x1d8`) limits it: a second spawn of the same row in a game does nothing unless row `+0x29` or the global is set. Direct `call 0x499ba0` sites (instruction addresses): `0x40206d`, `0x40d64a`, `0x40dcee` and `0x50a8c5` (the earlier list named `0x50a050`; `0x50a8c5` is probably inside that function, inference). The "cheat" label on the first three is from the earlier read and was not re-checked. **Needs game:** spawn a superunique in two slot visits.
- **Unique or champion leader:** `0x499880` (+ `0x49ead0` mods), reached from the populator's `0x50b1b0` and `0x50b4d0` paths.
- **TC drop:** `0x441300` for a monster unit takes the unit's level from stat `0xc`, looks up the compiled TC with `0x397290` (index 0 = none) and rolls with `0x4404f0`. Direct `call 0x441300` sites: `0x40bb3e`, `0x415c21` and `0x447e17` (the earlier list gave `0x40b730` and `0x415310`, the starts of the functions that contain the first two, as the TestTreasure cheat and a collect-mode caller; that labelling was not re-checked). A direct item drop is `0x444680`. The TC-name-to-index route (`0xa121f0`) is pd2-sourced: inference. **Needs game:** call `0x441300` with a known `tcIndex` at a monster, list the items made, and confirm the six stack args (the cheat callers show the shape).
- **Objects:** `0x490410(2, objects row, x, y, game, room, ...)` makes an object and runs its InitFn. Operating one goes through `0x592c40` and the `OperateFn` slot table at `0x23a51c0`: slots 11, 13, 17, 35 to 38, 60 and 62 to 64 are null, slot 4 is the chest handler `0x58e2f0`, slot 75 points to data (static). A global plugin cannot add object rows, so use vanilla rows and recognise a slot object by unit id in a `0x592c40` hook. **Needs game:** operate a vanilla chest placed by the plugin and observe the hook.
- **Room activation has two paths (static).** (a) `0x4042a0(game, room)` runs when `0x2f0600(room) == 0`, in order `0x48f190` (preset loop, called at `0x4042c7`), `0x503790`, `0x510f00`, `0x50acf0` (populator, at `0x4042e8`), `0x2f04e0(room, 1)`, then `0x2f0440(room, 1)`; for an already populated room only `0x503790` runs. Its direct callers are `0x410332`, `0x435e7a`, `0x49016c` and `0x59ec0f`. (b) `0x408670`, the per-game loop (one direct caller, `0x40678e`), does not call `0x4042a0`: it runs the same steps inline (`0x2f0600` test at `0x4087ec`, then `0x48f190` at `0x4087fb`, `0x503790`, `0x510f00`, `0x50acf0` at `0x40881c`, `0x2f04e0`, `0x2f0440`). A hook on `0x4042a0` alone misses every room activated through `0x408670`.
- **Common hook point: `0x48f190(game, room)`.** Its only two direct callers are `0x4042c7` and `0x4087fb`, one per path, and it runs before the populator (`0x50acf0`, also only two callers: `0x4042e8`, `0x40881c`) in both. Prefix `40 55 56 41 56 48 8B EC 48 81 EC 80 00 00 00 4C 8B F2 48 8B F1` (21 bytes, unique on the dump; the first 15 bytes match 3 sites). The first 15 bytes are 5 whole instructions with no relative operand (the first relative operand is the `jne` at `+0x18`), so it is hookable by the rule. Run the Q2 region edit before the original (read the level id with `0x2efc10(room)`, called by address). `0x48f190` runs before the populator, so it cannot add events to the new room; do that after the room is populated, either by also hooking both `0x4042a0` and `0x408670` and acting after the original, or from a later step. The alternative for everything is to hook both `0x4042a0` and `0x408670`. This is static reading; no hook has been tried.
- **Needs game:** log, on entering a slot level, which of `0x4042a0` and `0x408670` (through `0x40678e`) activates its rooms, the call order, and the level id; check that the Q2 region edit made at `0x48f190` takes effect on the first room.
- **Placement:** `0x38dfb0(room, x, y)` gives the room for a point; the server room's subtile rect is at `+0x80/+0x84/+0x88/+0x8c` (read by `0x2f0680`, prefix not signed here). Units use subtiles (5 per tile, section 4 S1). Server room `+0x18` is the DRLG room (section 5).

### Q4. Server to client text for entry messages

- **Chat packet (static):** `0x4016c0` (GameChat, prefix `48 81 EC 88 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 70 01 00 00 33 C0`; call-only, relative operand at `+0x7`) copies a `0x188`-byte packet struct and calls `0x477a30(*(game + 0x138), packet)` (SendChat, prefix `48 89 5C 24 18 55 56 57 41 56 41 57 48 81 EC 90 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 80 01 00 00 48 C7 C7 FF FF FF FF`; first relative operand at `+0x13`, hookable by the rule, no hook planned). `0x477a30` writes header byte `0x26`, `+1` type, `+2`, `+3` unit type, `+4` unit id (int), `+8` colour, `+9`, the name at `+10` (clamped to 60 bytes) and the text at `+0x47` (clamped to 255 bytes), then broadcasts through `0x4817f0`.
- **Client display rule is not re-tested here:** pd2's note says type 4 or unit type 0 shows, and type 1 with unit type 2 is dropped (inference, second-hand). **Needs game:** send one packet per (type, unit type) pair and note which lines show and where.
- `0x491960(player, u16 msgId, arg)` (prefix `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 49 8B F8 0F B7 F2`) writes unit `+0xb2` / `+0xb8` and queues an update. It sends fixed client message ids (the Cartographer portal uses `0x15`) and cannot carry custom text (static).
- **SDK routes (not engine):** `NetworkService` `SendToClient` (`network.h`) for a custom message to a client plugin, overlay `drawText`, and `WriteConsoleMessage`. A route that avoids the engine is the safer default; `0x477a30` is the fallback if the client has no plugin. **Needs game:** which the player sees in a normal game and in a shared game.

### Left for the game

1. Client replication of an attached list, percent stats on monsters, and the save behaviour (Q1).
2. `0x49dd70` timing against `0x494a60`, and whether a later level write resets hp (Q1A).
3. The region edit effect, the pack-kind mapping of `0x419d20`, and whether the region array is built once per game (Q2).
4. Every spawn, superunique, TC and object call exercised once, including the six TC stack args, and which room-activation path runs for the slot level (the per-row checks in Q3).
5. The chat packet display rule and the best text route (Q4).
