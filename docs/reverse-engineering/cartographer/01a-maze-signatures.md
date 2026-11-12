## 1a. Maze signatures (issue #29)

Supersedes "Guard: not yet" for these nine maze rows (`0x3eb850`, `0x3eb960`, `0x3e9820`, `0x3e77a0`, `0x32c6a0`,
`0x360bb0`, `0x360520`, `0x360470`, `0x360c80`); other rows are unchanged. Evidence for all of them is static (a dump of the
running image), not game.

| Field | Record |
|---|---|
| Date | 2026-11-11 |
| Consumer | Cartographer guided maze (plan section 5, phase 3); hooked in `plugins/cartographer/src/engine/maze_hooks.cpp` (issue #78) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, under D2RLoader 1.3.1; read from a memory-layout image of the running D2RLoader process (see Evidence), SHA-256 not recorded |
| Location | Function entry RVAs in the table; every signature starts at the entry (offset 0), so signature match address = function RVA |
| Guard | The whole signature compared under its mask at the resolved RVA (a hook does this before it patches). Separately, each row records a max patch length: the bytes from the entry before the first relative branch, call or RIP-relative instruction (the same definition as section 3; no RIP-relative instruction comes first in any of these nine, so the numbers hold), so a phase 3 patch must not split or overwrite one unless D2RLoader relocates it. Three entries are short: `0x32c6a0` 12, `0x360470` 11, `0x360c80` 13 (under the workflow's 14), so for those the masked full signature (25 bytes) is the check |
| Contract | Roles as in section 1. Wildcards (`??`) cover relative call and branch displacements and, in `0x32c6a0`, the data-table offset and record size of `imul rcx,[rax+0x18c8],0x1c` (they shift whenever a txt table is added; with them masked the signature is still unique). No RIP-relative data is in a signature |
| Evidence | Disassembly of the entries (Capstone, over the dump); each signature is the shortest whole-instruction prefix of at least 24 bytes that matches once (after masking) across all executable sections (`.text`, `.maho`, `.koishi`, `.btext`); `.pdata` and direct-call xrefs checked: every address except `0x360470` and `0x360c80` is a `.pdata` function start, those two are leaf functions without unwind data and have 39 and 29 direct callers. Direct callers (E8 only): `0x3e9820` and `0x3e96d0` only from the maze builder `0x3e69e0` (`0x3eb850` has 44 call sites in six functions, `0x3e69e0` among them); `0x3e77a0` calls the overlap checks and `0x360bb0` (consistent with section 1); `0x32c6a0` has one caller, `0x3e7760` |
| Validation | Static only (the `Evidence` column of the table below says `static` for every row): `plugins/cartographer/tools/check_maze_signatures.py` run on the 93847 image, 9 of 9 unique and resolving to the recorded RVAs. Not tested in game, not checked on Steam build 94995 (same unpacked image per the registry, unverified here), no hook written |
| Failure mode | Fail open: a signature with no match or more than one match, or a guard mismatch, logs which function and leaves the guided maze off |
| Coexistence | The dump came from a running D2RLoader process, so any detours D2RCore had applied by then would show in it: none of the nine entries starts with `E9` or `FF 25` (they start with `41 56`, `48 89`, `89 54`, `40 55`, `48 89`, `48 89`, `40 53`, `8B 01`, `48 8B`). That covers only patches present when the dump was taken and the plugins loaded then. If D2RCore or another plugin detours an entry later, the guard fails and the hook stays off |

| RVA | Role | Signature (offset 0 = function entry) | Offset | Guard / max patch bytes | Evidence (static / game) |
|---|---|---|---|---|---|
| `0x3eb850` | Entrance and exit placer | `41 56 48 83 EC 30 44 8B 0A 4D 8B F0 48 89 5C 24 40 4C 8B D2 48 8B 59 10` | 0 | 24 / 47 | static, unique in 93847 |
| `0x3eb960` | Piece picker | `48 89 5C 24 18 48 89 6C 24 20 57 41 56 41 57 48 83 EC 20 4C 8B 09 33 DB` | 0 | 24 / 36 | static, unique in 93847 |
| `0x3e9820` | Ring grower | `89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 48 48 8B 49 10` | 0 | 27 / 39 | static, unique in 93847 |
| `0x3e77a0` | Add-room | `40 55 56 57 41 56 41 57 48 83 EC 30 48 63 EA 48 8B F9 48 8B 89 90 00 00 00` | 0 | 25 / 33 | static, unique in 93847 |
| `0x32c6a0` | LvlMaze lookup | `48 89 5C 24 08 57 48 83 EC 20 8B FA E8 ?? ?? ?? ?? 48 6B 88 ?? ?? ?? ?? ??` | 0 | 25 / 12 | static, unique in 93847 |
| `0x360bb0` | Link two rooms | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 8B D8 48 8B F2 48 8B F9 E8 ?? ?? ?? ?? 44 8D 43 FE` | 0 | 33 / 24 | static, unique in 93847 |
| `0x360520` | Overlap check A | `40 53 57 48 83 EC 38 48 8B DA 48 8D 44 24 60 48 8D 54 24 58 48 89 44 24 20` | 0 | 25 / 38 | static, unique in 93847 |
| `0x360470` | Overlap check B | `8B 01 4C 8B D1 44 8B 0A 41 3B C1 7D ?? 44 2B 49 08 44 2B C8 EB ?? 2B 42 08` | 0 | 25 / 11 | static, unique in 93847 |
| `0x360c80` | Overlap check C | `48 8B 41 10 4D 8B D8 4C 8B D2 48 85 C0 74 ?? 49 3B C2 74 ?? 49 3B C3 74 ??` | 0 | 25 / 13 | static, unique in 93847 |

All nine recorded RVAs are function entries in this build; none looks wrong. The maze builder `0x3e69e0` and add-room
caller `0x3e7760` are named here only as evidence; they have no signatures.

Re-check on a new build: write a memory-layout image with d2rAI-toolkit's `tools/dump_image.py` (game running through
D2RLoader), then `python -I plugins/cartographer/tools/check_maze_signatures.py <image> --stamp <stamp>`. The
packed `D2R.exe` on disk cannot be checked (encrypted code).

### Hooked maze signatures (issue #78)

Two more rows, for the hooks of `plugins/cartographer/src/engine/maze_hooks.cpp`. Static evidence only (the dump of section 1a,
`check_maze_signatures.py`: unique, resolving to the RVA); not run in game.

| RVA | Role | Signature (offset 0 = function entry) | Hook cut / max patch bytes |
|---|---|---|---|
| `0x3271c0` | Level build (writes the slot level's size, then runs the original) | `48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2` | 14 / 48 |
| `0x3e96d0` | Maze walk (steered for the slot level) | `48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 81 C8 01 00 00 48 8B F9 48 8B 41 18` | 18 / 50 |

Hook cuts (whole instructions, all exact bytes): `0x3271c0` 14, `0x3e96d0` 18, `0x3eb850` 17, `0x3e9820` 14. Cartographer hooks these four (run in game 2026-11-12, section 5).

**One hook on `0x3271c0`, two users.** The outdoor builder plan also needs the level build. The loader refuses a second `InstallInlineHook` on the same entry (its guard no longer matches), so the outdoor work must not install its own: it will share this one hook, which dispatches by level (the maze slot to the maze code, the outdoor slot to the outdoor code, every other level to the original). The rows `0x3e77a0`,
`0x360bb0` and `0x3eb960` are only called. All seven must be found or the maze steering stays off (fail open, logged per part).
