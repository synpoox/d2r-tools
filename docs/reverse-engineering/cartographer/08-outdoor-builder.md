## 8. Outdoor builder engine checks (issue #83)

Static answers for the twelve open questions of plan section 7 ("Still to check, in Ghidra or in game") of
[cartographer-outdoor-builder.md](../../cartographer/plans/cartographer-outdoor-builder.md). Everything is read from the build 93847 dump
(Capstone disassembly, plus the d2rAI-toolkit `d2r-data/93847` tables for Q12); nothing was run in a game and no code changed. Where a question
cannot be settled from the dump, the exact in-game check is given as "needs game". Each answer carries an evidence level: **static** (read
from bytes or tables), **inference** (follows from static facts but was not observed), or **needs game**. Nothing here is live evidence; the live
evidence for the outdoor builder stays in section 4.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer outdoor builder (plan sections 6 and 7; phase S2 and builder phase 5); no hook exists yet beyond the section 4 spike |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the same memory-layout dump as sections 1a and 5 (`D2RLoader_6ab3782c_dump.exe`) |
| Location | RVAs in the table below. Level record offsets as in section 5; outdoor grid block (level `+0x18`, `0x340` bytes): grids at `+0x08` (piece id), `+0x30`, `+0x58` (flags), `+0x80`, each a `{w, h, rows*, cells*}` record whose cells are `u64`; `+0xb0/+0xb4` grid size in 8-tile cells; `+0xb8` and `+0xc0..+0xe8` outline lists; `+0x338` neighbour list |
| Guard | Existing signed rows (`0x3271c0`, `0x3ee320`, `0x3fe2e0`, `0x3ecfc0`, `0x3ee780`, `0x3fa7f0`, `0x3fa790`, `0x3fac10`, section 4) keep their signatures; no second signature is recorded for them. The rows in the table below are unsigned, read-only evidence. The prefix of each was cut as the shortest unique whole-instruction prefix of at least 24 bytes, extended where a twin needed it (relative calls, jumps and RIP-relative operands masked) and checked with `pat` to match exactly once, at its own RVA, across the executable sections of the dump (`.text` in each case); none has been checked in a running game. A hook that needs one of them records its signature and max patch length first |
| Rule | The 14-byte rule of section 6: a 14-byte patch at the entry, rounded up to a whole instruction, must contain no relative operand (RIP-relative memory, relative call or jump). "hookable" passes, "call-only" does not. No hook is proposed on this page |
| Evidence | Disassembly of the functions named. Reproduce with `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]` and `... pat "<hex with ??>"` |
| Validation | Static only. No game, no hook. Not checked on Steam build 94995 |
| Failure mode | Fail open, as in sections 1a and 5 |
| Coexistence | No new patch is proposed on this page |

Addresses found while answering (all `D2R.exe`, build 93847; "hookable" and "call-only" are by the 14-byte rule of section 6; the prefix is read from the entry, relative operands masked as `??`):

