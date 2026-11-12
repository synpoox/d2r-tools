# Cartographer native contracts (`cartographer`)

**Status:** the nine maze rows in section 1a have byte signatures, checked statically against a build 93847 dump (no game
evidence, no hook written); every other row is still a stub, not validated in this repo. Cartographer has no plugin code in `plugins/` yet.
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are D2R.exe RVAs (image base `0x140000000` + RVA); final addresses will be found by
byte signature, not fixed RVAs. The `0x3e2b5ce` / `[0x3e2ac38]` row is a D2RLoader stub in D2R.exe whose jump target is
expected in D2RCore (to be logged in S0). `0x3e2b5c8` (6 bytes before it) is probably another such stub; unproven.
**Evidence source:** the pd2-resurrected research and spikes, not re-checked here:
- `docs/research/drlg-steering.md` on pd2-resurrected branch `docs/drlg-steering-spike` (pull 929);
- `docs/research/level-regeneration.md` on pd2-resurrected branch `claude/level-regen-spike-117061` (pull 930);
- `spike/cartographer-steer-proto`: the maze steering prototype (`plugin/cartographer-steer-spike/plugin.cpp`);
- `spike/cartographer-level-regen`: the level regeneration spike;
- `spike/cartographer-outdoor-s1` at b78cfe2: the outdoor builder spike (uncompiled).

All repository links are under https://github.com/synpoox/pd2-resurrected.

Every row in section 1 has: Build 93847, Guard "not yet: needs a byte signature" (except the nine maze rows of
section 1a, which have one), Status "not validated in this repo" (the 1a rows: static only), and
Failure mode "fail open: log and leave Cartographer's map off". The tested image stamp is the registry's
(`0x6ab3782c`). Coexistence: D2RCore patches or thunks some DRLG code (`0x3e2b5ce`); see that row.

## 1. Addresses

| RVA | Role (per the plans) | Evidence pointer |
|---|---|---|
| `0x3271c0` | Level build `(dataFlag, level)`: reseeds `+0x1e4`, dispatches on level `+0x00` (type 3 goes to the D2RCore stub), runs the tail. Planned hook | outdoor plan §2; `spike/cartographer-steer-proto` |
| `0x3e96d0` | Maze random walk `(level)`: adds rooms until the room count is reached. Replaced for the map level | drlg-steering.md; steer-proto |
| `0x3eb850` | Maze entrance and exit placer `(level, int[4], int*)`. Steered | drlg-steering.md; steer-proto |
| `0x3eb960` | Maze piece picker `(room, clearFixed)`: piece = tileset base + exit mask (base hardcoded). Planned hook for the piece set | drlg-steering.md section 2.3; steer-proto |
| `0x3e9820` | Maze ring grower. No-op in the planned steering for more tilesets | drlg-steering.md |
| `0x3e77a0` | Maze add-room: adds one room to the maze. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x32c6a0` | LvlMaze lookup: finds the LvlMaze row for a level. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x360bb0` | Links two rooms (called after the overlap checks `0x360520`, `0x360470`, `0x360c80`); also used to link loop edges | drlg-steering.md lines 148, 188 |
| `0x3ee320` | Outdoor grid builder; dispatches on act, calls `0x3fe2e0` for Act 5, then `0x3ee780` | outdoor plan 1.2; `spike/cartographer-outdoor-s1` |
| `0x3fe2e0` | Act 5 grid allocation, reached from `0x3ee320`. Planned byte-checked hook that runs the stamper for the slot level. Finding: `0x3ee320` reaches it only when D2RCore's act lookup `0x3e2b5c8` returns 4 for the slot id | outdoor plan 1.2; outdoor-s1 |
| `0x3ecfc0` | Stamps a piece `(dataFlag, level, cx, cy, lvlprest, file or -1, 0)`; sizes it from LvlPrest, marks cells, draws the file from the level RNG at `+0x1e4` | outdoor plan; outdoor-s1 |
| `0x3ee780` | Makes the preset rooms from the stamped piece-origin cells | outdoor plan; outdoor-s1 |
| `0x3fa7f0` | Allocates one of the four grids (`+0x08`, `+0x30`, `+0x58`, `+0x80`) | outdoor plan 1.2 |
| `0x3fac10` | Per-act builder; probe step counts its calls to see whether D2RCore still uses D2R's per-act builders | outdoor plan, probe step S0 |
| `0x3e2b5ce` / `[0x3e2ac38]` | D2RLoader stub `JMP [0x3e2ac38]` into D2RCore, taken by `0x3271c0` for type 3. Which module holds the target is to be logged | outdoor plan 2 |
| `0x3e2b5c8` | D2RCore's act lookup for a level id; expected 4 for the slot (levels.txt `Act`) | outdoor plan 1.1; outdoor-s1 |
| `0x3f3970` | Room loader: adds a room's DT1s from `level+0x1e0`. Planned hook for mixed tilesets (plan phase 9) | outdoor plan; cartographer-plugin.md |
| `0x86bc0` | Client act load: preloads only the current act's LvlTypes DT1s. Fallback hook for cross-act tiles | outdoor plan |

## 1a. Maze signatures (issue #29)

Supersedes "Guard: not yet" for these nine maze rows (`0x3eb850`, `0x3eb960`, `0x3e9820`, `0x3e77a0`, `0x32c6a0`,
`0x360bb0`, `0x360520`, `0x360470`, `0x360c80`); other rows are unchanged. Evidence for all of them is static (a dump of the
running image), not game.

| Field | Record |
|---|---|
| Date | 2026-11-11 |
| Consumer | Cartographer guided maze (plan section 5, phase 3); no hook exists yet |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, under D2RLoader 1.3.1; read from a memory-layout image of the running D2RLoader process (see Evidence), SHA-256 not recorded |
| Location | Function entry RVAs in the table; every signature starts at the entry (offset 0), so signature match address = function RVA |
| Guard | The whole signature compared under its mask at the resolved RVA (a hook does this before it patches). Separately, each row records a max patch length: the bytes from the entry before the first relative branch or call, so a phase 3 patch must not split or overwrite one unless D2RLoader relocates it. Three entries are short: `0x32c6a0` 12, `0x360470` 11, `0x360c80` 13 (under the workflow's 14), so for those the masked full signature (25 bytes) is the check |
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

## 2. Not yet recorded

Signatures for `0x3e96d0`, `0x3f3970` and `0x86bc0` (the other rows have signatures in section 1a or, once #34 merges,
section 3), contracts beyond the roles above, and any game validation of the maze rows. Until then no hook may be
written from this page alone for the rows without a signature (AGENTS.md); the 1a rows are static only.