| RVA | Role found | Question | Label | Unique masked prefix (offset 0) |
|---|---|---|---|---|
| `0x3faa50` | Grid op `(grid, x, y, value, op)`; op 0 OR, 1 AND, 2 XOR, 3 SET, 4 set if zero, 5 AND-NOT | Q3 | call-only | `40 53 48 8B 41 20 48 8D 1D ?? ?? ?? ?? 4C 63 5C 24 30 4D 63 C0 4E 63 14 80` |
| `0x3ede00` | Void cell setter `(level, x, y)`: SETs piece grid `+0x08` to 0 and flag grid `+0x58` to `0x100` | Q3 | hookable; signed (see "Signed for the outdoor slot" below) | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 41 B9 00 01 00 00` |
| `0x3edda0` | Clear cell `(level, x, y)`: SETs both grids to 0 (same shape as `0x3ede00`; only the last argument differs: `xor r9d, r9d` against `mov r9d, 0x100`) | Q3 | hookable | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 45 33 C9` |
| `0x3efaf0` | Outdoor outline and border pass `(dataFlag, level)`; its chunk holds the loop at `0x3f00a0` that ORs `0x100` into edge cells | Q3 | hookable | `48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9 48 81 EC E0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 17 4C 8B EA` |
| `0x3f95e0` | Per-room grid pass `(dataFlag, arg)`: `arg` holds the level at `+0x90`; allocates three grids at `[arg+0x40]` with `0x3fa7f0` and loops grid `+0x50` of that block, ORing (op 0) a value chosen by a switch on `level+0x1e0 - 0x10` (jump table `0x3f987c`; `0x100` only for 16, Act 2 desert) into cells whose `(flags & 0x3f0ff80) == 0`; `.pdata` chunk `0x3f95e0..0x3f98bc`. Not the outdoor flag grid | Q3 | hookable | `88 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC C8 00 00 00` |
| `0x327c40` | Level teardown `(dataFlag, level, keep)`; dispatches on level `+0x00` to `0x3e69d0` (maze), `0x3dd370` (preset) or `0x3ee220` (outdoor); frees `+0x1c0`, `+0x1d8`, `+0x1f0` | Q4 | hookable | `48 89 5C 24 08 48 89 6C 24 18 56 57 41 56 48 83 EC 20 41 8B E8 48 89 54 24 48` |
| `0x3ee220` | Outdoor free `(level, keep)`: frees the four grids, the outline lists, the neighbour list and, when `keep` is 0, the block at level `+0x18` | Q4 | hookable | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B 59 18` |
| `0x3f9a90` | Outline list free `(list*)`: walks a circular list through `+0x10`, then frees the head; skips a NULL head | Q4 | hookable | `48 89 5C 24 10 56 48 83 EC 20 48 8B 19 48 8B F1 48 85 DB 74 ?? 48 8B 5B 10` |
| `0x3fa730` | Grid free `(grid)`: frees `+0x20` when set, then zeroes `+0x18` and `+0x20` | Q4 | hookable | `40 53 48 83 EC 20 48 83 79 20 00 48 8B D9 74 ?? 48 89 7C 24 30 E8 ?? ?? ?? ??` |
| `0x360730` | Neighbour list free `(head)`: NULL tolerant, walks `+0x20` | Q4 | call-only | `48 85 C9 74 ?? 53 48 83 EC 20 48 8B D9 48 89 7C 24 30 48 8B FB 48 8B 5B 20` |
| `0x3daad0` | Vis exit lookup `(dataFlag, level, exit index)`: the Vis entry from the drlg list at `drlg+0x118`, else the levels.txt row at `+0x68 + n*4`; no RNG | Q5 | hookable | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 20 57 48 83 EC 20 48 8B 9A C8 01 00 00` |
| `0x334ed0` | Tile to subtile: multiplies two ints by 5 in place (17 bytes, no relative operand; the first 14 bytes end on an instruction boundary; the prefix runs into the padding after it) | Q5, Q6 | hookable | `8B 01 44 8D 04 80 44 89 01 8B 02 8D 0C 80 89 0A C3 CC CC CC CC CC CC CC` |
| `0x402820` | Debug console command `WarpToPreset` handler; stores its arguments in the globals at `0x2a9a5c0`, `0x2a9a5c4`, `0x2a9a5cc` and the byte `0x2a9a5c9` | Q9 | call-only | `40 53 48 83 EC 20 48 C7 05 ?? ?? ?? ?? 00 00 00 00 48 8B D9 C6 05 ?? ?? ?? ?? 00` |
| `0x3feb80` | Act 5 pass for level id `0x6f` only: up to 90 random even-cell tries through `0x3ff420`, then a second pass with the same restamp | Q8 | hookable | `48 8B C4 53 41 55 48 83 EC 68 83 BA F8 01 00 00 6F 48 8B DA 44 0F B6 E9` |
| `0x3ff420` | Barricade restamp at one cell `(dataFlag, level, x, y)`: a piece id in `0x393..0x39a` is stamped again as `+0x10` with file `-1` | Q8 | hookable | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 40 41 8B F8 48 8B F2 0F B6 E9` |
| `0x3feee0` | Act 5 entrance and exit stamps (pieces `0x38b..0x38e`, 907..910) at cells whose flag has bit `0x400` | Q8 | hookable | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 40 48 8B 5A 18` |
| `0x3ff0e0` | Act 5 helper that reads the outline list at block `+0xb8` (tests level id `0x75`) | Q8 | hookable | `88 4C 24 08 53 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 78 48 8B 4A 18` |
| `0x3fe9e0` | Act 5 cave entrance table walk: 20-byte records at `0x1d12bf8`; LvlPrest ids per side | Q8 | hookable | `48 89 5C 24 10 48 89 6C 24 18 56 41 56 41 57 48 83 EC 40 4C 8B 72 18 48 8D 1D ?? ?? ?? ??` |
| `0x3edfb0` | Piece id at a cell `(level, x, y)` (reads grid `+0x08`) | Q8 | hookable | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 71 18 41 8B D8 8B FA` |
| `0x3e75bc` | Maze path chunk, not an entry (`.pdata` `0x3e75bc..0x3e775c`, in the middle of a function) holding the level id test against `[0x2a9a5cc]` at `0x3e75f6` | Q9 | chunk (no hook point) | `8B 83 F8 01 00 00 83 F8 1C 74 ?? 83 F8 6B 74 ?? 48 8B CB E8 ?? ?? ?? ??` |
| `0x3277e0` | Reads `[drlg + 0x110]`; its result picks which of the two `WarpToPreset` preset ids is used | Q9 | call-only | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 83 10 01 00 00 83 E0 01` |
| `0x32c360` | LvlPrest row lookup `(dataFlag, id)` | Q9 | hookable | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30 48 63 F2 E8 ?? ?? ?? ?? 48 8B F8 48 8B DE 85 F6 78 ?? 48 3B 98 C0 14 00 00` |

### Q1. Entry bytes and patch state of `0x3fe2e0`, `0x3ee320`, `0x3ecfc0`, `0x3fa7f0`

- **Static:** the dump holds, at each RVA, exactly the bytes of the section 4 S0 signature (the full signatures from that table were each run
  through `pat`: one match, at the RVA shown). The first 16 bytes are `40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9` (`0x3fe2e0`),
  `48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48` (`0x3ee320`), `48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 55` (`0x3ecfc0`) and
  `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57` (`0x3fa7f0`). None starts with a jump (`E9`, `FF 25`) or a breakpoint, so none of the four
  is patched in the dump. The same check on the other four S0 rows (`0x3271c0`, `0x3ee780`, `0x3fa790`, `0x3fac10`) gives the same result.
- **14-byte rule (static):** all four are hookable; the first relative operand is at `+37` (`0x3fe2e0`), `+90` (`0x3ee320`), `+61` (`0x3ecfc0`)
  and `+46` (`0x3fa7f0`). The spike's 16-byte and 19-byte hooks on the first two are consistent with this.
- **What this does not show:** the dump is one process image. It says nothing about another loader, plugin or anti-virus patching the same
  entries on a player's machine; the plugin's signature check at load (section 4, all-or-nothing, fail open) is what covers that.
- **Evidence level:** static. **Needs game:** nothing new; the live signature matches and hook diagnostics are in section 4 S0.

### Q2. What `0x3e2b5c8` returns for the slot id

- **Already in section 4 (S1):** for id 138 the act lookup returned 4 (live), which is why `0x3ee320` takes the Act 5 branch; section 1 has the row.
- **Static addition:** `0x3e2b5c8` is `FF 25` through the pointer slot at `0x3e2a4b8`, which in the dump holds an address outside the image (the
  D2RCore function). Its body is not in the dump, so the mapping from level id to act cannot be read here. The callers store the result in a
  byte (`0x3efaf0` at `0x3efb26` passes the level `+0x1f8`; `0x3ee320` makes the same call near its start), so the value is an act index 0..4.
- **Inference:** the lookup returns the Act column of the levels.txt row minus one for rows it knows (4 for the Act 5 clone in section 4). Not seen for any other id.
- **Evidence level:** live for id 138 (section 4); static for the shape. **Needs game (only if the slot id changes):** log `0x3e2b5c8(dataFlag, id)` for
  one row of each Act and for the chosen slot id with the section 4 probe; expect Act - 1 and, for the slot row, 4.

### Q3. Flag bit `0x100` in grid `+0x58`, and what sets it

- **Cell flags seen (static), grid `+0x58`:** `0x200` covered (set with `bts 9` in `0x3ecfc0` next to the 4-bit file index masks `0xf0000`), `0x400` marks an
  Act 5 border cell (tested with `bt rax, 0xa` in `0x3feee0`, which stamps the entrance and exit pieces 907..910 there), and `0x100` is the
  "void" mark below. The mask `0x3f0ff80` (bits 7..15 and 20..25) is the test `0x3f95e0` applies to its own per-room grid; it is not shown to be a decided-cell test on the outdoor flag grid.
- **What `0x100` is (static):** a cell that is deliberately empty. `0x3ede00(level, x, y)` SETs the piece id (grid `+0x08`) to 0 and the flag
  (grid `+0x58`) to `0x100`; its twin `0x3edda0` SETs both to 0 (fully cleared). The border pass `0x3efaf0` ORs `0x100` along the edge cells
  whose bit 0 is clear (loop at `0x3f00a0`, op 0 of `0x3faa50`). `0x3f95e0` is not a setter of `0x100` for this grid: its argument holds the level at `+0x90`,
  it allocates its own three grids at `[arg+0x40]` (`0x3fa7f0`, `0x3f965f`..`0x3f9687`) and loops grid `+0x50` of that block (a per-room grid, not level `+0x18` -> `+0x58`),
  with op `r13d` = 0 (OR). The value depends on a switch on `level+0x1e0 - 0x10` (jump table `0x3f987c`): `0x100` only for 16 (Act 2 desert), `0x120000` for 21 (jungle),
  `0x100000` for 22, `0xa00000` for 27, `0x1600000` for 28, `0x600000` for 31 when the level id is `0x75`, else 0.
  `0x3ede00` and `0x3edda0` are only reached through function pointers: their addresses sit in three tables in the data (`0x1eb2cb1`, `0x34dd3a1`, `0x3c3d310` hold
  `0x3ede00`; `0x1eb2ca5`, `0x34dd395`, `0x3c3d304` hold `0x3edda0`, as 8-byte absolute pointers), and no direct call exists.
- **Consequence for the slot:** a cell that is stamped by `0x3ecfc0` carries a piece id and the file bits, never `0x100`; the section 4 dump shows
  every cell of the 20x4 line covered, so no `0x100` is expected there. A plan that leaves cells unstamped should stamp them or
  SET `0x100` itself; what the rest of the engine does with an unstamped cell is not shown here (see the evidence level).
- **Evidence level:** static for who writes it (`0x3ede00` and `0x3efaf0` are the only writers found, by a scan of the 37 direct call sites of `0x3faa50`; a value passed in a register was not traced, so another writer through a computed value is not excluded; `0x3f95e0` is described above, not a setter here); **inference** for what the rest of the engine does with it (no reader of the bit was found in the
  stamper or the room maker `0x3ee780`; the bit is read only as part of the `0x3f0ff80` test). **Needs game:** log the `+0x58` grid after the build of the slot level and
  after a `0x3ede00` call from the hook; observe the `0x100` cell is not turned into a room by `0x3ee780` (room count unchanged by the mark).

### Q4. Whether the outdoor-type free handles a NULL or unusual `+0xb8` / `+0x1d8`

- **Level teardown `0x327c40` (static):** `+0x1c0` is freed only when non-null (`0x327d56..0x327d67`), `+0x1d8` only when `keep == 0` and non-null
  (`0x327deb..0x327e17`), the `+0x1f0` list until its next pointer is NULL (`0x327e1e..0x327e53`). A NULL `+0x1d8` is therefore safe; a non-null
  garbage `+0x1d8` is freed through the allocator vtable and would fault.
- **Outdoor free `0x3ee220` (static):** reads the block at level `+0x18` with no NULL check (`mov eax, [rbx]` at `0x3ee244`), so a type 3 level
  needs a real block until it is torn down. It clears bits `0x20` and `0x40` of block `+0x00`, frees the four grids with `0x3fa730`
  (NULL tolerant: it tests `+0x20` first), frees block `+0xb8` and the six lists `+0xc0..+0xe8` with `0x3f9a90`, zeroes `+0xf0..+0x32f`, and, only when `keep` is 0 (`test r14d, r14d; jne` at `0x3ee2c3`), calls `0x360730` on `+0x338`
  (NULL tolerant) and then frees the block itself.
- **Outline lists `0x3f9a90` (static):** takes the address of the list head. A NULL head is skipped. A non-null head must point to a circular list
  whose nodes link through `+0x10` and whose end is the head itself (the loop stops when the next pointer equals the head); anything else walks
  unrelated memory. So a NULL `+0xb8` is safe, a stale or foreign one is not.
- **Evidence level:** static. **Needs game:** build the slot level, leave and come back (the section 4 leave-and-return already exercises the
  free with a real block); for a hook that builds the block itself, run once with `+0xb8` left zero and watch for a crash_guard entry on the next teardown.

### Q5. Whether the tail of `0x3271c0` reads the level RNG or `+0x00`

- **Static:** after the dispatch the tail (`0x32722f..0x327361`) does not read `+0x1e4/+0x1e8` and does not read `+0x00` again. The reseed
  (`lea rcx, [r14+0x1e4]` then `call 0x367110`) is before the dispatch, and `+0x00` is read once for the dispatch. The tail reads: room count `+0x08`, the
  saved room bits `+0x258` (ORs `1` into room `+0xa0` for each saved bit, rooms in list order), the room list `+0x10/+0x48`, each room's `+0x50`
  flags, and for rooms with a warp bit (`0xff0`) or a `0x30000` bit it calls `0x3daad0` (no RNG; Vis lookup) and records the room centre.
  The centre is `+0x60 + (+0x68)/2` and `+0x64 + (+0x6c)/2`, converted by `0x334ed0` (times 5) into the arrays at level `+0x208` (x) and `+0x22c` (y), counted in `+0x250` (9 slots each).
- **See also section 4:** the spike already ran the native tail live with level type 0, which is stronger than this static read.
- **Consequence:** a hook that replaces the dispatch (as the spike does) can run the original tail afterwards without touching the RNG. The
  tail needs the rooms to be in the list and the room flags to be set by the builder; the type word `+0x00` may be anything at that point.
  The one place that does depend on RNG is the level-entry finder (`0x3da650`, last fallback, section 5 Q3).
- **Evidence level:** static. **Needs game:** none required; to confirm, log `+0x1e4` before and after the original tail in the hook (expect equal) and the
  `+0x250` warp count (expect the number of exit-flagged rooms).

### Q6. How a portal picks its target coordinates

- **Creation (static):** `OpenRedPortal` `0x5da800` finds a free spot near the player (`0x517f90`), spawns object 60 there (`0x432ce0`), then calls
  `0x34e9d0(portal, level)` with the `level` argument. Only the destination **level id** is set here; no coordinates are chosen at creation.
  What `0x34e9d0` stores inside the object is not read (the first lines test the pointer; the body was not followed).
- **Use (static for `0x2ef8b0`; inference for everything before it):** only this is static: `0x2ef8b0` calls `0x3da650` (`0x2ef8de`), `0x334ed0` and `0x362f90` with collision mask `0x1c09`. That red-portal use reaches `0x2ef8b0` (who calls it was not read), and that `0x34e9d0` leaves nothing the arrival reads, are **inference**. `0x2ef8b0` gets the target
  level, converts the returned room centre to subtiles (`0x334ed0`, then adds 3 to centre in the tile) and asks `0x362f90` for a free spot with collision mask `0x1c09`. The finder is
  described in section 5 Q3 (first room with `(room+0x50 & 0x30000)`, then warp-flag rooms against the Vis entries, then the room at the declared
  centre, then a seeded random room); in the code read here it takes the shortcut straight to the first helper (`0x3dad90`) when the exit index is `0xd` and
  the levels.txt row field at `+0x90` is non-zero, and to `0x3dac20` for other exit indexes when `+0x90` is non-zero.
  `0x3dad90` returns the first room that has a unit in its list whose object row has bit `0x40` at `+0x127` (the meaning of that row bit is not
  determined here) and writes its position into the out parameters.
- **Consequence (inference, from the unverified portal-to-`0x2ef8b0` link and the unread `0x34e9d0`):** for the slot, the arrival coordinates would come from room data (flags `0x30000` or exit bits) or from the declared centre and not from anything the portal object holds. An outdoor slot level with no exit-flagged rooms arrives at the declared-centre room
  (`+0x24 + w/2 - 2`, `+0x28 + h/2 - 2`), then at a random room.
- **Evidence level:** static for `0x2ef8b0` calling `0x3da650`, `0x334ed0` and `0x362f90`; **inference** for the whole chain from portal use to `0x3da650`, including the exit index. **Needs game:** confirm the chain by logging the callers and arguments of `0x2ef8b0` and `0x3da650` (exit index, out x/y,
  returned room) and the final position after entering the slot level by the red portal; observe which branch answered and that the character stands on a walkable cell.

### Q7. What the mask means in `Riv*.ds1` (DS1 walk flags)

- **Already in section 4:** the DS1 walk flags were not used as evidence and the cause of the impassable channel is "not known". Nothing in the
  dump settles it: the DS1 files are data, and the collision bits are built by the room loader from the tile files.
- **Static facts for the check:** the collision test used in section 4 is `0x350a20(unit, x, y, mask)` with mask `0x1c09`, which is the bits `0x1`, `0x8`,
  `0x400`, `0x800` and `0x1000`. Which of those the water cells carry is not known. The d2rAI-toolkit lists the teleport mask as `0x804`.
  The lvlprest rows of the line (530..532) point to `Act3/Jungle/RivW.ds1`, `RivE.ds1`, `RivEW.ds1` and the `...2/...3` variants (Q12).
- **Evidence level:** needs game. **Exact check:** in one room of the stamped line that contains the water (the section 4 route-guided run
  names x 70..90), use the spike's `coll <mask> dump` once per single bit (`0x1`, `0x8`, `0x400`, `0x800`, `0x1000`) and once for `0x1c09`, over the same 800 x 160 subtile window; the bits that
  light up exactly on the water subtiles are the water bits. Then parse the same piece's DS1 and DT1 offline (the files are in the toolkit's `d2r-data`) and compare which tile blocks sit under those subtiles.

### Q8. How Barricade 915..930 are picked in `0x3fe2e0` / `0x3feb80`

- **Not picked in `0x3fe2e0` or `0x3feb80` (static):** no instruction in `0x3e0000..0x400400` carries a piece id in 915..930 as an immediate. The only uses of
  the range are two tests, `lea r, [rax - 0x393] ; cmp r, 7 ; ja` at `0x3fee20` (inside the `0x3feb80` chunk `0x3fedac..0x3feed5`) and `0x3ff44d` (in `0x3ff420`), both on the piece
  id returned by `0x3edfb0` for a cell. For ids 915..922 (`0x393..0x39a`) they stamp the cell again with id + `0x10` (931..938, "Barricade Prison 1..8") and file `-1` through `0x3ecfc0`.
- **`0x3feb80` (static):** returns at once unless the level id is `0x6f` (111). It makes up to 90 tries (`cmp esi, 0x5a`) at random even cells (the level RNG at
  `+0x1e4`, multiplier `0x6ac690c5`, `2 * rand(size/2)` on each axis) through `0x3ff420`, counting the cells that were barricades, stopping the first pass at 3, and then runs a second pass of
  the same restamp from a random start; so the choice is "which three or more barricades become prison variants", not which barricade pieces exist.
- **Other Act 5 stamps (static):** `0x3feee0` stamps the entrance and exit pieces 907..910 at cells with flag bit `0x400` (level id `0x6e` flips
  the file sign); `0x3fe9e0` walks three 20-byte records at `0x1d12bf8` -- (level 112, 0, 0, 913, 914), (117, 0, 1, 983, 984) and (117, 0, 0, 985, 986) -- stamping the cave entrance rows; `0x3fe2e0` stamps
  `0x361 + i` (865..879) for i in 0..14 in a loop at `0x3fe382`.
- **Not found:** where the plain barricade pieces 915..930 enter the grid. They are not an immediate in the DRLG range, and the 915 and 987
  values found in the data at `0x1d12d2c` and `0x1d12d40` are in an unrelated table. Inference: they come from the generic path under `0x3fe3b7` (`0x3efa20`, `0x3ff0e0`) or from D2RCore.
- **Evidence level:** static for the restamp and the Frigid Highlands gate (the immediate scan covered `0x3e0000..0x400400` only; other DRLG code such as `0x3da650` and `0x3dd370` lies below it); **needs game** for the origin of 915..930. **Exact check:** count the `0x3ecfc0` calls while building level 111 (and
  112, 117) with the spike's counter hook, logging `(lvlprest id, file, cx, cy)` per call; the ids in 915..930 and the call that placed each name the stamper. A wanted layout does not need this: the slot stamps its own pieces.

### Q9. The special level id `DAT_142a9a5cc` in `0x3ee320`

- **Static:** it is the target of the debug console command `WarpToPreset`. The command table entry at `0x400dc0` (next to `SetObjectMode`) names it; its handler
  `0x402820` parses the arguments into the globals at `0x2a9a5c0` and `0x2a9a5c4` (preset ids), `0x2a9a5cc` (level id) and sets the byte `0x2a9a5c9` (enable). It stores its first argument in both `0x2a9a5c0` and `0x2a9a5c4` (the handler writes the same value to both preset globals), and its second in `0x2372488` (`0x40286d`), outside that range. In `0x3ee320` (at `0x3ee4e7`, after the Act 1 builder
  call) and in the maze path (`0x3e75f6`), when the level being built has `+0x1f8` equal to `[0x2a9a5cc]` (`0x3ee4e7`; `0x3ee320` does not test the enable byte `0x2a9a5c9` there, but the maze path tests it at `0x3e7602`) and the chosen preset id is non-zero (`test ebx, ebx` at `0x3ee516`), the code asks `0x3277e0` for a flag, picks one of the two preset ids,
  looks the LvlPrest row up (`0x32c360`) and forces that preset into the grid, then writes the used id back. In the dump all of `0x2a9a5c0..0x2a9a5d0` are zero, and 0 is no valid level id, so the branch is dead
  in normal play.
- **Consequence:** nothing for a normal slot level. It matters only if a player has typed `WarpToPreset` for the slot's level id (the debug hook would stamp a preset into it); the slot id differs
  from anything the command is normally given, and the builder need not guard against it.
- **Evidence level:** static (handler, globals, consumers); the meaning of the arguments beyond the names above is inference. **Needs game:** none required.

### Q10. The level-size limit (outdoor)

- **Already in section 5 Q1** for the maze (no cap below int32 on the path; the u16 subtile bound of 13107 tiles from origin and size). Outdoor additions (static):
  `0x3ee320` sets the grid size to `size / 8` per axis, truncating (`0x3ee34b..0x3ee371`: `(v + ((v >> 31) & 7)) >> 3` stored at block `+0xb0/+0xb4`), so a size that is not a multiple of 8 loses the
  remainder. Each of the four grids is allocated by `0x3fa7f0` as `h * 4 + w * h * 8` bytes with `int` arithmetic and zero filled; no check exists. At 360 x 360 that is 45 x 45 cells, about 65 KB of cells per grid.
  The file index in the flag is 4 bits (16 files). The warp arrays hold at most 9 entries (`+0x208`, `+0x22c`). Room generation is `0x3ee780`, one room per 8 x 8 cell piece, so the room count grows with the grid and the `+0x258` arrays are sized at run time.
- **Evidence level:** static; the live evidence is the 160 x 32 spike only (section 4). **Needs game:** the section 5 Q1 size check on an outdoor row (SizeX = SizeY = 360, DrlgType 3, origin per section 5 Q2), logging `+0x2c/+0x30`, `+0xb0/+0xb4`, the room count and the grid dumps; observe no crash_guard entry, then
  walk to each end and open the automap.

### Q11. Whether jungle DT1s preload in Act 5

- **Already in section 4 (S1):** jungle ground, palms, ferns and the fallen-tree bridge rendered in Act 5 on the slot level (live, first run). That is a render result;
  a preload list was not read. The client act load `0x86bc0` (section 1) is the planned fallback hook for cross-act tiles and was not needed for the jungle line.
- **Evidence level:** live in section 4; **needs game** only for other tile sets (a tile set from an act other than the slot's): build the level with that LvlType and watch for black or missing tiles.

### Q12. The HD preset path for each DS1

- **Static (data):** the paths are the `File1..File6` columns of lvlprest.txt, relative to `data/global/tiles/`. The slot's pieces 530..574
  (`Act 3 - Jungle ...`) use 79 files, for example `Act3/Jungle/RivW.ds1` (530), `RivE.ds1` (531), `RivEW.ds1` (532), `RivNSEW.ds1` (544), `RivWoE.ds1` (545); all 79 exist under `d2r-data/93847/data/global/tiles/act3/jungle/` in the toolkit tree. There is no separate HD copy:
  the toolkit's knowledge files record that `.dt1` and `.ds1` map files are the classic ones and are loaded in HD too (`knowledge/hd-graphics.md`, `knowledge/tables.md`).
  The dump has the act prefixes `data/global/tiles/act1/`..`act4/` and `data/global/tiles/expansion/` (`0x1d0db10..0x1d0db70`) and the base `data/global/tiles/` (`0x1d132a0`); the
  function that joins a lvlprest file name to its prefix was not located.
- **Evidence level:** static from the data; inference for the join. **Needs game:** run with a file-open trace (or a log of the loader's file hook) while the slot builds a piece, and read the path opened for `RivEW.ds1`; expect `data/global/tiles/act3/jungle/rivew.ds1`
  (or the act-4 `expansion/` name for Act 5 rows, which the section 4 run does not need because the slot uses the Act 3 files by their lvlprest path).

### Left for the game

1. Q3: the `+0x58` grid after the build, and whether a `0x100` cell stays out of the room maker.
2. Q4: a hook-built block with `+0xb8` left zero, through one teardown.
3. Q6: the arguments and result of `0x3da650` on red portal entry, and the position reached.
4. Q7: the single-bit collision dumps (`0x1`, `0x8`, `0x400`, `0x800`, `0x1000`) on the water, then the offline DS1/DT1 comparison.
5. Q8: the `0x3ecfc0` call log while building levels 111, 112 and 117 (origin of pieces 915..930).
6. Q10: a 360 x 360 outdoor row (build, walk, automap).
7. Q11: a tile set from a different act than the slot's.
8. Q12: the file path actually opened for a jungle DS1.

### Signed for the outdoor slot (desk work, 2026-11-12)

Four outdoor parts now have a signature in `plugins/cartographer/src/core/part_signatures.h` and a row in `plugins/cartographer/tools/check_maze_signatures.py`; they were checked, not run, until section 8a (the hook lengths in the table below are the spike's; section 8a has the derived ones).

| Part | RVA | Signature source | Hook length | Evidence |
|---|---|---|---|---|
| `OutdoorEntry` | `0x3ee320` | section 4 S0 table, same bytes | 19 in the spike; not re-derived | static: unique, resolves to the RVA, guard holds (checker run on the 93847 dump, stamp `0x6ab3782c`); the spike's live match is section 4 |
| `OutdoorAct5Builder` | `0x3fe2e0` | section 4 S0 table | 16 in the spike | same |
| `PieceStamp` | `0x3ecfc0` | section 4 S0 table | called only | same |
| `VoidCell` | `0x3ede00` | the 62-byte masked signature of the section 8 table; it ends with `41 B9 00 01 00 00` (`mov r9d, 0x100`), where its twin `0x3edda0` has `45 33 C9` (`xor r9d, r9d`), so the twin cannot match | called only (hookable by the 14-byte rule) | static, same checker (the signature matches `0x3ede00` only; without the last 6 bytes it matches both); never run in a game |

The outdoor slot itself (moved here from the plugin README, which is for players): outdoor maps (`Jungle` first) are planned to open through a third level, the `Outdoor` slot, a `levels.txt` level with `DrlgType` 3 and `LevelType` 21 listed in `slots.txt` as an `Outdoor` row. What exists is groundwork only, none of it verified in game and none of it called by the plugin: `slots.txt` accepts the row, the loader reads that level's `DrlgType` and `LevelType` (`ReadSlotLevelTypes`, the one part that runs), `OutdoorSlotProblem` can compare them with a map's tileset but nothing calls it yet, and the piece picker and stamp plan (`src/outdoor/`, unit tested) turn a layout into the pieces, level size and void cells. No hook exists and no map opens through the slot: `Jungle` and `Snow` maps are still refused, and an `Outdoor` row changes nothing in game. `planStamps` refuses every bad input itself (including piece 529 and a pinned file over 15); `checkStampPlan` is the independent second check.

The checker rows carried max patch 999 ("only checked") until the outdoor build hook (section 8a) derived them. `VoidCell` is reached in the engine only through function pointers (Q3), so calling it from a plugin is untested: calling it directly, rather than setting the grid with `0x3faa50`, is an assumption that the first game check settles.
