# Cartographer native contracts (`cartographer`)

**Status:** the nine maze rows in section 1a and the twelve phase 1 parts in section 3 have byte signatures, checked statically against a build 93847 dump (no game
evidence; the portal hooks are written, not validated in game). Section 5 answers the maze slot's level size, origin and `0x3eb850` questions statically (issue #62), with the in-game checks still open. Section 6 answers, statically, how to apply stats to monsters, the player and the map item, how to override region density and level for one level, the event entry points and the entry text path (issue #72), each claim marked static, inference or needs game. Section 8 answers, statically, the twelve open outdoor builder questions (issue #83), each marked static, inference or needs game with the exact in-game check. The outdoor rows (section 4) have byte signatures that matched on build 93847 in a test slot, and the spike exercised the parts live at the levels shown per row. Every other row of sections 1 and 2 is still a stub, not validated in this repo.
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are D2R.exe RVAs (image base `0x140000000` + RVA); final addresses will be found by
byte signature, not fixed RVAs. The `0x3e2b5ce` / `[0x3e2ac38]` row is a D2RLoader stub in D2R.exe whose jump target was
logged inside `D2RCore.dll` (section 4, S0). `0x3e2b5c8` (6 bytes before it) is the same kind of stub, also into `D2RCore.dll`.
**Evidence source:** the pd2-resurrected research and spikes, not re-checked here:
- `docs/research/drlg-steering.md` on pd2-resurrected branch `docs/drlg-steering-spike` (pull 929);
- `docs/research/level-regeneration.md` on pd2-resurrected branch `claude/level-regen-spike-117061` (pull 930);
- `spike/cartographer-steer-proto`: the maze steering prototype (`plugin/cartographer-steer-spike/plugin.cpp`);
- `spike/cartographer-level-regen`: the level regeneration spike;
- `spike/cartographer-outdoor-s1` at b78cfe2: the pd2 outdoor builder spike (uncompiled; reference only);
- `spike/cartographer-outdoor-s1` in this repository (synpoox/d2r-tools): the compiled spike `plugins/cartographer-outdoor-spike/`
  at 8d377ca (S0 probe, S1 jungle line) and the follow-up commit named in section 4; it is the evidence for section 4.

The pd2-resurrected links above are under https://github.com/synpoox/pd2-resurrected.

Every row in section 1 has: Build 93847, Guard "not yet: needs a byte signature" (except the nine maze rows of
section 1a, which have one, and the rows marked "signed", which carry a signature that matched exactly once in the running image; their Guard and Validation are in section 4), Status "not validated in this repo" (the 1a rows: static only), and
Failure mode "fail open: log and leave Cartographer's map off". The tested image stamp is `0x6ab3782c`, read
from the loaded image and logged by the outdoor spike at load (section 4). Coexistence: D2RCore patches or thunks some DRLG code (`0x3e2b5ce`); see that row.

## 1. Addresses

| RVA | Role (per the plans) | Evidence pointer |
|---|---|---|
| `0x3271c0` | Level build `(dataFlag, level)`: reseeds `+0x1e4`, dispatches on level `+0x00` (type 3 goes to the D2RCore stub), runs the tail. Signed; hooked and replaced for the slot level in the spike (§4) | outdoor plan §2; §4 |
| `0x3e96d0` | Maze random walk `(level)`: adds rooms until the room count is reached. Replaced for the map level. Signature recorded in section 1a; hooked by Cartographer (run in game 2026-11-12, section 5) | drlg-steering.md; steer-proto |
| `0x3eb850` | Maze entrance and exit placer `(level, int[4], int*)`. Steered | drlg-steering.md; steer-proto |
| `0x3eb960` | Maze piece picker `(room, clearFixed)`: piece = tileset base + exit mask (base hardcoded). Planned hook for the piece set | drlg-steering.md section 2.3; steer-proto |
| `0x3e9820` | Maze ring grower. No-op in the planned steering for more tilesets | drlg-steering.md |
| `0x3e77a0` | Maze add-room: adds one room to the maze. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x32c6a0` | LvlMaze lookup: finds the LvlMaze row for a level. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x360bb0` | Links two rooms (called after the overlap checks `0x360520`, `0x360470`, `0x360c80`); also used to link loop edges | drlg-steering.md lines 148, 188 |
| `0x3ee320` | Outdoor grid builder; dispatches on act, calls `0x3fe2e0` for Act 5, then `0x3ee780`. Signed; hooked and counted in the spike, called for the slot level (§4) | outdoor plan 1.2; §4 |
| `0x3fe2e0` | Act 5 grid allocation, reached from `0x3ee320`. Signed; hooked in the spike, which runs the stamper for the slot level instead. Finding: `0x3ee320` reaches it only when D2RCore's act lookup `0x3e2b5c8` returns 4 for the slot id (it did, §4) | outdoor plan 1.2; §4 |
| `0x3ecfc0` | Stamps a piece `(dataFlag, level, cx, cy, lvlprest, file or -1, 0)`; sizes it from LvlPrest, marks cells, draws the file from the level RNG at `+0x1e4`. Signed; called from the spike's `0x3fe2e0` hook, not hooked (§4) | outdoor plan; §4 |
| `0x3ee780` | Makes the preset rooms from the stamped piece-origin cells. Signed; hooked and counted in the spike (§4) | outdoor plan; §4 |
| `0x3fa7f0` | Allocates one of the four grids (`+0x08`, `+0x30`, `+0x58`, `+0x80`). Signed only: matched, never hooked or called by the spike; the native `0x3ee320` / `0x3fe2e0` call it | outdoor plan 1.2; §4 |
| `0x3fa790` | Grid read `(grid + field, x, y) -> u64`, used for the grid dumps. Signed; called by the spike, not hooked | §4 |
| `0x367110` | Level reseed `(level + 0x1e4, seedBase + level id)`: the first `E8` call in `0x3271c0` (`+0x30`). Found by that call, not signed; called by the spike | §4 |
| `0x3fac10` | Act 1 per-act builder `(dataFlag, level)`. Signed; hooked and counted: still called for levels 2 and 3 when a game starts (§4). Callers not logged | outdoor plan, probe step S0; §4 |
| `0x3e2b5ce` / `[0x3e2ac38]` | D2RLoader stub `JMP [0x3e2ac38]` into D2RCore, taken by `0x3271c0` for type 3. Found by following the first `E8` in `0x3271c0` whose target starts `FF 25`; the pointer held an address inside `D2RCore.dll` (§4) | outdoor plan 2; §4 |
| `0x3e2b5c8` | D2RCore's act lookup `int8 (u8 dataFlag, int32 level id)`, a `FF 25` stub into `D2RCore.dll`. Found as the first `E8` in `0x3ee320` whose target starts `FF 25`, not a fixed RVA. Returned 4 for the slot id 138 (§4) | outdoor plan 1.1; §4 |
| `0x3f3970` | Room loader: adds a room's DT1s from `level+0x1e0`. Planned hook for mixed tilesets (plan phase 9) | outdoor plan; cartographer-plugin.md |
| `0x86bc0` | Client act load: preloads only the current act's LvlTypes DT1s. Fallback hook for cross-act tiles | outdoor plan |

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

## 2. Not yet recorded

Signatures for `0x3f3970` and `0x86bc0` (the other rows have signatures in section 1a or, for the outdoor rows, section 4, or are found from a signed function by following its calls, as `0x367110`, `0x3e2b5ce` and `0x3e2b5c8` are), contracts beyond the roles above, and any game validation of the maze rows. Until then no hook may be
written from this page alone for the rows without a signature (AGENTS.md); the 1a rows are static only.

## 3. Phase 1 parts: the map item and the red portal (`plugins/cartographer`, issue #20)

**Date:** 2026-11-11 (validated 2026-11-12). **Status:** validated in game on build 93847, image stamp `0x6ab3782c`
(D2R 3.3.0, D2RLoader 1.3.1-beta, test slot with the D2RToolsTest mod and a test save, Normal difficulty): all thirteen
parts found by signature at load, the three hooks installed, the item opened its own map's portal, the map loaded and
returned, a second open was refused, the Hell gate refused, the console `open` worked, and the tick's unit id matched
`LocalPlayerReady.playerId`. A second pass on 2026-11-12 (Hell game, level-1 then level-25 Barbarian) settled more; the
per-check levels are in "Game checks of 2026-11-12" at the end of this section. Still not validated in game: the free-spot
refusal (tried, not reproduced) and the TCP/IP client refusal (what would settle each is in that table); the spell-16 use
with no matching check is not reachable through its one direct caller and stays unit tested only.

Cartographer finds every part below by byte signature at load. A part needs exactly one match in the game's executable
sections; otherwise it stays off, the reason goes to the plugin log and `cartographer check`. The two item hooks are
written at load, and only then, whenever their own two parts resolve; until every required part is on they pass straight
through to the game (fail open: the item does what D2R does with an unknown spell). The portal is live only when all
required parts are on, and `cartographer check` can turn it on later by re-finding the called parts. The use hook goes
in first and acts only while the portal is live, so no portal can open ungated. A signature for any part can be supplied without a rebuild in `signatures.txt` (columns `Part`, `Signature`),
beside `maps.txt`; it replaces the built-in one.

Build 93847, image stamp `0x6ab3782c`. The RVAs are evidence from pd2-resurrected `plugin/core-maps/map_open.cpp` and
`plugin/common/pd2_engine.h` (UnitStat's from d2rAI-toolkit `rvas_93847.h`); the plugin finds the parts by signature and
does not use them. The signatures are in `plugins/cartographer/src/core/part_signatures.h`.

| Field | Record (issue #39) |
|---|---|
| Date | 2026-11-11 |
| Consumer | Cartographer portal (`src/engine/portal.cpp`) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, under D2RLoader 1.3.1; memory-layout image of the running D2RLoader process (d2rAI-toolkit `tools/dump_image.py`). SHA-256 not recorded: the image is read from process memory, so it is not reproducible byte for byte (D2RLoader writes runtime pointers into it, such as the `UnitStat` thunk slot); the stamp and the checker output identify it |
| Location | Function entry RVAs in the table; every signature starts at the entry (offset 0), so the match address is the function |
| Guard | The whole signature compared under its mask at the resolved address; for a part that is only called, a unique match at the entry is the whole guard. For the two hooked parts the guard D2RLoader re-checks is the signature's own first bytes (15 and 14, no `??`), not bytes copied from the live entry at load. Max patch length (hooked parts only): the bytes from the entry before the first relative branch, call or RIP-relative instruction, so a patch must not split or overwrite one; both hooks copy fewer (15 of 44, 14 of 38). Called-only parts show `n/a (called)`: they are never patched, and the value would only invite it. The column reads signature length / max patch, as in section 1a |
| Contract | Roles and signatures in the table. Wildcards (`??`) cover relative branch and call targets, RIP-relative displacements, struct-table offsets (`ItemRecord`: the table count offset `+0x15a8`) and the thunk's jump slot (`UnitStat`). One exception: `GameCounter` keeps one call displacement unmasked (below) |
| Evidence | Disassembly of each entry (Capstone, over the dump). Each signature is the shortest whole-instruction prefix of at least 24 bytes that matches once, with masked bytes as wildcards, across all executable sections (`.text`, `.maho`, `.koishi`, `.btext`; each section's VirtualSize, as the plugin scans). Roles checked from the code: `OpenRedPortal` calls `UnitRoom`, a room check, `GameCounter` on `game + 0x1aac8` (read, and incremented after a successful opening), `UnitCoords` and `FindFreeSpot` (`3, 0x400, 5, 100` as recorded), a create call with object 60, and `ClientMessage(player, 0x15, player)` on a refusal; `UnitClassOf` returns `[unit + 4]`; `RoomLevelId` follows `[room + 0x18]` into a function reading `[[level + 0x90] + 0x1f8]`; `ItemRecord` is a bounds-checked `base + row * 0x1c0` over a vector at table `+0x15a0` |
| Validation | Static, then in game: `plugins/cartographer/tools/check_portal_signatures.py` on the 93847 image (with `--stamp 0x6ab3782c`), 13 of 13 (the twelve plus `PlayerTick`) unique and resolving to the recorded RVAs, and `GameCounter` confirmed as the target of two calls in `OpenRedPortal`; unit tests parse every signature and check each is at least 24 bytes (uniqueness is the checker's job). Then run in game on build 93847 (see the status above); not checked on Steam build 94995 |
| Failure mode | Fail open: a signature with no match or more than one match, or a guard mismatch, logs which part and leaves the portal off |
| Coexistence | The dump is of a D2RLoader process, so detours already in place show in it. None of the hooked or called entries starts with `E9`; `UnitStat` starts with D2RLoader's own `FF 25` thunk (below). Another plugin detouring a part later fails that part's guard |

| Part | Role | RVA | Signature (offset 0 = function entry) | Offset | Guard / max patch bytes | Required |
|---|---|---|---|---|---|---|
| ItemSpellCheck | Hooked. `(game, player, item, target) -> u64`; item spell 16 is the map | `0x5817f0` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 0 | 30 / 44 (hook copies 15) | yes |
| ItemSpellUse | Hooked. `(game, player, item, target)`; runs the opening | `0x581680` | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 0 | 24 / 38 (hook copies 14) | yes |
| ItemRecord | Called. `(dataFlag, row) -> record`; record +0x80 code, +0x94 pSpell | `0x314110` | `40 57 48 83 EC 30 8B FA E8 ?? ?? ?? ?? 3B B8 ?? ?? ?? ?? 72 ?? 33 C0 48 83 C4 30` | 0 | 27 / n/a (called) | yes |
| UnitClassOf | Called. `(unit, file, line) -> row` | `0x349860` | `48 83 EC 28 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC B8 FF FF FF FF 48 83 C4 28 C3 8B 41 04` | 0 | 41 / n/a (called) | yes |
| UnitRoom | Called. `(unit) -> room` | `0x34b440` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9 74 ?? 48 83 C4 20` | 0 | 63 / n/a (called) | yes |
| RoomLevelId | Called. `(room) -> level id` | `0x2efc10` | `48 85 C9 75 ?? 33 C0 C3 48 8B 49 18 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 85 C9` | 0 | 35 / n/a (called) | yes |
| ClientMessage | Called. `(player, 0x15, player)`: "can't do that" | `0x491960` | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 49 8B F8 0F B7 F2 48 8B D9` | 0 | 24 / n/a (called) | yes |
| OpenRedPortal | Called. `(game, player, key, level, max)`: red portal 60 by the player; counts openings per key and refuses at `max` (the one-map-per-game rule) | `0x5da800` | `48 89 5C 24 08 48 89 6C 24 10 44 89 44 24 18 56 57 41 56 48 83 EC 70 48 8B E9` | 0 | 26 / n/a (called) | yes |
| GameCounter | Called. `(game + 0x1aac8, &key) -> int*`; key `'MAPS'` (`0x4d415053`) | `0x5d88b0` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 41 08 4C 8B F2 48 8B D9 4D 85 C0 75 ?? 48 8B 11 E8 A4 3F 00 00` | 0 | 44 / n/a (called) | yes |
| UnitStat | Called. `(unit, stat, layer) -> int`; stat 12 is the level | `0x2f5020` (d2rAI-toolkit `rvas_93847.h`; pd2-resurrected: a jump into D2RCore) | `FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA` | 0 | 26 / n/a (called) | yes |
| UnitCoords | Called. `(unit, int xy[2])` | `0x34a040` | `48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 85 FF 75 ?? 48 8D 4C 24 30 40 88 7C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9` | 0 | 89 / n/a (called) | no (spot check skipped) |
| FindFreeSpot | Called. `(game, room, xy, 3, 0x400, &found, 5, 100)` | `0x517f90` | `48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 90 00 00 00 48 8B 05 ?? ?? ?? ??` | 0 | 30 / n/a (called) | no (spot check skipped) |
| PlayerTick | Hooked (issue #40). The player mana step `(game, player) -> al`; the server runs it once per frame for every live player, so the console command gets a game and a player there. Optional: the map item does not need it | `0x42e350` | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA 4C 8B F9 48 85 D2 0F 84 ?? ?? ?? ?? 45 33 C0 48 8B CF 41 8D 50 08 E8 ?? ?? ?? ?? 48 8B CF 8B F0 E8 ?? ?? ?? ?? 33 DB 48 8B CF 8B E8 8D 53 55 E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ??` (81 bytes, `??` on every rel32; hook cuts the first 14: five whole instructions, no relative operand) | 0 | 81 / 27 (hook copies 14; the first rel32 is the `0F 84` at 27) | no (console command only) |

PlayerTick evidence (static, read-only, against `D2RLoader_6ab3782c_dump.exe`, a memory-layout dump made by
d2rAI-toolkit `tools/dump_image.py`): the 81-byte signature has 1 match (RVA `0x42e350`; its first 14 bytes alone match 15
places, so the whole signature is used). It runs past the prologue into the body (null check of the player, the
`8D 53 55` = state 0x55 test), so another function cannot become the unique match by register allocation alone.
Reproduce: read the dump file, convert the signature to a regex (`??` = `.`, with `re.S`) and `finditer` over all bytes:
one match at `0x42e350`. The call at `0x42e74d` (`E8 FE FB FF FF`) resolves to `0x42e350` too. `rdx` is the player and `rcx`
the game (saved to `rdi`/`r15` at entry). Its only caller is the player regen tick `0x42e600` (better-auras.md section 4),
which reaches it on every path for a live player (`0x42e74d`). `0x42e600` itself is hooked by Better Auras, and a second
hook on the same entry is refused, so the tick part uses `0x42e350` instead. Contract: the hook calls the original
first, then serves a posted console request when the unit is a player (`+0` == 0) whose unit id (`+8`) equals the
`playerId` of the SDK's `LocalPlayerReady` event, using the SDK event as the source of "which player is local". That
`playerId` is the unit id is an assumption (not documented by the SDK): if it is wrong, the request is dropped after 10 s
with a log line naming both ids; the hook also logs the first player unit id it sees next to `LocalPlayerReady.playerId`.
`cartographer open` refuses (no arming) when the part is off, and in a game with no server tick (a TCP/IP client).
The portal opens from inside the unit timer event (`0x42e600`), not the item-use handler: needs the in-game re-entrancy
checks in the plugin README. In game the console `open` worked in a
Normal-difficulty game (section 3, Game checks); the TCP/IP client refusal was not exercised.

Evidence level of every row: static, dump (not game). Eleven of the twelve RVAs are `.pdata` function entries in this build; `RoomLevelId` (`0x2efc10`) is a frameless leaf with no `.pdata` entry (the nearest `.pdata` entry before it starts at `0x2efb70`, a different function). The plugin checks `.pdata` only for hooked parts overridden in `signatures.txt`, so `RoomLevelId` needs only a unique match. None looks wrong. Findings per part:

- **ItemSpellCheck, ItemSpellUse:** the 15 and 14 entry bytes recorded earlier were not unique (3140 and 11 matches in the
  executable sections), so with them both parts would have stayed off. Extended to 30 and 24 bytes, each unique; the hook
  sizes are unchanged and inside the max patch length.
- **UnitStat (load timing):** the parts are looked up once at plugin load, so the thunk must already be in place then; the dump
  shows it only in a running process. If it is not, the part and the portal stay off until `cartographer check` looks again
  (it re-scans all called parts with the load-time overrides and turns the portal on if every required part is found; it never
  writes code: the two item hooks are installed at load when their own parts resolve, and pass through to the game until the
  portal is on, so a missing hooked part needs a restart). Put "UnitStat found at load" on the in-game checklist.
- **UnitStat:** `0x2f5020` is a D2RLoader thunk, `jmp [rip+x]` plus four NOPs, into D2RCore (pd2-resurrected found the same);
  the bytes after it are the dead original body, and calling the thunk is the way to read a stat. The signature covers the
  thunk and the start of the dead body, so it exists only while D2RLoader's thunk is in place; without it the part is off.
  The jump slot is masked. Do not patch it.
- **RoomLevelId:** a 17-byte wrapper (`[room + 0x18]`, then a jump). Four wrappers have the same masked bytes (`0x2efc10`,
  `0x2efc30`, `0x2efd00`, `0x2f0720`; the next one, `0x2efc30`, is also a twin and jumps to `0x360fd0`). `0x2efc10` is
  unique only because it is the one twin followed by padding and another twin, so the signature takes in the 15 bytes of padding
  and the next function's first three bytes. Its uniqueness rests on code layout: a linker reordering, a change to the
  neighbour or to the padding breaks it (fail open). A sturdier way for later is to find it through a caller or its jump
  target `0x360fc0` and follow the rel32.
- **GameCounter:** an `unordered_map<int,int>` lookup, and three copies of it have identical bytes apart from their call
  displacements (`0x5d88b0`, `0xd8590`, `0x8723b0`). No fully masked signature tells them apart, so the first call
  displacement (`E8 A4 3F 00 00`, to the map's rehash helper) is kept unmasked. This is the most build-sensitive signature
  here: a game update that moves the helper turns the part off (fail open), and the signature is then re-derived. `tools/check_portal_signatures.py` also checks that `OpenRedPortal` contains
  two `E8` calls (at `+0x5e` and `+0x13e`) whose target is the matched `GameCounter`, which ties the right copy to a unique part;
  a later step could derive `GameCounter` from there.
- **UnitRoom, UnitCoords:** share an assert-check prologue with many accessors, so the shortest unique prefix is long
  (63 and 89 bytes); `UnitCoords` is optional.

Game layout used: `game + 0x104` difficulty (u8), `game + 0x106` data flag (u8), `game + 0x1aac8` counters; item
record `+0x80` code (4 letters), `+0x94` pSpell. Item spell 16 is free in vanilla D2R. The town is Harrogath, level 109.

Failure mode for every part: fail open, log the reason, leave the portal off. Coexistence: another plugin hooking the
same dispatchers is not handled; D2RLoader refuses a second hook on the same bytes and the part then reports that.

Re-check on a new build: write a memory-layout image with d2rAI-toolkit's `tools/dump_image.py` (game running through
D2RLoader), then `python -I plugins/cartographer/tools/check_portal_signatures.py <image> --stamp <stamp>`. The packed
`D2R.exe` on disk cannot be checked (encrypted code).

### Game checks of 2026-11-12

Hell game on slot 11 with the D2RToolsTest mod, test save `cartos` (progression raised so Hell is offered), vanilla
`levels.txt`, `lvlprest.txt` and `misc.txt` plus the installer's "Cartographer Preset" level 138 and three `misc.txt`
rows with `pSpell` 16 (`t20`, `t21`, `t22`); maps `Shorelines` (Hell, level 1) and `Gated` (Hell, level 20).
Levels: live = seen in the running game; static = read from the dump only; tried = run in game without reaching the result; not done = not tried.

| Check | Level | Result |
|---|---|---|
| Open a map in Hell | live | `t21` in Harrogath opened `Gated` through level 138; a red portal appeared, the scroll was consumed, the portal led to level 138 and back. |
| Level gate refuses | live | Level 1 using `Gated` (needs 20): log "Gated needs character level 20 (you are 1)", item kept. On screen the player sees only "cartos says: I can't use this yet."; the Cartographer wording is in the log and console. |
| Level gate allows | live | After `cheat level 25` the same item opened the portal. This exercises `UnitStat` (`0x2f5020`) on its success path only. |
| Free-spot gate | tried, refusal not reproduced | `cheat mspawn zombie1,100` around the player did not stop the spot search; the portal opened (kept as evidence). A refusal was never seen, so `FindFreeSpot` (`0x517f90`) has only its success path exercised. Static reading is in "Free-spot search" below: the mask tests a collision bit, not a monster crowd, and the search runs out to about 99 subtiles, so a refusal needs every tested window in that area blocked. What would settle it: a debug build that forces `found == nullptr` once, or a spot where collision bit `0x400` is set on every window out to that radius (none known). Until then the refusal is unit tested only (`CheckGates` -> `NoSpot`). |
| Spell-16 item no map names | live | `t22` (spell 16, in no `maps.txt` row): no log line, no message, not consumed, the same as with Cartographer unloaded. |
| Spell-16 use with no matching check | not done | Static: the only direct caller (`0x4f591e`) runs the use only after `ItemSpellCheck` returned nonzero (see "Callers of ItemSpellUse" below), so the case is not reachable through it. It is defensive code, unit tested only, with no in-game test planned; only an indirect caller (none searched) could reach it. |
| Leave game with a portal open, next game | live | Save and Exit from level 138 with the portal open: clean exit, no crash. In the next game `Shorelines` opened (the per-game `'MAPS'` counter was fresh) and the tick logged its first player unit again. Whether that open used the item or `cartographer open` was not recorded, so the console path after a game change is not shown. |
| TCP/IP client refused | not done | Needs a second game instance joined as a TCP/IP client, then `cartographer open <Name>`; not run. The refusal is in the code (no tick seen) and the unit tests. |
| Town portal scroll on the map level | live | `tsc` used in level 138 did nothing: not consumed, no portal, no log line. With Cartographer unloaded the same scroll did the same, so this is vanilla behaviour with a preset-level slot, not Cartographer. |
| Better Auras alongside | live | Both loaded in one game (Better Auras' log: "other plugins: cartographer.dll"); `Shorelines` opened; no new crash report. Cartographer hooks `0x42e350`, Better Auras `0x42e600`, so the two do not meet. |

Gate evidence, as it stands: the order `NotInTown`, `Difficulty`, `CharLevel`, `AlreadyOpen`, `NoSpot` is the code's; in game
the Normal-difficulty refusal, the Hell allow, the level refusal and the level allow were seen in this pass, and the
AlreadyOpen refusal (a second open) and the NotInTown refusal (console `open` outside town) were seen live in #70. `UnitStat`
and `FindFreeSpot` were run only on their success path (a level read, a spot found); neither has been shown to refuse live.
Only `NoSpot` has never been seen refusing.

Free-spot search (static, read-only, `python -I plugins/cartographer/tools/dump_dis.py C:\ghidra-work\D2RLoader_6ab3782c_dump.exe
--stamp 0x6ab3782c dis <rva>`; dump, not game): `FindFreeSpot` (`0x517f90`) takes the `0x400` as its fifth argument and
passes it, with a 7 by 7 window (`3 * 2 + 1`), to `0x364b20` (its seventh argument, `5`, is never read), then `0x365f80`, then `0x3666c0`. That last function ANDs
each 16-bit entry of the room's collision grid (`[room_coll + 0x20]`, one `u16` per subtile) with the mask and ORs the
results; a nonzero result means the window is blocked. So `0x400` is bit `0x400` of the per-subtile collision word, not a
unit count: zombies would matter only if their presence sets that bit, which the 100-zombie run suggests it does not.
The search spirals outward from the player in rings (candidates step by 2 subtiles; windows whose corner is outside the room are skipped) up to the eighth argument (100), moving into neighbouring rooms, and sets
`found` to null only when every tested window up to that radius is blocked or no room exists. What bit `0x400` means (its engine
name) is not established by this dump. The exact in-game check that would reproduce a refusal: stand where bit `0x400` is
set on every 7 by 7 window out to about 99 subtiles (about 20 tiles) of the player. Harrogath has no such spot that we know
of, so a forced `found == nullptr` in a debug build is the practical route. Not run.

Callers of ItemSpellUse (static, dump; `0x581680` and `0x5817f0` found by scanning every `E8` and `E9` in `.text`): each has
exactly one direct caller, both inside one large function (the item-use handler; `0x4f45eb` is its shared tail):
`ItemSpellCheck` at `0x4f52c6`, `ItemSpellUse` at `0x4f591e`. The use block `0x4f590f` is reached only from `0x4f580d` and
`0x4f5816` (both `jne`), in block `0x4f57f4`. That block is reached only from `0x4f55f9` and `0x4f562c`, both in block
`0x4f55f0`, which is reached only from `0x4f5470`, in block `0x4f5438`, which is reached only from `0x4f52cd`: the
`test al, al; jne` right after `call 0x5817f0` at `0x4f52c6`. So the use runs only after `ItemSpellCheck` returned nonzero.
The two jumps at `0x4f580d` (`[record + 4]` of the row from `0x300ae0(dataFlag, ...)` compared with `r13d`) and `0x4f5816`
(`r14b` nonzero) choose between the use and the other branch (`0x4f581c`); they do not skip the check. Through this caller
the "use with no matching check" case is therefore not reachable (static); it is defensive code in Cartographer's hook, and
it stays unit tested only. Indirect callers (vtable or function pointer) were not searched.

Portal label: the red portal's tooltip read "Missing String" when the player stood in Harrogath and "Harrogath" inside level
138. Likely cause: the installer sets level 138's `LevelName` to "Cartographer Preset" and adds no string for that key, so every
installer user would see it; not verified in game. Tracked in #87.

What remains: the free-spot refusal and the TCP/IP client refusal (each row above says what would settle it); the spell-16
use with no matching check stays unit tested only. The hook entries and `UnitStat`'s thunk are checked in the dump and by the game running through them.

## 4. Outdoor S0 and S1 spike results (issue #22)

**Spike:** `spike/cartographer-outdoor-s1-finish` at 7f59e16 (`plugins/cartographer-outdoor-spike/`, never merged; S0 and the level build ran at 8d377ca, the position, collision and walk tooling is 9eeaedc, 7f59e16 adds the load-time stamp and loader log).
**Run:** build 93847, D2RLoader v1.3.1-beta, vanilla RotW test slot 11 (mod `D2RToolsTest`), test character
(Barbarian, Normal), 2026-11-11 (first run) and 2026-11-12 (finishing run). The owner's install was not touched.
**Validation levels reached:** clean build; installed; live in the test slot (signatures, hooks, a level built in game, a
logged-position walk from tile 7 to tile 150 (of 0..159), route-guided to tile 142.8 (one run as operated; continuity not logged); own-room collision reads, automap
screenshots). Live in game means the position log, the collision reads and the screenshots below; unit tests do not apply
to the spike. The DS1 walk flags were not used as evidence.
**Build identity (logged live at load by the spike, 7f59e16):** PE TimeDateStamp `0x6ab3782c`, game build strings the loader reports
(`buildVersion` / `buildName`: `3.3.0` / `93847`), the context's `abiVersion` (4) and `contextSize`. Not logged: PluginSDK 0.4.0 is the build-time submodule
pin, and D2RLoader v1.3.1-beta comes from the loader banner on screen (the loader does not expose its version through the
context).

**Probe commands** (console, test harness): `cartographer pos`, `track on|off|reset` (logs `pos subtile ...` lines and the
reached extents), `coll <mask> [dump]` (collision at the player; `dump` writes an 800x160 subtile file), `peek <addr> <len>`,
`rooms <level ptr>`, plus the S0 commands `cartographer probe-outdoor` (byte checks, the module behind the thunk, call counts) and `cartographer reset` (counters). Config (the spike's own toml): `level` (levels.txt row), `line` (block line, e.g. `E,EW,EW,EW,W`), `offset_x`, `offset_y`, `stamp` (false: leave the Act 5 branch to D2R) and `build`.

### S0

Signatures (hex, unique match in the executable sections of the running image; the hook length is the byte count
the inline hook overwrote, "-" where not hooked). All eight matched exactly once, at the RVA shown, and the hooked ones
reported "tracked" by D2RLoader's hook diagnostics:

| RVA | Hook bytes | Signature |
|---|---|---|
| `0x3271c0` level build | 14 | `48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2 44 0F B6 F9 8B 90 60 08 00 00 49 8D 8E E4 01 00 00 41 03 96 F8` |
| `0x3ee320` outdoor entry | 19 | `48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48 83 EC 50 4C 8B 7A 18 33 C0 4C 8B F2 44 0F B6 E1 49 89 87 A8 00 00 00 49 8D 4F 08 8B 42 2C 99 83` |
| `0x3fe2e0` Act 5 builder | 16 | `40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 48 81 EC C0 00 00 00 83 BA F8 01 00 00 6E 48 8B F2 44 0F B6 F1` |
| `0x3ecfc0` piece stamp | - (called) | `48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 4C 8B 62 18 48 8B FA 48 63 B4 24 90 00 00 00 45 8B E9 8B D6 4C 89 64 24` |
| `0x3ee780` grid to rooms | 15 | `40 55 41 54 41 55 41 56 48 81 EC 98 00 00 00` |
| `0x3fa7f0` grid alloc | - (called) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 49 63 E8 44 8B FA 4C 8B F1 89 51 08 89 69 0C 8D 34` |
| `0x3fa790` grid read | - (called) | `48 8B 41 20 4D 63 C0 48 63 D2 4E 63 0C 80 48 8B 41 18 4C 03 CA 4A 8B 04 C8` |
| `0x3fac10` Act 1 builder | 10 | `48 89 5C 24 08 57 48 83 EC 20 0F B6 F9 48 8B DA 8B 8A F8 01 00 00 8D 41 FE 83 F8 01 76 0D 83 F9 11 74 08 48 8B CA` |

The signatures were built from a decrypted dump of the same build, because the `D2R.exe` on disk is encrypted. How they were cut from the dump (tool, command, where the dump is kept) is not recorded: the spike code only scans for them, it does not show the cutting. The
spike checks every signature at load, before it installs its own hooks, and is all-or-nothing: if any signature fails, every hook stays off
and the plugin logs it and leaves the level to the game (fail open). No signature failed.

- **Module behind the thunk:** `0x3e2b5ce` is `FF 25` through the pointer slot at `0x3e2ac38`, holding an address
  inside `D2RCore.dll` (a loader-mapped copy of the module, size `0x12e0000`; its base is not stable and is not recorded). The act lookup thunk `0x3e2b5c8`
  is the same kind of stub (`FF 25`) and its target also lands inside `D2RCore.dll`. So type 3
  generation really is D2RCore's, and the sibling stub is confirmed.
- **Level reseed:** the first `E8` call in `0x3271c0` (offset `+0x30`) is the reseed, `0x367110`.
- **Act 1 builder count:** `0x3fac10` is still called by the game. Starting a game and spawning at the Rogue Encampment
  built Blood Moor (level 2) and the next level (3) through it: one call each, on the server and again on the client
  (four calls in all). A later `cheat goto 2` made no new call because those levels already existed. D2RCore therefore
  does not replace D2R's per-act builders for these levels (who called `0x3fac10` was not logged, so this does not prove D2RCore never calls it); only `0x3271c0`'s type 3 branch goes through D2RCore.

### S1

- **Slot id:** a new row in the test slot's `levels.txt`, vanilla RotW next free id 138 (the pd2 spike's 202 was that
  mod's). Cells cloned from row 111 (Frigid Highlands), then Act 4 (Act 5), Layer 79, size 160x32, OffsetX 6000,
  OffsetY 3000, DrlgType 3, LevelType 21 (jungle), Sub* -1, Waypoint 255, Vis/Warp cleared. The level shows as
  "Frigid Highlands" with that row's monsters: the name and monster columns were not changed.
- **Build path:** the spike hooks `0x3271c0`, writes the level rect, calls `0x3ee320`, and its `0x3fe2e0` hook stamps the
  jungle line instead of D2R's Act 5 builder, then runs the native tail with the drlg type temporarily 0 so
  D2RCore's generator is skipped. D2RCore's act lookup for id 138 said 4, so `0x3ee320` takes the Act 5 branch. The
  level rect on entry to `0x3271c0` was (0,0 0x0): the hook has to write it.
- **Stamp:** piece id = `0x211 + mask` with W=1, E=2, S=4, N=8, LvlPrest rows 530 to 544; line `E,EW,EW,EW,W` is
  five 32-tile blocks, each 4x4 grid cells of 8 tiles (a 20x4 cell grid over 160x32 tiles). `0x3ecfc0` takes the file from the level RNG at
  `+0x1e4` (file 0 each time here).
- **Jungle tiles render in Act 5:** yes. Jungle ground, palms, ferns and a fallen-tree bridge render on the level (in game).
- **Rooms:** 80 rooms (8x8 tiles each, a 160x32 tile rect) on both the server and the client drlg. Piece/fill split (live, from LogRooms): 80 piece rooms and 0 fill rooms on both sides, 16 of lvlprest 530 (Jungle W, `rivw`), 16 of 531 (Jungle E, `rive`) and 48 of 532 (Jungle EW, `rivew`), file 0 each time. Every cell of the 20x4 grid is covered by a piece, so the line has no fill rooms to account for in the room bits at `+0x258` or in S2. The room lists and grid dumps of every build
  (server, client, client again after leaving and returning, and server and client in a second game) are identical.
- **Monsters:** spawn on the level (Frigid Highlands monsters, from the cloned levels.txt row; they attack).
- **Leave and return:** `cheat goto 1` then back to 138: the client rebuilt the level and the room list and grid dump
  matched the first build exactly. The server kept its level.
- **New game:** leave the game, start another, `cheat goto 138`: the level built again with the same rooms and grid,
  no crash. No crash dump was written and the spike's log shows no fault, in either game.
- **Position readout (live):** `cartographer pos` and `track` read the player's dynamic path (unit `+0x38`, x `+0x2`, y `+0x6`,
  subtiles; control unit `0x484c90`, byte-checked at load). Level origin is subtile (30000,15000). Validated live.
- **Walking (live, route-guided; one run as operated, continuity not logged):** the earlier two-segment walk (x 148 to 89 and x 11 to 70, all four seams crossed) is superseded. In the finishing run the character went from tile 7 (`cheat goto 138,30037,15066`) to the east end in what was operated as a single click-walk, driven by the logged position and a route computed from the own-room collision scan (below). It crossed all four seams (tiles 32, 64, 96, 128) and the former stall zone at x 70..89 with no stall in the routed phase, reaching subtile x 714 (tile 142.8); the game's own pathing then took it blind east to a dead end at subtile (751,64), tile 150.2 (`track off`: reached x 30034..30751, y 15032..15066). Tiles 142.8 to 150.2 were the game's own pathing, not the route; the east tail is almost certainly the closed outer edge of the last block, not proven. Tiles 0 to 5 and 151 to 159 were not walked; tile 6 was only touched (min x 30034 is in tile 6). The run is route-guided, not free exploration: the route was planned from the collision scan, then followed by feedback clicks. Not recorded: whether `track reset` was run before the counted walk (the extents also cover teleports, such as the `goto` sweeps and the abandoned attempt, so the extents alone do not show one run), and whether the `pos subtile` log runs without jumps from 30037 to 30751; the spike code does not show either. The offline merge and route planning tooling is not recorded and is not in the repo or on the spike branch (`coll <mask> dump` overwrites one fixed file per mask, so keeping each room's window and merging 62 rooms happened outside the spike). The monsters' columns (64 to 72 and 75) were zeroed in the slot's `levels.txt` for the walks (restored afterwards); the first stall of the first run was a locked chest object, and one stall near tile 23.8 in the first attempt was a dead end of an over-eroded route, not terrain. Whether the counted run was a fresh start at tile 7 after that attempt was abandoned is not recorded in the logs; the `cheat goto 138,30037,15066` start suggests it, but that is an inference.
- **Collision (live, own-room only):** `0x350a20` (mask `0x1c09`) is reliable only inside the player's own room (it walks that room's near list; elsewhere it reports blocked, about 98 percent of the level). The finishing run teleported the player into each room (`cheat goto`) and kept the own-room window: 62 of 80 rooms were sampled. Missing: rooms (0,0), (0,3), (3,0), (4,0), (4,3), (5,0), (6,0), (7,0), (8,3), (11,0), (12,3), (15,0), (16,0), (17,0) and the whole column 19 (non-walkable targets snap to the entry, so those rooms could not be entered by `goto`). The own-room merge is not a bound either way: the first run's merge over-reported open ground (it showed open tiles in the dark channel the character could not enter), and the finishing run's data does not show that the over-report is gone, so the caveat stands; unsampled rooms count as blocked. From it the walk route was found, and the route was then confirmed by the character actually standing along it (logged positions), which is the evidence, not the map alone.
- **Automap (live, compared by eye):** the automap draws a continuous blue dotted line along the whole line (both ends and
  across the seams seen) with tree rows flanking the corridor, matching `[E][EW][EW][EW][W]` running east-west. It was
  compared by screenshots (`automap_east`, `automap_west`) at two positions, not cell by cell and not against a mask dump.
  Verdict: consistent with the masks; not an exhaustive check.
- **River or path (decided: both exist, the line is traversable):** the dark channel at x 70..90 looks like water in the screenshots (a river, or a lake where the line widens) and is impassable (the live evidence is the stalls and the route around the channel; why it is impassable is not known): a straight click walk into it stalled, as the earlier runs did. It does not cut the line: the banks and a north shelf are walkable ground. The live route kept to the north bank (rows about 9 to 12), detoured up to rows about 6 to 8 around the x 70..90 water body, then ran along rows about 8 to 11 to x 142 and crossed every seam on foot. So the stamped line is a navigable path with a river beside and through it, not a river-only line. Evidence: live in game (position log, own-room collision, the click walk). Offline DS1 walk flags (catalogue tool, `lay.unwalkable` = cell flag `0x20000`, subtile flag = DT1 bit 0) were ambiguous: honouring the flag cut the line near x 22, ignoring it left everything open, so they are not used as evidence (offline, not live). Not covered: the S2 questions (an L-shape plus a branch, a closed edge next to an occupied cell).
- **Earlier S1 conditions not redone in the finishing run:** render, automap, monsters, host and client logs, leave and return, and new game are from the first run (above). The finishing run did not repeat them, so issue #22 is `Refs`, not `Closes`.
- **Slot used:** 11 (`D2RToolsTest`). Game stopped, slot unlocked, the spike DLL removed, and the slot's plugins, config, logs,
  `levels.txt` and save folder restored from a backup taken before the runs.

See also section 8 for the static answers to the open outdoor builder questions (issue #83).

## 5. Maze slot engine checks (issue #62)

Answers the four static questions of plan section 3.6 [V] for the guided maze slot. Everything here is read from the build 93847 dump (Capstone
disassembly; Ghidra was only a reading aid); nothing was run in a game. Where a conclusion needs the game, the check is given as "needs game: set X, observe Y".

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer guided maze hook (plan section 5, phase 3b); hooked in `plugins/cartographer/src/engine/maze_hooks.cpp` (issue #78) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the same memory-layout dump as section 1a (`D2RLoader_6ab3782c_dump.exe`) |
| Location | RVAs in the table below. Offsets are the DRLG level record (one per level per act; list head at `drlg+0x868`, next at level `+0x1b8`): `+0x00` DRLG type (1 maze, 2 preset, 3 outdoor), `+0x08` room count, `+0x10` first room, `+0x18` LvlMaze row, `+0x24/+0x28` origin x/y (tiles), `+0x2c/+0x30` width/height (tiles), `+0x1f8` level id. Room: `+0x48` next, `+0x50` flags, `+0x60/+0x64` x/y, `+0x68/+0x6c` w/h |
| Guard | The section 1a rows keep their signatures. The functions in the table below are unsigned, read-only evidence, except `0x3271c0`, which is already signed in sections 1 and 4 (its row is listed for the masked prefix only). Section 1a repeats the same function's first 24 bytes as the maze hooks' own signature (`sig::LevelBuild`): it is the same prefix as section 4's, not a different one, kept so the maze parts check themselves without the portal parts. The others were followed from signed functions or from the level build `0x3271c0`. The masked prefix shown for each matched exactly once across `.text`, `.maho`, `.koishi` and `.btext` of the dump (relative displacements and RIP-relative operands masked), so it can become a signature later; none has been checked in a running game. A hook that needs one of them records its signature and max patch length first |
| Contract | See the answers below |
| Evidence | Disassembly (Capstone) of the functions named, over the dump (the Ghidra decompile was only a reading aid and is not needed). Reproduce a listing with `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]`, and a prefix match with `... pat "<hex with ??>"` (read-only; refuses a different stamp; needs `capstone`). Example: `dis 0x32751e 3` shows `mov rdx, rbx / movzx ecx, r12b / call 0x3271c0` |
| Validation | Static, then the guided maze hooks were run in game on build 93847 on 2026-11-12 ("Game checks of 2026-11-12" below; probe lines `[maze probe]`, 46 consecutive clean games in the first run, then 9 clean built games in a second run on another table state, 55 in all; see "Games toward the 50-games check"). Not checked on Steam build 94995 |
| Failure mode | Fail open: a hook built on this leaves the maze off if a signature or guard does not match, as in section 1a |
| Coexistence | No new patch is proposed on this page |

| RVA | Role found | Unique masked prefix (offset 0) |
|---|---|---|
| `0x3267c0` | Level getter and allocator for (drlg, level id) | `48 89 6C 24 10 48 89 74 24 18 57 48 81 EC 80 02 00 00 48 8B 82 68 08 00 00` |
| `0x327650` | Writes the level rect, once, at allocation | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B EA 49 8B D8` |
| `0x3e7760` | Maze level init (stores the LvlMaze row at `+0x18`, then calls `0x327650`) | `48 89 5C 24 08 57 48 83 EC 20 48 8B DA 0F B6 F9 8B 92 F8 01 00 00 E8 ?? ?? ?? ??` |
| `0x3271c0` | Level build (reseed, then dispatch to maze, preset or outdoor) | `48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2` |
| `0x3e69e0` | Maze builder (the Cave/Crypt case calls `0x3eb850`) | `40 55 53 56 57 41 55 41 56 48 8B EC 48 83 EC 28 48 8B DA 44 0F B6 E9 48 8B CB` |
| `0x327880` | After-build normaliser (checks the span, then moves the rooms so their bounding box starts at the origin) | `48 89 4C 24 08 55 53 48 8B EC 48 83 EC 58 48 8B D9 48 85 C9 75 ?? 88 4D 20` |
| `0x326580` | Bounding box of a level's rooms | `48 89 5C 24 08 48 8B 41 10 4D 8B D0 48 8B 5C 24 28 8B 48 60 89 0A 8B 48 64` |
| `0x327400` | World-space room lookup (x, y) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B BC 24 80 00 00 00 4D 8B E9` |
| `0x3605b0` | Point in rect (x in [r0, r0+r2), y in [r1, r1+r3)) | `44 8B 09 41 3B D1 7C ?? 44 8B 51 04 45 3B C2 7C ?? 44 03 49 08 41 3B D1 7D ??` |
| `0x3da650` | Level-entry finder (arrival room) | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B FA` |
| `0x3dad90` | Arrival helper: first room with `(room+0x50 & 0x30000)` (flag test in `0x3612d0`) | `48 89 6C 24 18 57 41 56 41 57 48 83 EC 20 48 8B 7A 10 4D 8B F1 4D 8B F8` |
| `0x3275b0` | Arrival helper: room containing a point | `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B F8 8B F2 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 83 7B 10 00` |

### Q1. Largest level size the engine accepts

- **Static answer:** no cap below int32 exists on the maze path. The size is two ints copied from the LevelDefs row by `0x327650` into `+0x2c` (SizeX, difficulty column at `+0xc + diff*4`) and `+0x30` (SizeY, `+0x18 + diff*4`), where `diff` is `drlg+0x830`. Room records (`0x1c0` bytes, general allocator `0x361140`) and level records (a pool) are not sized by the level size, and the per-room arrays (the `+0x258` int array, allocated as `roomCount*4` in the teardown `0x327c40`) are sized at run time.
- **The only DRLG checks on the size:** (1) the add-room span check, `0x3e77a0` -> `0x360520` (signed, section 1a) -> bounding box `0x326580`: a candidate room is rejected when the merged bounding box of all rooms is wider than `+0x2c` or taller than `+0x30` (signed int compare of the span, not of the position; skipped for level ids `0x6b` and `0x1c`); (2) the assert in `0x327880` when the final span exceeds `+0x2c/+0x30`; (3) the world lookup (Q2).
- **Hard bound:** dynamic unit positions are u16 subtiles (5 per tile, section 4 S1), so origin plus size must stay at or below 13107 tiles.
- **The rule is the span, not a centred first room.** `0x3e69e0` puts the first room at `(+0x2c - roomW)/2 + +0x24`, but `0x327880` then moves all rooms so the bounding box starts at the origin. The test is span <= size per axis, so a layout with the first room near one end fits as well. The symmetric "k rooms each side" model in `plugins/cartographer/src/maze/maze.h` (`Capacity`) is stricter than the engine; that is safe. A plan could in principle size the level to its span (needs game, below).
- **Room count:** nothing caps it by level size. A Crypt (8 tile rooms) at 360 x 360 is up to 45 x 45 = 2025 rooms; the overlap checks are O(n^2) in rooms, which is probably acceptable for the DRLG pass (inference from the complexity, not measured; needs game together with the `+0x258` arrays below).
- **Evidence level:** static for the DRLG side. **Needs game** for the rest of the game accepting 360 (client room load, collision, automap, the `+0x258` arrays at 900 rooms or more). The only live size evidence is the S1 outdoor spike at 160 x 32 (section 4).
- **Needs game, size:** slot row SizeX = SizeY = 360 in all three difficulty columns, Depend 0, Position 0, origin from Q2. Log level `+0x24..+0x30` at entry and exit of the `0x3271c0` hook; after the build log the room count and the min and max of room `+0x60/+0x64` (plus `+0x68/+0x6c`). Observe: no assert and no crash_guard entry, the bounding box starts at the origin and spans at most 360. Then walk to each end and open the automap.
- **Needs game, lopsided layout:** force the first room to one end of a 15-room span and observe that `0x360520` does not reject and the build completes.

### Q2. Level origin

- **How it is set (static):** at allocation only, `0x3267c0` -> `0x3e7760` -> `0x327650`: `+0x24` = LevelDefs OffsetX (`+0x24`) plus the origin x of the Depend level (LevelDefs `+0x2c`, fetched through `0x3267c0`; Depend 0 means absolute), and the same for y with OffsetY (`+0x28`). The teardown `0x327c40` clears `+0x34..+0x1b3` only, so the rect `+0x24..+0x30` survives a rebuild, and a write made in a `0x3271c0` hook persists.
- **What must be clear:** the declared rect, not the room bounding box. `0x327400` (world lookup) tries the hint room and its neighbours, then walks all levels of the act from the list head at `drlg+0x868` and takes the first level whose declared rect contains the point (`0x3605b0`). A declared rect that overlaps another level's rect in the same act can resolve rooms to the wrong level.
- **Vanilla table (levels.txt of build 93847 in a test slot; numbers only):** maze levels are 200 x 200, stacked 300 apart in y, columns 500 apart in x. Non-outdoor, non-Depend envelopes per act: Act 1 x 0..5043, y 0..2896; Act 2 x 1000..5200, y 1000..3147; Act 3 x 1000..3700, y 1000..2380; Act 4 x 1000..1032, y 1000..1024 (Diablo, id 108, at 1500,1000 with 120 x 120); Act 5 x 1000..5043, y 1000..3084. A 360 rect cannot reuse a vanilla maze column (300 y-spacing is smaller than 360); it fits in a free column.
- **Recommendation:** Depend 0, OffsetX past the act's envelope plus a margin (6000 for any act, as the S1 spike used in Act 5), any OffsetY, origin + 360 well under 13107. Outdoor rows (DrlgType 3) have Offset -1 or arbitrary values and their rects are set by act start-up (`0x3f0130` -> `0x3f1c60`), so their positions are not known statically.
- **Lazy build in the lookup (static):** when the level that matched has no rooms yet (`+0x10 == 0`), `0x327400` itself calls `0x3271c0` (`0x32751e..0x327525`, via `mov rdx, rbx / movzx ecx, r12b / call`); if no rect matches it falls through to `0x3267c0(drlg, 0)` and builds that. So the slot's `0x3271c0` hook can run from a world lookup that matched on the allocation-time rect. Consequence: the hook must not move the origin away from the row's rect (write the size, or an origin equal to the row's), or the point that triggered the build may fall outside the level it just built. The row therefore carries the maximum size (Q4).
- **Evidence level:** static for the mechanism and the table envelopes; the clear origin is an inference from the table plus the S1 precedent (6000/3000, size 160 x 32, worked live in Act 5). **Needs game:** with the new row in a test slot, log the rect (`+0x24..+0x30`) and level id of every level on the `drlg+0x868` list for the act, and the result of `0x327400` at the four corners of the slot's rect. Observe the slot's own id for each corner and that no other level's rect intersects.

### Q3. `0x3eb850` out parameter and what the slot's later code expects

- **The out parameter (static):** `0x3eb850(level, int entry[4] = {from, to, file, dir}, uint* rot)` returns void. `rot` is the maze builder's rotation counter, not a result: it may be NULL, and on every path, including "nothing placed", the epilogue stores `(*rot + 1) mod 4` (`0x3eb936..0x3eb94e`: NULL test, `inc`, `and 0x80000003`, sign fix, store). The Cave case in `0x3e69e0` uses that counter as the table index base: it calls `0x3eb850(level, table + rot*16, &rot)` for the entrance group, then `table + (rot+8)*16` (`+4` for level 8) for the Down/Next group; level 9 adds a third call (`+0xc`) and level 10 a last one (`+0x10`). A slot level id (not 8, 9 or 10) gets two calls.
- **Consequence for the hook:** when it suppresses a call it must leave `*rot` in 0..3. Either leave it unchanged or store `(v + 1) & 3` as the original does. Any other value makes the next table index run outside the entry table. Nothing reads a return value.
- **Later code and an exit room (static):** no code on the arrival or link path asserts on a missing exit. The level-entry finder `0x3da650` takes the first room with `(room+0x50 & 0x30000)` (`0x3dad90`), then rooms with warp bits (`0xff0`) against the Vis entries, then the room at the declared centre (`0x3275b0` at `+0x24 + w/2 - 2, +0x28 + h/2 - 2`), then a seeded random room of the level, and returns 0 only if the level has no rooms. The tail of `0x3271c0` records warp-room centres into level `+0x82..` (at most 9, only for flagged rooms). A warp tile comes from room data, so with no Down piece the slot simply has no warp tile. This was read for the paths named; it is not an exhaustive scan of every consumer of the level links.
- **Evidence level:** static. **Needs game:** a Cave slot id with the second (+8 group) call suppressed and the first (Prev) call allowed. Observe: two `0x3eb850` calls in the log with the second answered by the hook, `rot` after each in 0..3, the build completes, the character arrives in the Prev room (the portal room) and walks, and no crash_guard entry appears.
- **Note:** `Position` of the slot row must be 0. A non-zero Position makes the level pick an entry from the maze's preset list (the d2rAI-toolkit `engine-limits.md` records a crash when there is none).

### Q4. Where to clamp and write

- **Clamp (plan side):** `maxLevelTiles` is a plan limit, so clamp it in the core before any plan is made, to the smaller of 360 and the size the game checks of Q1 confirm. The engine does not clamp it.
- **Write (static):** in the `0x3271c0` build hook, before calling the original, write the size `+0x2c/+0x30` of the slot's level; leave the origin `+0x24/+0x28` at the row's values (the world lookup can build the level from the allocation-time rect, Q2). The first room is positioned from `+0x2c/+0x30` in `0x3e69e0`, which runs after the hook, and the values survive a teardown. This matches the pd2-resurrected note "write per-level fields in the 0x3271c0 hook before the original runs".
- **Also set the table row:** the declared rect is what the world lookup uses (Q2) and `0x327650` runs once, so give the slot row the maximum size in all three difficulty columns (and the chosen offsets) so the allocation-time rect already fits any plan; the hook never needs to move the origin. A rect that grows after allocation could newly overlap another level between lookups (needs game, Q2).
- **Evidence level:** static. **Needs game:** the Q1 logs, plus the first room's x/y equal to `(size - roomW)/2 + origin` before `0x327880` and the final bounding box at the origin.

### Status after issue #78

The maze steering hooks now depend on these answers; checked in game on 2026-11-12 on a Cave slot at Normal difficulty (next subsection) (the code logs the reason and falls back to
the game's own maze when a step fails): the size written at `+0x2c/+0x30` in the `0x3271c0` hook before the original (Q4), the
slot row's declared rect as the lookup rect (Q2), the suppressed second `0x3eb850` call and the rotation counter kept in 0..3
(Q3), a private LvlMaze row (never freed) behind `+0x18`, and 360 x 360 accepted (Q1). The declared size is recorded once per game, per level id, before the first write; every plan is limited to that record (the hook overwrites the level's `+0x2c/+0x30` with the plan's size, which survives a teardown). On a fallback, and on any later unsteered build, the declared size and the level's own row pointer are put back first, so the game's walk runs against the level it was written for (after a fallback mid-walk it finishes from the plan's rooms already built). Checked in game on 2026-11-12 (Cave slot, Normal; next subsection): whether `0x3271c0` runs
before the plan exists and whether the piece struct is per room (fixed-flag writes). Shown only indirectly: that the add-room
checks (`0x360c80`) reject adjacent but unlinked plan cells (the built grids equal the planned ones, but a rejection is not logged)
and `clearFixed=1` in `0x3eb960` for every room (no room has the Fixed flag afterwards; the argument was not observed). `MazeProbe=1` logs the data to check.

### Game checks of 2026-11-12 (issues #78 and #62)

Run in a test slot (D2R build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta, test mod and test save, Normal difficulty,
Barbarian; the character was often killed on arrival by Dark Rangers, which does not matter for the build). The mod's tables were brought to a clean Cartographer state first: vanilla `levels.txt`, `lvlprest.txt`, `lvlmaze.txt`
and `misc.txt` (five map item rows), then `tools\install.ps1`, then one Cave level added by hand (level 139, DrlgType 1, LevelType 3,
Act 5, 360 x 360 in all three columns, Depend 0, Position 0, OffsetX 6000, OffsetY 0, `lvlmaze.txt` row `Cartographer Maze 139 4 4 4 24 24 500`), `slots.txt` `Maze 139`, `MazeProbe 1`. `maps.txt` had `CaveCorridor` (Small), `CaveWinding` (Medium), `CaveLoop`
(Large) and `CryptBranching` (Medium, the wrong tileset on purpose). Levels: **live** = seen in the running game through the
`[maze probe]` log lines; **static** = read only; **not done**.

Probe facts the numbers rely on: one plan cell is `roomTiles` (Cave 24) tiles and becomes 3 x 3 engine rooms of 8 x 8, so a plan of
N cells gives 9 N engine rooms. The new probe lines print the built and the planned room grid one character per plan cell, the
engine's room sizes, and how many piece structs the rooms have.

| # | Check | Level | Result |
|---|---|---|---|
| 1 | Probe: slot rect and levels, room count and box, `rot` after each `0x3eb850` call, map in the planned shape | live | Slot rect `139:(6000,0)+(360,360)` clear of the other seven levels of the act. Built room grid equals the planned grid for `CaveCorridor` (5 x 5 box, 9 cells), `CaveWinding` (9 x 7, 26 cells) and `CaveLoop` (10 x 6, 28 cells): 81, 234 and 252 rooms, all 8 x 8, none outside the grid. `rot` after every call stayed in 0..3 (`3 -> 3`, `3 -> 0`; `2 -> 3`, `3 -> 0`; `3 -> 1`, `1 -> 2`). The character stood on the map level and walked (a Cave map). **Not usable: arrival versus entrance room.** The arrival probe read a room at (0,0) of size 0 x 0 (`UnitRoom` gives the server room, not the DRLG room), and the entrance was counted once per sub-room (9 entrance rooms for one entrance cell), so the "entrance known" test (exactly one) never held. Probe limits, not engine findings. Fixed in the probe after the run (see "Arrival room"; built, not yet checked in game) |
| 2 | Does `0x3271c0` run for the slot before a map job exists, is the slot left usable | live | Not on its own: with a map opened the first call for the slot comes at the open (rooms 0 before it), and again on entering after the teardown with the same plan and seed. With no map opened (`cheat goto 139,...` straight from town) the hook ran for the slot, logged "building the game's own maze: no maze map was opened in this game", and the character stood in a normal Cave level. So a lazy build without a job is possible and is handled |
| 3 | Add-room checks (`0x360c80`) reject adjacent but unlinked cells | live, partly | Every built grid equals the planned grid cell for cell, no room was added or moved (252 = 28 x 9 for the Loop, 234 = 26 x 9, 81 = 9 x 9), for shapes whose planned cells touch without a link (the Loop and the Winding). The probe does not print piece masks, so a rejected candidate is not logged on its own; this is the evidence that no unlinked neighbour was added |
| 4 | Piece struct per room; `clearFixed` | live | Per room: 81 of 81, 234 of 234 and 252 of 252 rooms have their own piece struct (no two rooms share one). After the build no room has the Fixed flag set (0 of 252). `clearFixed=1` was not observed as a call argument; the 0 Fixed rooms are the evidence it holds |
| 5 | The suppressed Down call builds cleanly | live | Yes, in every game: the second `0x3eb850` call is answered by the hook, the build ends "built as planned (N rooms, ...)" with no crash_guard entry and no new crash file (the crash-report folder stayed at the 6 files it had before the run) |
| 6 | Level `+0x2c/+0x30` and private row writes accepted; 360 x 360 accepted | live | Yes. 360 x 360 in all three columns is accepted at load and allocation; the hook wrote the plan size (for example 216 x 216, 264 x 168), the level rect then read back as the plan size and the walk finished; the private row behind `+0x18` was accepted |
| 7 | A mismatched tileset, then reopen in the same game | live | `CryptBranching` on the Cave slot: the load gives a warning (the map is listed with a problem); `cartographer open CryptBranching` refuses with "the Maze slot level is a Cave level (LevelType 3), so it cannot build the Crypt map" and no portal; repeating the open is refused the same way; after a Cave map had been built, opening another map in the same game is refused ("already built ... Leave the game and start a new one") |
| 8 | Forced plan failure falls back to the game's maze | live | Slot rect set to 60 x 60 (all three columns), `CaveLoop` opened: log "building the game's own maze: Cave levels hold at most 1 by 1 rooms, which is too small for this shape. Use a smaller size or a simpler shape."; the portal opened, the character entered a Cave level, no crash, no new crash file |
| 9 | Leave the game, new game: the maze map opens again | live | Yes, at every one of the games below: Save and Exit, new game, a map opened and built with the new seed and plan (also a different map each time, because the previous plan does not carry over) |
| 10 | The 50-games check | live, 46 of 50 games run | 46 consecutive games in one session, one per game: Save and Exit (or die and Continue), start a new game, `cartographer open <Name>` cycling `CaveCorridor`, `CaveWinding`, `CaveLoop` (16, 15 and 15 games), enter the portal. Every game: "built as planned", the built grid equal to the planned grid, no crash-report file, no error line in `cartographer.log`. A few earlier games with the same checks, from a first version of the script that failed to leave the game every other time, are not counted. Each game was judged from the log (`built as planned`, grids equal, crash folder), not from a screenshot |

Not covered by these runs: a Crypt slot (the slot was Cave; the Crypt refusal was exercised only as the tileset mismatch), Hell or
Nightmare difficulty, a TCP/IP client, the slot with an engine-side Fixed flag (none was set), and the arrival room (see 1).
Frames per second and load time were not measured.

### Arrival room (issue #99 item 1): built, not yet checked in game

The arrival probe (`MazeProbeArrival`, `MazeProbe=1`) now reads the player's DRLG room through the server room, and the entrance is counted once per plan cell. Static reading of the dump (build 93847, `dump_dis.py ... dis <rva>`):

| Step | Offset | Evidence |
|---|---|---|
| unit to path | unit `+0x38` | `0x34b440` (`UnitRoom`): `mov rcx, [rbx+0x38]` at `0x34b472`, for unit types other than 2, 4 and 5 (those read `[[unit+0x38]]` at `0x34b48d`/`0x34b491`) |
| path to server room | path `+0x20` | `0x34b480` jumps to `0x341c30`: `mov rax, [rcx+0x20]` / `ret`. This is what `UnitRoom` returns, and what the probe received before |
| server room to DRLG room | server room `+0x18` | `0x2efc10` (`RoomLevelId`): `mov rcx, [rcx+0x18]` at `0x2efc18`, then `jmp 0x360fc0`. Also recorded in sections 6 and 9. In the #97 run `RoomLevelId(room) == slot` held for the player's room (that check let the arrival line print), and the allocator `0x361140` writes the level at `+0x90` (at `0x361190`) |
| DRLG room to level | DRLG room `+0x90`, level `+0x1f8` | `0x360fc0`: `mov rax, [rcx+0x90]` / `mov eax, [rax+0x1f8]`. So the pointer at server `+0x18` is the DRLG room, and the DRLG room has the `+0x60/+0x64/+0x68/+0x6c` rect of the level's room list (section 5 header) |

So the old read took `+0x60..` of the server room, which is not a tile rect (hence 0,0 and 0 x 0). The probe reads `+0x18` of the room `UnitRoom` returned, then `+0x60..+0x6c` of that, every pointer through `Read`/`Readable` (a wrong layout logs zeros, not a fault), and only while `MazeProbe=1`. The arrival is placed in a plan cell as `(pos - box min) / roomTiles`, the box and cell size being those of the post-build room walk; the entrance is the one cell holding Prev-piece rooms (all nine of its sub-rooms carry it). The log line says `ARRIVAL IS IN THE ENTRANCE CELL` or `arrival is NOT in the entrance cell`, and only when exactly one entrance cell was found. **Needs game:** one maze game with `MazeProbe=1`: `entrance: 1 cell(s) (9 engine rooms ...)` after the build, an arrival line with a non-zero size 8 x 8 and the entrance verdict.

### Left for the game

See also section 8 (Q6 and Q10) for the portal arrival and the outdoor size. Items 1 to 3 were settled in game on 2026-11-12 (above); the arrival room of item 3 was not (probe limit, check 1; probe rewritten, see "Arrival room").

1. The whole game accepting a 360 x 360 maze level (Q1, size and lopsided checks).
2. The origin being clear of every other level's rect, including outdoor levels (Q2 log).
3. A slot id built with its second `0x3eb850` call suppressed: clean build, arrival and walk (Q3).
4. Optional: sizing the level to the layout span instead of the centred capacity (Q1 lopsided check).

### Game checks of 2026-11-12 (second run, issue #99)

Run in a test slot (D2R build 93847, D2RLoader 1.3.1-beta, test mod and test save, Normal difficulty, `MazeProbe 1`), after the arrival probe was rewritten (see "Arrival room" above). The mod's tables were the vanilla `levels.txt`, `lvlprest.txt`, `lvlmaze.txt`, `itemstatcost.txt` and `misc.txt`, changed by the run's own staging scripts (`itemstatcost.txt` was later moved out of the mod; see the WARN note below). Nothing in the run's files shows that `plugins\cartographer\tools\install.ps1` was run for this run; that script writes only the Preset rows (`levels.txt`, `lvlprest.txt` and the `Preset` row of `slots.txt`), so the Preset level was not part of this evidence. The staging scripts added: the level 139 row in `levels.txt` (cloned from a vanilla Cave or Crypt level; DrlgType 1, LevelType 3 for Cave or 4 for Crypt, 360 x 360 in all three columns, Depend 0, Position 0, OffsetX 6000, OffsetY 0, `Warp0..7` -1), the `lvlmaze.txt` row `Cartographer Maze 139 4 4 4 <size> <size> 500` (cell size 24 for Cave, 8 for Crypt), the map items `cm0` to `cm8` in `misc.txt` (cloned from `isc`, `pSpell` 16), and `slots.txt` `Maze 139`. Per game: a new game, `cartographer open <Name>`, enter the portal, read the `[maze probe]` lines. **live** = seen in the running game.

| # | Check | Level | Result |
|---|---|---|---|
| 1 | Cave maps built, planned grid equal, arrival against the entrance cell | live | 6 Cave games (Corridor 2, Winding 2, Loop 2): all "built as planned", built grid equal to the plan, 0 crash files. Arrival: see the next table |
| 2 | Crypt maps built | live | `CryptBranching` (26 rooms), `CryptCorridor` (10), `CryptLoop` (31): all "built as planned", grid equal to the plan, 0 crashes |
| 3 | Reverse tileset check | live | `CaveWinding` on the Crypt slot: refused when opened ("the Maze slot level is a Crypt level (LevelType 4), so it cannot build the Cave map ... needs LevelType 3"), no portal, no crash |
| 4 | Nightmare and Hell | not run | The test save is locked to Normal; the save was not edited |

Arrival against the entrance cell (`[maze probe] arrival` lines: the player's DRLG room on the first tick in the level, by plan cell, against the cell of the engine room with the Prev piece):

| Game | Arrival (plan cell) | Entrance cell | Verdict |
|---|---|---|---|
| Cave Corridor | (6048,72) | (6048,0) | not in the entrance |
| Cave Corridor (second game) | no arrival line within the capture window (about 43 s) | (6120,48) | no verdict |
| Cave Winding | (6120,72) | (6192,144) | not in the entrance |
| Cave Winding | (6048,48) | (6000,192) | not in the entrance |
| Crypt Branching | (6024,40) | (6048,0) | not in the entrance |
| Crypt Corridor | (6016,24) | (6032,24) | not in the entrance |
| Cave Loop (2), Crypt Loop (1) | (6096,96), (6096,72), (6040,40) | unknown | no verdict: 0 entrance cells ("entrance an ordinary room"), the probe prints "the entrance cell is unknown" |

In all 5 games with an arrival line and a known entrance the player arrived in another room. Suspected cause (read statically afterwards, see "Arrival room: why the player arrives elsewhere"): the level-entry finder `0x3da650` (section 5 Q3: first room with flag `0x30000`, then the warp rooms, then the centre, then a seeded random room) does not pick the plugin's cell. The Loop maps have no entrance cell by design, so there is nothing to compare. Tracked in a separate issue; no code was changed in this run. The second Cave Corridor game placed an entrance cell at (6120,48) but logged no arrival line within the capture window, so it is not a sample.

Crashes met while setting the slot up: 5 crash reports, all at image `+0x66259d` (13:32 to 13:43 UTC), while the hand-made level 139 row had `Warp0..7` = 0. After that row's `Warp0..7` was changed from 0 to -1 the crashes stopped: the 10 games that followed had no crash file. The cause was not traced (the earlier row was not kept, so other differences cannot be ruled out).

Games toward the 50-games check: this run, 9 clean built games (6 Cave, 3 Crypt) and 1 clean refusal game, none with a crash file; with the 46 Cave games of the first run that is 55 clean built games. In this run "clean" means built as planned, grid equal to the plan and no crash file. Every game also logged WARN lines saying the map stats were skipped for monsters, because `itemstatcost.txt` was not present in the test mod; so map stats on monsters were not exercised here (the first run counted "no crash, no error line").

#99 item 3 (50 games in a row, or a recorded reason): settled as accepted. 55 clean built games over two sessions with different table states, not consecutive, are accepted as meeting the 50-games check. Caveat: they are not one unbroken series, and the second session's 9 built games ran without `itemstatcost.txt`.

Not run: Nightmare and Hell, a TCP/IP client, a second map in the same game on the Crypt slot, a slot with an engine-side Fixed flag.

### Arrival room: why the player arrives elsewhere (issue #127, sub-issue of #99)

Static reading of the dump (build 93847, stamp `0x6ab3782c`, `dump_dis.py ... dis <rva>` and a Capstone sweep over `.text`). **Evidence level: static for the finder's branches, the flag, who sets the flag and that the maze pieces never do; inference for which branch fired in the five games and for the chain from the portal to `0x2ef8b0`. Not run in game.**

| Step | Address / offset | Evidence |
|---|---|---|
| Callers | `0x2ef8b0` calls `0x3da650`, then `0x334ed0` (room centre to subtiles) and `0x362f90` (free spot, mask `0x1c09`) | static (section 8 Q6); that a red portal reaches `0x2ef8b0` is still an inference. A second caller is the wrapper `0x2efe80` (`mov rdx,[rdx+0x70]; jmp 0x3da650`), called from `0x509f27` (after `movzx ecx, byte [rax+0x106]`) and `0x593546`; with `Position` 0 they take the same branch, so they would also pick the marked room (inference, their callers were not traced) |
| Finder, when the levels.txt row's `Position` (row `+0x90`) is 0 | `0x3da650` tries, in order: (1) the first room of the level whose flags hold `0x30000`; (2) rooms with a warp bit (`0xff0`) whose Warp[k] is not -1; (3) the room at the declared centre (`0x3275b0`, `+0x24 + w/2 - 2`, `+0x28 + h/2 - 2`); (4) a seeded random room | static (section 5 Q3 read the same function; the helper below is new) |
| First room with the flag | `0x3dad90(level, ..)` walks the level's room list from level `+0x10` by room `+0x48` and tests each with `0x3612d0` (`mov eax,[rcx+0x50]; and eax,0x30000; ret`) | static: bytes of both functions and the loop `call; test eax,eax; jnz; mov rdi,[rdi+0x48]` |
| Who sets `0x30000` | the DS1 loader `0x3e0b20` (`mov r9d,0x30000` at `0x3e1214`), for preset objects whose objects.txt row has bit `0x40` at `+0x127` | static |
| Other readers of the flag | (a) `LevelBuild` `0x3271c0` tests `[room+0x50] & 0x30000` at `0x3272a0` and `& 0xff0` (warp) at `0x3272ab`; for each such room it stores the room centre at level `+0x208[n]` / `+0x22c[n]`, calls `0x334ed0` and increments the count at level `+0x250`. It runs inside the original build, before `MarkArrival`, so the marked room is not in that list; for the slot the list was empty before and still is (what consumes the list was not traced). (b) Room activation `0x510f00` calls `0x2efc00` at `0x510f4e` (server room `+0x18`, then `0x361100`, the flag test) and returns at once (`jne 0x5111f5`) for a flagged room, skipping the rolls after it. Those rolls look like the levels.txt `ObjGrp` random objects (inference). Effect: the marked entrance room gets no random objects (chests, shrines and the like); accepted, and said in the README and the CHANGELOG | static (disassembly); `ObjGrp` is inference |
| What `0x30000` probably means | the waypoint-room mark (objects.txt SubClass bit `0x40` is the waypoint class, and section 10's hover text uses that bit); this would explain both readers above | inference from the column's meaning |
| Maze pieces | the Cave and Crypt maze pieces (`Prev`, `Down`, the 15 mask pieces) place no such object, so no room of a built maze holds `0x30000` | static for the loader path; the pieces' own DS1 files were not read |
| Slot row | the Maze slot row has `Warp0..7` = -1, so branch (2) is skipped | the row the staging scripts wrote (section 5, game checks) |

Cause (static, inference for the firing branch): with branch (1) empty and branch (2) masked off, the player arrives in the room at the declared centre (3). That room is the middle of the level rect the plan wrote (`LvlSizeX/Y`, origin `+0x24/+0x28`), which is unrelated to the planned entrance cell; the five games with an arrival line, all outside the entrance cell, fit this. Loop mazes place no `Prev` piece, so they had no entrance cell at all, but the plan always has `plan.entrance.room`, so they now get one.

Fix, which calls and hooks nothing new: after the steered build the hook gives the room in the middle of the planned entrance cell the flags `0x30000` and takes those bits off every other room, so branch (1) answers with the planned cell whatever the tileset's pieces carry. The pure choice is `planArrival` in `src/maze/steer.cpp`: it needs the built rooms' box to equal the plan's box in cells (a Cave cell is 3 x 3 engine rooms, a Crypt cell one room), and picks the room whose top-left corner lies in the cell and whose centre is nearest the cell's centre (the middle of the cell's 3 x 3 engine rooms for Cave, 8 x 8 tiles; Crypt likewise). Anything that does not fit (not steered, box differs from the plan's exactly, empty cell) changes nothing and logs why, so the game arrives as before. `MarkArrival` in `src/engine/maze_hooks.cpp` writes the flags, logs `maze arrival: room N of M ... marked as the level entry, in plan cell (x,y)`, and sets the probe's entrance cell to the planned one (so the arrival verdict also works for loops).

Parts (signature only, never called or hooked, each unique in the 93847 dump and checked by `tools/check_maze_signatures.py`): all three must be found, otherwise the arrival is left to the game and the maze steering stays on.

| RVA | Role | Signature (offset 0 = function entry) |
|---|---|---|
| `0x3da650` | Level-entry finder, through its `Position == 0` branch (the call of `0x3dad90` at `0x3da718`) | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B FA 41 8B F1 41 8B D0 41 8B D8 44 0F B6 E1 E8 ?? ?? ?? ?? 4C 8B AC 24 80 00 00 00 44 8B C3 4C 8B B4 24 88 00 00 00 48 8B D7 41 0F B6 CC 48 8B E8 41 C7 45 00 FF FF FF FF 41 C7 06 FF FF FF FF E8 ?? ?? ?? ?? 4C 8B F8 48 83 78 10 00 75 0C 48 8B D0 41 0F B6 CC E8 ?? ?? ?? ?? 83 BD 90 00 00 00 00 49 8B D7 41 0F B6 CC 74 ?? 83 FE 0D 75 0D 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ?? EB 10 4D 8B CD 4C 89 74 24 20 44 8B C6 E8 ?? ?? ?? ?? 48 8B D0 41 0F B6 CC 48 8B D8 E8 ?? ?? ?? ?? 48 8B 43 58 E9 ?? ?? ?? ?? 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ??` |
| `0x3dad90` | First room with the flag | `48 89 6C 24 18 57 41 56 41 57 48 83 EC 20 48 8B 7A 10 4D 8B F1 4D 8B F8 0F B6 E9 48 85 FF 74 15 48 8B CF E8 ?? ?? ?? ?? 85 C0 75 1A 48 8B 7F 48` |
| `0x3612d0` | Flag test `(room+0x50 & 0x30000)` | `8B 41 50 25 00 00 03 00 C3` (ends at the `ret`; unique on its own) |

Not excluded: further readers of room `+0x50` bits 16 and 17 beyond the ones listed (the sweep found `0x3272a0`, `0x510f4e` via `0x2efc00`, the finder's helper and the DS1 loader; what consumes the `+0x208` list was not traced), and the cause of the earlier `+0x66259d` crashes (they were with `Warp0..7` = 0, a different row). **Needs game (done 2026-11-12, see "Game check of the arrival fix" below):** one Cave game and one Loop game with `MazeProbe=1`: the `maze arrival:` line after the build, then the `[maze probe] arrival` line must say `ARRIVAL IS IN THE ENTRANCE CELL`; and confirm the entrance room has no random objects (chests, shrines). If it does not, log the callers and results of `0x2ef8b0` and `0x3da650` (section 5 Q3, section 8 Q6) to see which branch answered.

### Game check of the arrival fix (issue #127, PR #133)

Run on 2026-11-12 in test slot 6 (D2R build 93847, D2RLoader 1.3.1-beta, the test mod `D2RToolsTest` and the test save `cartos`, a level-1 Barbarian, Normal difficulty, `MazeProbe 1`), with `cartographer.dll` built from `main` at `abcf72b` (the merge of #133). Staged like the earlier runs of this section: the vanilla `levels.txt`, `lvlprest.txt`, `lvlmaze.txt`, `itemstatcost.txt` and `misc.txt`, the hand-made level 139 row (`Warp0..7` -1, 360 x 360, OffsetX 6000), `slots.txt` `Maze 139`, `core-maps.dll` moved out of the slot; vanilla `itemstatcost.txt` was in the mod, so no map-stats WARN lines this time. Per game: a new game, `cartographer open <Name>`, enter the portal, read the log. The plugin logged `part ArrivalFinder`, `ArrivalRoomFinder` and `ArrivalFlagTest`: found by signature on startup. **live** = seen in the running game.

| Game | Build | `maze arrival:` line | `[maze probe] arrival` verdict | Entrance room objects | Crash |
|---|---|---|---|---|---|
| Cave Corridor | built as planned, 17 rooms (153 engine rooms), entrance placed | room 148 of 153 at (6008,128), plan cell (0,5), 0 other rooms lost the mark | player at (6008,128) in plan cell (6000,120), entrance cell (6000,120): **ARRIVAL IS IN THE ENTRANCE CELL** | one screenshot of the game window: the portal and a torch, no chest or shrine in view | none |
| Cave Loop | built as planned, 16 rooms (144 engine rooms); the Prev-piece count is 0, as for every Loop | room 67 of 144 at (6008,80), plan cell (0,3), 0 other rooms lost the mark | player at (6008,80) in plan cell (6000,72), entrance cell (6000,72): **ARRIVAL IS IN THE ENTRANCE CELL** | one screenshot: the portal and a torch, no chest or shrine in view | none |
| Crypt Branching (Crypt slot, game restarted for the other `LevelType`) | built as planned, 30 rooms, entrance placed | room 29 of 30 at (6000,32), plan cell (0,4), 0 other rooms lost the mark | player at (6000,32) in plan cell (6000,32), entrance cell (6000,32): **ARRIVAL IS IN THE ENTRANCE CELL** | one screenshot: the portal and pillars, no chest or shrine in view | none |

Levels of evidence, kept apart:

- **Live, build 93847 / D2RLoader 1.3.1-beta:** 3 games out of 3 (Cave Corridor, Cave Loop, Crypt Branching) arrived in the planned entrance cell, where the 5 games with a verdict before the fix did not (second run of this section). The marked room was the room the probe reported as the arrival room, in each game. The built grid equalled the plan in all three; no crash file was written (the slot's crash folder holds only older files, none from this run).
- **Live, with limits:** on the Loop game the engine's Prev-piece count is 0, as for every Loop, so the probe takes the entrance cell from the plan; the arrival landing in it is down to the mark. The "no random objects" check is by eye on one screenshot per game, taken a few seconds after arrival, of the part of the room on screen (a room is 8 x 8 tiles); it saw no chest or shrine, which fits the `0x510f00` reading above but does not prove it, because a room without a random object also looks like this. The Cave games each logged the build and the mark twice (the same room both times); the second pass came after the arrival line and was not looked into.
- **Not run:** Nightmare and Hell; a Cave Winding game and a second Crypt shape; any TCP/IP client (the client's arrival goes through the host's room, untested); build 94995; what consumes the `+0x208` list that `LevelBuild` fills (unchanged, not traced); the two other callers of the finder through `0x2efe80`.
- **Static only, unchanged:** the finder's branch order, the readers of the flag and the `Position == 0` reading are as in the table above. This run confirms the outcome the readings predicted (branch 1 answered with the marked room); it did not log which branch of `0x3da650` fired.

Slot 6 was set up for this run from the test mod and test save of slot 11 (slot 6 had neither), then restored to its backed-up state (all 2043 files byte-identical by SHA-256, the added mod, tables, save folder and `cartographer.dll` removed, `core-maps.dll` put back).

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

## 7. Map stats parts (issue #80)

**Status:** written and unit tested; player half off by default (`PlayerStats`); signatures checked statically on the build 93847 dump (`check_portal_signatures.py`); checked live on 2026-11-12 (Game checks of 2026-11-12 below; one fix, the chain offsets). Applies the section 6 recipe: monster stats from a hook on the monster setup `0x494a60`, player stats from the existing PlayerTick hook (section 3). Code: `plugins/cartographer/src/engine/stat_apply.cpp`; pure plan and tracking logic in `src/statplan/`.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer map stats (`src/engine/stat_apply.cpp`) |
| Module | `D2R.exe` (SetListStat and AttachStatList are D2RLoader thunks into `D2RCore.dll`: call, never hook) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | `NewStatList` `0x2f7300`, `SetListStat` `0x2f7c00` (thunk), `AttachStatList` `0x2f2cb0` (thunk), `DetachStatList` `0x2f7920`, `FreeStatList` `0x2f4180`, `MonsterSetup` `0x494a60` (hooked). `UnitRoom` `0x34b440`, `RoomLevelId` `0x2efc10`, `UnitStat` `0x2f5020` are the portal's parts, found again under the names `StatUnitRoom`, `StatRoomLevelId`, `StatUnitStat`. Unit fields read directly: type `+0x00` (0 player, 1 monster), id `+0x08`, data flag `+0x1bd` (a byte: the three short helpers `0x34b9d0`, `0x34a330`, `0x34a0e0` are exactly these loads behind a null check, read in this change), stat owner list `+0x88` (the engine's `0x34b870`), on which the chain head is `+0x90` (second head `+0x98`) and next is `+0x68` (live, see Game checks of 2026-11-12 below; the first version read the heads off the unit and faulted). The hook's first argument is the spawn block: `+0` game, `+8` room, `+0x18` monstats id (read at `0x494a72` to `0x494a84` and the caller `0x4943ec`..`0x4943f2`) |
| Guard | Signatures in `src/core/part_signatures.h` (`NewStatList`, `SetListStat`, `AttachStatList`, `DetachStatList`, `FreeStatList`, `MonsterSetup`). Each matched exactly once. `MonsterSetup` is 21 bytes with no `??`; the hook cuts the first 14 (whole instructions: `mov [rsp+18],rbx; mov [rsp+20],rbp; push rsi; push rdi; push r15`), which D2RLoader re-checks at the entry. The first relative operand is the `call 0x976e0` at `+0x2b`. Where a part does not match once, the part is logged and left off |
| Contract | Monster: after the game's own setup returns, if a plan is active and the room's level is the slot level, `NewStatList(0, 0, 1, unitId)`, `SetListStat(dataFlag, list, stat, value, 0)` for each monster entry, `AttachStatList(monster, list, 1)`. The list is never tracked or freed (it goes with the unit; inference). Player: from the tick, when the player's room level is the slot level, the same with type 0, and the list address is kept per unit id; on leaving the level (or the map closing) it is removed only if it is still on the unit's chain: `DetachStatList(unit, list)`, `FreeStatList(dataFlag, list)`. No state is set and no state bit is changed (the engine's own lists use a state, but a state id that does not exist in the game's states table would write outside the bit set; the plugin keeps the list address instead). Game thread only; the tables are read under a lock. On `GameLeft` the plugin forgets its state and touches no game memory |
| Evidence | `dump_dis.py ... dis 0x494a60 16` (prologue, args), `dis 0x4943d0 14` (the call and that its result is unused), `dis 0x34a0e0 / 0x34a330 / 0x34b9d0` (field loads), `dis 0x2f7920`, `dis 0x2f4180` (detach computes its own data flag; free does nothing for a list with a negative flags word), `pat "<signature>"` for uniqueness; `check_portal_signatures.py` for all of them |
| Validation | Unit tests (`statplan_tests`: the draw to plan, stat id lookup, player apply and remove decisions, the tracking table), signature check on the dump, and the live checks below |
| Failure mode | Each half is off when any part it needs is missing or its guard fails; the other half still works. Nothing is replaced by a guess. A map opens either way |
| Coexistence | The monster hook is a new hook on `0x494a60`, which nothing else here hooks; it calls the original first. The player half rides the portal's PlayerTick hook (also Better Auras' neighbour `0x42e600`). The D2RCore spawn hook (flag `0x40`) runs after `0x494a60` and is untouched |

### Needs game (the section 6 stats checks, settled by the `statprobe:` log lines)

1. **Percent stat on a monster, and the order against `0x49dd70`.** For the first six monsters of a map the log has `statprobe: monster #n ... stat 12 (level) was L at the hook, maxhp A -> B, totals [..] -> [..]`. A level of 0 at the hook means `0x49dd70` runs later; a nonzero level means earlier. Totals that change by the listed value show the list counts; maxhp A -> B shows whether `item_maxhp_percent` reaches a monster's life (if not, pd2's direct `SetUnitStat` on maxhp is the fallback). Then check the monster's life in play, and whether a later level write resets it.
2. **List on the chain.** The same lines end `list on chain: yes/NO (n lists)`. `NO` means the chain offsets are wrong for an attached list; removal then never runs (it only frees a list it finds). Result: the chain hangs on the unit's owner list `[unit+0x88]`, not on the unit; fixed, and now `yes`.
3. **Client replication.** (Player half: see "Client sync of player stats" below, issue #104.) The probe is server side. In play: the monster's life bar and damage as the client sees them, the player's resistances on the character sheet, and the game's own stat text (the client builder `0x2dc4b0`).
4. **Stats without a state.** If totals do not change at all, the list needs a state; add one (a state id read from `states.txt`) and re-test.
5. **Player list and saves.** `statprobe: player ... entered the map` and `left the map` show the stat totals and chain length before and after. Save and exit inside the map, reload, and read `statprobe: first player unit ... holds N stat lists` against a game that never opened a map; a higher count means a list survived the save.
6. **Removal and leaving.** Leave the map level (town portal): the log shows `left the map: list removed (chain n -> n-1)`, and the stats are gone from the character sheet. Leave the game: `left the game: N monster(s) and M player(s)`, and a new game shows no leftover stats.
7. **Every monster.** Count of `monster #n` against the monsters seen in the level; whether minions and bosses get the list (they all pass the same setup call).
8. **Monster list free or leak.** Monster lists are never tracked or freed; that the game frees a unit's lists with the unit is inference (section 6). Heap count (or process private bytes) before and after several map runs with many monsters; steady growth per monster means the lists leak and each needs a detach and free on the monster's death or removal.
9. **Level stat on monsters.** `level` is refused on `Monsters` rows (the game's own level write at `0x49dd70` runs in or after the same setup and would interact with it); nothing here tests that write, so a `level` row is not offered until it has been checked.

### Game checks of 2026-11-12

Slot 11, D2RToolsTest mod, test save `cartos` (Barbarian level 1), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, Normal difficulty. Test tables: map `StatsMap` (level 138, `Properties` 6, `PlayerStats` 1) and a `mapstats.txt` of `item_maxhp_percent` 100, `fireresist` 50, `level` 5 and `hitpoints` 50 (monsters), `strength` 40, `coldresist` 30, `gold` 500, `state` 1 and `nonexistent_stat` (players). Monsters from `cheat mspawn zombie1,3`; the map was opened with `cartographer open StatsMap`. `core-maps` was moved out for the run. Levels: live = seen in the running game; not done = not tried. Screenshots are in `D2RTestSlots\runs\carto-stats-checks\screenshots` (not committed).

| Check | Level | Result |
|---|---|---|
| Parts found, hook installs | live | The stat parts and `MonsterSetup` matched once at load; the hook on `0x494a60` installed. |
| Fault in the first build | live | The first Play crashed: the crash guard logged a read of `0x69` in `cartographer.dll`, in the chain walk. Cause: the walk started at the unit (`+0x90`) instead of the unit's owner list. Fixed in this change: the walk starts at `[unit+0x88]`, every pointer is checked readable first, and the probe logs the owner list and heads. |
| Chain offsets | live | After the fix the first player unit holds 2 lists in a fresh game (`+0x90` set, `+0x98` null; the 2 -> 3 step before the map is not explained, probably a list the game attaches after the first tick, not checked); entering the map makes 4 (`list on chain: yes`, 3 -> 4 at the first probe) and leaving makes 3 again (`list removed (chain 4 -> 3 lists)`). |
| Monster stats in the slot level | live | Three zombies spawned in the map level: `statprobe: monster #n ... totals [39=0 76=0] -> [39=50 76=100], list on chain: yes (3 lists)`, `maxhp 1792 -> 3584`, `2560 -> 5120`, `2048 -> 4096`. The level stat was 1 at the hook, so the game's level write at `0x49dd70` runs later (and `level` stays refused). The overlay (`inmap2.png`) shows a zombie with Life 16/16 and Fire Resist 50%. `item_maxhp_percent` does reach a monster's maxhp. |
| Not applied elsewhere | live | Zombies spawned in town (before the map and in a new game after it) got no `statprobe: monster` line; the count stayed at the map's 3. |
| Player stats apply | live | `entered the map: totals [0=30 43=0] -> [0=70 43=30]` (strength 30 to 70, cold resist 0 to 30) on the server. |
| Player stats on the character sheet | live, **fails** | The character screen inside the map still shows Strength 30 and Cold Resistance 0% (`char_map.png` against `char_town_before.png`). The list is applied on the server only and does not reach the client's sheet in this single-player game. Filed as a sub-issue of #80. |
| Removed on leaving the map | live | Town portal out: `left the map: list removed (chain 4 -> 3 lists)`; a second entry applied again and removed again. |
| Removed on leaving the game | live | `left the game: 3 monster(s) and 1 player(s) were given the map's stats` (and `0 monster(s) and 2 player(s)` in the game that was left inside the map). A new game: the first player unit holds 2 lists, the same as the first game's baseline; the character screen shows Strength 30 and Cold Resistance 0% (`char_reload.png`). |
| Save and Exit inside the map | live | Save and Exit with the list attached rewrote `cartos.d2s` (timestamp changed). Its character stats section: strength 30, dexterity 20, vitality 25, energy 10, no +40. Reload: 2 lists, Strength 30. Section 7 check 5 passes for strength; resistances are not in the saved base stats at all. One test character, one exit; an autosave inside the map was not separately tried. |
| Refused stats | live | The log names each refusal at load and again when the map draws them: `level` and `hitpoints` on monsters, `gold` and `state` on players (`... skipped: ... is refused ...`). |
| Guards fail open | not done | No part could be made to mismatch in the live game (no altered binary). Static: a part that does not match once is logged and its half left off (unit tested). The crash guard logged the one fault above with its cause. |
| Natural spawns | not done | `cheat mspawn` goes through the same `0x494a60` setup call; the map's own population was not walked (rooms were populated but no monster was set up in them in the second run, so the cheat spawns carried the check). |
| Life bar and damage, heap growth | not done | Only totals, maxhp (server) and the overlay were read; no heap measurement (check 8). |

Correction to section 6: the list chain is on the owner list `[unit+0x88]` (`+0x90`, `+0x98`, next `+0x68`), not on the unit. `PlayerStats` stays `0` by default: the character sheet does not show the stats, so the feature does nothing a player can see, although a Save and Exit with it on left a test character clean.

### Player stats are off by default (review of issue #80, fix cycle 1)

- **`PlayerStats` in `cartographer.txt` is 0 by default.** While it is 0 nothing is attached to the player: the plan carries no player entries and the tick does not apply. Reason: the list is on the unit's chain until the next tick outside the map level, so a Save and Exit inside the map, or an autosave there, may write the character while it is attached. Whether the save writer reads base stats only is inference (equipped items' lists sit on the same chain and are not saved) and needs check 5 above. Check 5 has passed for one Save and Exit (strength, section 7); an autosave inside the map was not tried, so use `1` on a test slot only. Removal when the map closes or the player leaves the level is unchanged.
- **Refused stats.** `buildPlan` refuses these on `Players` rows, because the game reads them as a total (`UnitStat`: base plus lists) and writes the result back as the base value: `statpts` (4), `newskills` (5), `hitpoints` (6), `mana` (8), `stamina` (10), `level` (12), `experience` (13), `gold` (14), `goldbank` (15). Matched by name or id. The other stats of the vanilla itemstatcost with no item bit (`lastexp` 29 and `nextexp` 30) are level-table look-ups and are not written back; `maxhp`, `maxmana`, `maxstamina` and the attributes are item stats and allowed. No further stat with this behaviour was found by reading vanilla `itemstatcost.txt`; this is a static reading of the table's save columns, not of the game's writers. `level` is also refused on `Monsters`, and so are `hitpoints`, `mana` and `stamina` (6, 8, 10): the mechanism above also runs for a monster on every hit and on regeneration, so a `+X` list would be given back each time base is rewritten (inferred; the damage writer was not traced in the dump). Param stats are refused on both targets: the 17 rows of the vanilla 3.3.0 `itemstatcost.txt` with a Save Param Bits value (static reading of that column; ids 83 `item_addclassskills`, 97 `item_nonclassskill`, 98 `state`, 107 `item_singleskill`, 126 `item_elemskill`, 151 `item_aura`, 155 `item_reanimate`, 179 `attack_vs_montype`, 180 `damage_vs_montype`, 188 `item_addskill_tab`, 195-199 and 201 `item_skillonattack`/`onkill`/`ondeath`/`onhit`/`onlevelup`/`ongethit`, 204 `item_charged_skill`), matched by name or id, because the list entry is set with layer 0, which would name skill or state 0. Stat ids are the itemstatcost row numbers, not the `*ID` column (vanilla row 212 says 213). `check` and the report name each refused row.
- **Generation counter.** A counter bumped under the lock on `GameLeft` is read with the plan on each tick and checked under the lock before a made list is recorded; if the game was left meanwhile the list is not recorded or touched (it belongs to the old game's unit). So a list pointer from an earlier game is never used, whichever thread delivers `GameLeft`.

### Client sync of player stats (issue #104)

**Status:** desk work on the build 93847 dump only; written, not verified in game. The compiled states row layout is now proven at dump level (below), so Route A is implemented in `stat_apply.cpp` behind `PlayerStats=1`: the state is picked at run time by a pure function (`pickPlayerState`, unit tested), the list gets that state, and the game's own sender ships it. Everything in game is open (the needs-game list). Levels: **dump** (read in the dump with Capstone), **SDK** (read in the pinned `third_party/PluginSDK`), **inference**, **needs game**.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer player map stats (`src/engine/stat_apply.cpp`, issue #104, sub-issue of #80) |
| Module | `D2R.exe` (the unit update and the state sender are in `D2R.exe`; the list internals and `0x477310` forward into `D2RCore.dll` and were not read) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | The table below |
| Guard | The prefixes below; each matched exactly once with `dump_dis.py ... pat` on the dump. The four the code calls (`FindStateList`, `SetListState`, `SetStateBit`, `StateOn`) are in `src/core/part_signatures.h` and `tools/check_portal_signatures.py` (`all signatures ok` on the dump); the states-vector offsets are checked at run time against the SDK table |
| Contract | Below |
| Evidence | `dump_dis.py ... dis <rva>`; callers found by scanning `E8`/`E9` rel32 operands; prefixes checked by `pat` |
| Validation | Dump (layout, prefixes, signature checker); host unit tests for the pick and the table check (`tests/statplan_tests.cpp`); a clean build. In game: nothing |
| Failure mode | Player stats stay off (logged) when a part does not match once, the SDK has no DataTable service, the SDK states table is not the game's, no state is unused, or the refusal rule or the post-set finder check fails. No version gate |
| Coexistence | `0x53a210` and `0x539890` carry D2RLoader's `jmp [rip+x]` at their entry (the wide-state senders, toolkit `engine-re.md`). The original body of `0x53a210` follows the jmp and is what was read; that the replacement sends the same packets for states below 512 is inference |

**Answer in one line.** The game does not sync a stat list by looking at the list or the unit. It syncs **states**: the server sends each changed, switched-on state of a unit together with that state's stat list (packet `0xa8`), and the client builds its own list from it. The plugin's list has no state and no state bit is ever set, so nothing is sent. The route is `SetListState` plus `SetStateBit`, both call-only; the open choice is which state id.

| RVA | Role | Prefix (matches once) | Level |
|---|---|---|---|
| `0x42d410` | Server unit update for one client `(?, unit, client, flag)`. At `0x42d5f4` it calls `0x335c60(unit)` and, when nonzero, `0x539880(unit, client, 0)`; at its end it calls `0x2f4960` for stats `0x43`, `0x44`, `0xc`, `0`, `2` with `0x4f3ad0` as the callback | `40 53 56 57 48 83 EC 50 4C 89 64 24 78 48 8B DA` | dump |
| `0x335c60` | Any state changed on the unit (reads the changed array from `0x2f62d0`) | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B E9 E8 ?? ?? ?? ??` | dump |
| `0x539880` | `(unit, client, all)`: `all == 0` jumps to `0x53a210` (changed states), else to the `0x539890` thunk (all states) | `45 85 C0 0F 85 ?? ?? ?? ?? E9 ?? ?? ?? ??` | dump |
| `0x53a216` | Body of the changed-state sender `0x53a210` (its entry is the loader's 6-byte `jmp`, so the prefix starts after it) | `48 8D AC 24 88 FE FF FF 48 81 EC 78 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 50 01 00 00 48 8B F2 48 89 54 24 48` | dump |
| `0x335d00` | State `(unit, state)` is flagged changed | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 48 63 DA 48 8B F1` | dump |
| `0x3351b0` | State `(unit, state)` is on (unit types 0, 1 and 3 only: `0x34b9d0` returns `[unit+0]`, tested at `0x3351c9`; players are type 0) | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B DA 48 8B F1 E8 ?? ?? ?? ?? 85 C0 74 ??` | dump |
| `0x3354c0` | `SetStateBit(unit, state, on)`: checks `state < [table + 0x298]`, calls `0x2f7ea0`, then `0x38ebf0(unit)` (sets bit 13 of `[unit+0x124]` and links the unit into an update list, which is what makes the next unit update run) | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 20 41 8B E8 8B DA 48 8B F1` | dump |
| `0x2f72f0` | `SetListState(list, state)`, writes list `+0x20` (section 6) | `48 85 C9 74 03 89 51 20 C3` (9 bytes, whole instructions, matches once; the function is a leaf with only 9 bytes of real code followed by 7 `int3` of padding, so a longer prefix would depend on padding and is not used). The 9-byte body is what ships (`SetListState` in `part_signatures.h`); any byte-identical match does the same thing, so uniqueness is the only risk (a duplicate turns the part off) and the finder check after the call catches a wrong field. The caller anchor is an alternative, not used: `B9 02 00 00 00 E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 74 ?? BA 79 00 00 00 48 8B C8 E8 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ??` (matches once, at `0x339dd3`; the `E8` at `+26` is the `SetListState` call and its rel32 target must be the leaf above) | dump |
| `0x2f5940` | `FindStateList(unit, state)`: the first list on the unit's chain whose `+0x20` is `state`, or null (the sender's finder; the refusal rule and the post-set check use it) | `48 89 5C 24 10 57 48 83 EC 20 8B DA 48 8B F9 48 85 C9 75 13 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B CF E8 ?? ?? ?? ??` (matches once) | dump |
| `0x4801b0` / `0x4800a0` | Packet writers for `0xa7` (state on, no stats) and `0xa9` (state off) | `40 57 48 83 EC 40 C6 44 24 20 A7 48 8B F9` / `40 57 48 83 EC 40 C6 44 24 20 A9 48 8B F9` | dump |
| `0x2d9730` | Stat row `(dataFlag, stat)`; the row byte `+8` is the **send bits** | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30 48 63 F2 E8 ?? ?? ?? ?? 48 8B F8 48 8B DE 85 F6 78 ?? 48 3B 98 60 12 00 00` | dump |
| `0x4f3ad0` | Per-stat callback `(unit, stat, value, client)`: one branch sends attribute packet `0x1d`, `0x1e` or `0x1f` through `0x4782b0` (`0x4f3b26`); a second branch (`0x4f3b2d`) sends packet `0x20` through `0x47dfd0` when the unit is not the client's | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 30 48 8B F1 49 8B F9 49 8B C9 41 8B E8` | dump |
| `0x1c6ac0` | The client's own recipe for state `0x36` (find by state, else `NewStatList` with flags 2, `SetListState`, callback `0x2f7a50`, `Attach`). It reuses a list it finds; the plugin must not (see the refusal rule) | `40 57 48 83 EC 20 BA 36 00 00 00 48 8B F9` | dump |

**What the server does for a state (dump, `0x53a210` body).**
1. Copies the unit's changed array (`0x3352b0` -> `0x2f62d0`) and walks its set bits (`0xa1b9c0` finds the next one).
2. Skips a state whose state-table row has the flag `test [row+0x10], [0x1d996d0]` (the byte at `0x1d996d0` is `1`; this is `nosend` in `states.txt`, inference from the name; vanilla `nosend=1` rows include 12, 64 to 66, 121, 170, 171, 185).
3. `0x3351b0(unit, state)`: **off** -> packet `0xa9`. **On** -> list = `0x2f5940(unit, state)` (the list whose `+0x20` is this state). No list -> packet `0xa7` (state on, no stats). With a list, `0x2f64e0(list, buf, 0x10)` copies up to **16** `{stat<<16 | layer, value}` pairs.
4. For each pair the stat row (`0x2d9730`) gives the send bits (`row+8`). **A stat whose Send Bits is 0 is left out**; the others are packed into packet `0xa8` (type, id, state, packed stats; header at `0x53a4f4`, sent through `0x477310` at `0x53a6d7`). **Values are clamped to the Send Bits range** (`0x53a557` to `0x53a612`; the Signed flag is row `+4` mask 2): an unsigned stat goes out clamped to `0..2^n-1`, so a negative `strength` is sent as 0; a signed stat to `-2^(n-1)..2^(n-1)-1`, so 9-bit resistances are limited to -256..255 (reading of the clamp branches, not run). A `check` warning for plan values outside that range would be worthwhile.
5. In the toolkit's copy of vanilla `itemstatcost.txt` (build 93847, static) Send Bits is 11 for `strength` and `dexterity`, 9 for the four resistances, 32 for `maxhp`, 16 for `item_maxhp_percent`. The test stats of the 2026-11-12 check (`strength`, `coldresist`) would be sent.

**What sets the changed flag (dump).** `0x2f7ea0` (its only caller is `0x3354c0`) writes the state bit at `[statlistEx + idx*4 + 0xaf0]` and, when the bit changes, ors the matching bit of the changed block (`0x2f7ffb`, `or dword [r14+rcx*4+0xaf0], ebx`). So `SetStateBit` is the dirty flag. There is no dirty flag on the list or the unit, and `AttachStatList` sets none; this fits the live result (list on the chain, state never on, nothing sent). Where the sender's source array is cleared so a state goes out once was not read (`0x53a210` works on a local copy; the clear is in `D2RCore.dll`: inference).

**Why the sheet showed nothing (inference, fits the dump).** The per-update stat walk `0x2f4960` is asked for stats `0x43` (velocity), `0x44` (attack rate), `0xc` (level), `0` (strength) and `2` (dexterity) with callback `0x4f3ad0`. It calls the callback only when the stat is **absent** from the owner's `+0xc0` array (`0x2f49ea`: `cmp eax, -1; jne` skips otherwise); only then does it read the entry under `+0x30` (`[entry+4]`), the unit's own **base list** value, not the total. The plugin's list is a separate list on the chain, so the client's base stays 30. The client's total is its base plus the lists it holds, which come from item packets and state packets (`0xa8`). The plugin's list reaches the client by neither.

**Route A: replicate by state (recommended; needs game).** Keep the engine's order: `NewStatList`, `SetListStat`, `Attach` (`0x2f2cb0`), `SetListState(list, S)` (`0x2f72f0`), then `SetStateBit(unit, S, 1)` (`0x3354c0`) last. The two engine recipes differ: `0x213000` is `Attach` (`0x21311d`), callback `0x2f7a50` (`0x21312c`), `SetListState` (`0x213139`), `SetStateBit` (`0x21314c`); `0x339d70` is `SetListState` (`0x339ded`, state `0x79`), `0x2f7a50` (`0x339dfc`), `Attach` (`0x339e0d`), `SetStateBit` (`0x339e1e`). Only `0x213000` sets the list's state after `Attach`; in both, `SetStateBit` is last, after the list is linked. `SetStateBit` itself does not need the list (`0x2f7ea0` never touches one; the sender finds it with `0x2f5940` at send time), so the order is the engine's practice, not a requirement. The next unit update (`0x42d410` -> `0x539880` -> `0x53a210`) sends `0xa8` to every client that sees the unit (the local client too in single player: inference), so it also works for other players in multiplayer. Removal, in this order: `SetStateBit(unit, S, 0)` (the client gets `0xa9`), `DetachStatList`, `FreeStatList`.
- **Refusal rule (the game can free the plugin's list).** About 30 places in `D2R.exe` call `0x2f5940(unit, state)` and then Detach `0x2f7920` and Free `0x2f4180`; some use a fixed id (`0x44dddd`, state `0xb`), some a table-driven one (`0x3379a8`, state from the word `[r14+0xb4]` of a table row; `0x21acc0`, a variable state). If the game ever clears `S` on the player it detaches and **frees the plugin's list** while the plugin still holds the pointer. The existing chain check (`stat_apply.cpp:259`, "no longer on the unit ... forgotten, not freed") prevents a double free, except when the freed address is reused (ABA). Also `0x2f5940` returns the **first** list whose `+0x20` is `S`, so with a game list already on `S` the game's list would be sent instead of the plugin's, and reusing a found list as `0x1c6ac0` does would write into a game list. So the plugin must **not reuse a list found on `S`**: apply only when `0x3351b0(unit, S) == 0` (bit off) **and** `0x2f5940(unit, S) == nullptr` (no list carries `S`); otherwise log it and leave player stats off. After setting the state, check that `0x2f5940(unit, S)` returns the plugin's own list; if not, undo and log.
- **The open choice, `S` (needs game).** `S` must be below the state table count (`0x3354c0` checks it; the SDK `DataTableService` table `States` = 18 gives the row count), must not be `nosend`, and must not be a state the game or the mod sets or reads: the client acts on the state (overlays, `stat`, `setfunc`/`remfunc`, `skill`, colour, curse and group rules in `states.txt`), and removing the bit would also clear a state that was already on. No vanilla row can be called free from the dump (state 0 is asserted against in the sender, `0x53a3ac`). The game names some rows (a scan of `D2R.exe` call sites that pass a constant state id to `0x2f5940`, `0x2f5b80`, `0x3354c0`, `0x3351b0` or `0x2f72f0` finds 6, 7, 18, 22, 24, 52, 62, 100 to 107, 109, 118, 154, 164, 171, 172, 184 to 186, 189, 209 and 219 among them; an under-count, since skills and items pick states from tables), items use 165 to 171, and passive skills use the mastery rows through their `aurastate`.
  - **Server-side effect of `SetStateBit` (dump, `0x2f7ea0`, `0x2f8078` to `0x2f80b5`).** It reads byte `+0x12` of the state row (rows are `0x44` bytes: `imul rax, ..., 0x44` at `0x2f806d`). When that byte has mask 2 (the global at `0x1d996d4` holds 2) it calls `0x34e140(unit, 8, on)`, which toggles unit flag 8; on clear it does so only if `0x336280(unit, 0x11)` is 0. Rows with that bit must be excluded.
  - **The compiled states row (dump, settled).** Row size `0x44`. The `states.txt` loader at `0x3074d0` builds a `0x20`-stride column descriptor array (name, type, row offset or bit, destination) and passes the row size to the generic parser in D2RCore (`call 0x3e2b544`). Types: 29 = a bit of the 8-byte flag field at `+0x10`, 22 = an i16 name lookup (empty is negative), 3 = i16, 4 = byte, 2 = dword. The flag field: bit `k` is byte `+0x10+(k>>3)`, mask `1<<(k&7)`, proven by the post-load loop at `0x3085d5` to `0x30863e` and the mask table at `0x1d996d0`.
    - `+0x00` is the key column `state` (type 17, destination `dt+0x2a8`). `+0x02` overlay1, `+0x04` overlay2, `+0x06` overlay3, `+0x08` overlay4, `+0x0a` castoverlay, `+0x0c` removerlay, `+0x0e` pgsvoverlay: i16, negative = none (consumers test `movsx; test cx,cx; jns`: `0x2193ba`, `0x219597`, `0x21977e`, `0x21a27b`).
    - Flag bits `+0x10..+0x17`: 0 nosend, 1 aura, 2 hide, 3 hidedead, 4 transform, 5 pgsv, 6 active, 7 remhit, 8 damblue, 9 damred, 10 attblue, 11 attred, 12 curse, 13 curable, 14 plrstaydeath, 15 monstaydeath, 16 bossstaydeath, **17 disguise (byte `+0x12` mask 2, the test `SetStateBit` makes)**, 18 restrict, 19 a column named `blue` (not a vanilla header), 20 armblue, 21 rfblue, 22 rcblue, 23 rlblue, 24 rpblue, 25 stambarblue, 26 armred, 27 rfred, 28 rcred, 29 rlred, 30 rpred, 31 exp, 32 shatter, 33 life, 34 udead, 35 green, 36 nooverlays, 37 noclear, 38 bossinv, 39 meleeonly, 40 notondead, 41 sunderfull, 42 sunder-res-reduce.
    - `+0x18` stat (i16 lookup), `+0x1a` setfunc (i16), `+0x1c` remfunc (i16; `cmp ax,0x1d; ja` at `0x21ae73`), `+0x1e` group (u16; 0 = none at `0x4370e7`, `0x5d102d`, `0x5d5e0f`), `+0x20` colorpri (byte, `0x21b561`), `+0x21` colorshift, `+0x22..0x24` light r/g/b, `+0x26` onsound, `+0x28` offsound, `+0x2a` itemtype, `+0x2c` itemtrans, `+0x2d` gfxtype (byte), `+0x2e` gfxclass, `+0x30` cltevent, `+0x32` clteventfunc, `+0x34` cltactivefunc, `+0x36` srvactivefunc, `+0x38` skill (i16, `jl` at `0x219ad4`), `+0x3a` missile (i16), `+0x3c` canstack (byte), `+0x40` vfxweaponstate (dword).
    - **Where the loader is the evidence (dump):** the descriptor table gives name, type and offset; the consumers above confirm the signedness and "empty" for overlay, group, remfunc, skill. The empty value of `setfunc` and `remfunc` is **dump**: setfunc dispatches through the table at `0x23600a0` (`cmp ax,0x1e; ja` at `0x21af74`) and remfunc through `0x23601a0` (`cmp ax,0x1d; ja` at `0x21ae73`); entry 0 of both tables is null, and -1 fails the unsigned bound, so 0 and -1 are both no-ops, and the pick accepts both. `stat` is a type 22 lookup like overlay, so it must be negative. The empty value of the colour, light, sound, itemtype, itemtrans, gfx, clt/srv function and vfxweaponstate columns is not read from a consumer: see the next bullet. The first version of the pick guessed it (0 for every byte column, 0 or negative for sounds, itemtype and cltevent) and the first game run (issue #104) found no state, which the next bullet explains.
    - **Empty values per column (file evidence, 2026-11-12; row 39 read from game memory in the second check below).** The D2RLoader compiler's own output for this build is on disk: `data/compiler/3.3.0/data/global/excel/states.bin` (magic `D2RLBIN`, 232 rows of `0x44` bytes starting at file offset `0x7b7`, the first row's key `0x0000`, the next `0x0001`, ...), next to the `states.txt` it was compiled from. Reading every row of the `.bin` against the `.txt` (232 rows, each column, empty cell against stored bytes) gives, with no exception in any column:

      | Column (offset, width) | Empty cell is stored as | A non-empty cell, for contrast |
      |---|---|---|
      | overlay1-4 `+2..+8`, castoverlay `+0xa`, removerlay `+0xc`, pgsvoverlay `+0xe`, stat `+0x18`, cltevent `+0x30`, skill `+0x38`, missile `+0x3a` (i16 name lookups) | `0xffff` (-1) | the row index of the named row; a lookup that names row 0 is stored as `0x0000` (cltevent has one such row), so 0 is not empty |
      | flag field `+0x10..+0x17` | all 8 bytes `0` | bit set per `1` cell |
      | setfunc `+0x1a`, remfunc `+0x1c`, group `+0x1e`, clteventfunc `+0x32`, cltactivefunc `+0x34`, srvactivefunc `+0x36` (words) | `0x0000` | the number |
      | colorpri `+0x20`, colorshift `+0x21`, light r/g/b `+0x22..+0x24`, gfxtype `+0x2d`, canstack `+0x3c` (bytes) | `0x00` | the number |
      | onsound `+0x26`, offsound `+0x28`, itemtype `+0x2a`, gfxclass `+0x2e` (words) | `0x0000` (the sound and itemtype lookups are **not** -1) | the lookup row; gfxclass is a plain number |
      | **itemtrans `+0x2c` (byte)** | **`0xff`** | `0x09`, `0x0c` (a colour transform; 0 is a real value) |
      | vfxweaponstate `+0x40` (dword) | `0` | the number |

      Padding bytes `+0x25`, `+0x3d..+0x3f` and `+0x41..+0x43` are `0` in every row. `+0x39` and `+0x3b` are the high bytes of skill and missile.

      **This is why the first game run found nothing.** Every one of the 232 compiled rows has `0xff` at `+0x2c` unless it names an item colour transform, and the first pick required that byte to be 0 (a guess; "every byte column is 0 when empty"). So no row passed, although 43 rows are empty in every other column. The first pick's other guesses (sounds, itemtype and cltevent "0 or negative") were looser than the file, the setfunc and remfunc guess (0 or -1) matches it for 0.

      **Predicted rows (derived from `states.txt`, then compared with the file bytes):** rows 39 `lightningbolt`, 92 `death_delay`, 116 `dragonflight`, 230 `missilelimit` and 231 `primeevil_threat` have every cell empty in the `.txt`, and their `0x44` bytes in the `.bin` are all the same apart from the key: `kk kk` then 14 bytes `ff` (overlays), 8 bytes `00` (flags), `ff ff` (stat), 18 bytes `00` (setfunc to itemtype, with the colour and light bytes), `ff` (itemtrans), 3 bytes `00` (gfxtype, gfxclass), `ff ff` (cltevent), 6 bytes `00` (three functions), `ff ff ff ff` (skill, missile), 8 bytes `00` (canstack, padding, vfxweaponstate). Keys `27 00`, `5c 00`, `74 00`, `e6 00`, `e7 00`. The unit test `compiledEmptyRow` builds exactly this.

      **Evidence levels.** File (the compiler's output, the `.bin` and `.txt` in this checkout): the table above and the predicted rows. Dump: that the loader (`0x3074d0`) passes the row size `0x44` to the generic parser in D2RCore (`call 0x3e2b544` is a thunk into `D2RCore.dll`, not in the dump), so how the parser turns a `.bin` row into memory is **not** read in the dump; the in-memory row is assumed equal to the file row (the post-load loop at `0x3085d5` only builds bit arrays from the flag field). Game: row 39 of the live table holds the same bytes (second check below, 2026-11-12); the other rows were not read.

      **Pick, after the fix.** Candidates in the `.bin` (every column empty as above, row above 0): 6, 7, 18, 22, 24, 39, 67, 69, 71, 72, 74 to 81, 83, 92, 100, 102, 106, 109, 116, 152, 155, 164 to 169, 181 to 183, 186, 188, 197, 209, 229, 230, 231 (43). Those in the static list (6, 7, 18, 22, 24, 92, 100, 102, 106, 109, 164 to 169, 186, 209) and those vanilla `skills.txt` names in `aurastate`, `auratargetstate` or `passivestate` (22, 24, 67, 69, 71 to 81, 83, 152, 155, 181 to 183, 188, 197, 229) go; what is left is **39, 116, 230, 231**, and the pick is **state 39** (`lightningbolt`). This reads the text of `skills.txt` for the three named columns and also `State1` to `State3` (capitalised in the file: `bear`, `wolf`; neither is a candidate), so the text check and the code agree; the code reads the compiled skills rows, so a state a mod's skills name is also excluded at run time. **Game result of the second check (below): the live table holds the predicted row and the pick is state 39.**
    - **Skills row (`0x2ec` bytes, loader `0x302380`, dump):** aurastate `+0xa0` (also `plugins/better-auras/src/aura_api.h`), auratargetstate `+0xa2`, passivestate `+0xb4`, state1/2/3 `+0x2d2`, `+0x2d4`, `+0x2d6`; i16, negative = none.
    - **Vector in `dt`:** the data tables base (`DataTablesBase(dataFlag)`) holds the states rows pointer at `+0x290` and the 64-bit count at `+0x298` (`SetStateBit` reads the count there; `GetStateRow` is `0x21b0c0`). The SDK `States` table (18) must give the same pointer, count and row size `0x44` for some bank 1 to 3, else nothing is used.
    - **Vanilla cross-check (superseded by "Pick, after the fix" above; kept for the history):** the first static reading (`states.txt` 3.3.0, 232 rows) counted 44 rows with every flag bit and the other columns empty. The compiled `.bin` has 43 above row 0, the static list holds 92, and the check was later run against the compiled table: the candidates are 39, 116, 230 and 231 and the pick is 39 (confirmed on the live table by the second game check below). A mod that adds rows gets its own answer from the same function.
    - **The pick (`pickPlayerState`, `statplan.cpp`, pure, unit tested):** the lowest `S` with `0 < S < count`, not in the static list (0; 6, 7, 11, 18, 22, 24, 52, 62, 92; 100 to 107, 109, 118, 154, 164; 165 to 172; 184, 185, 186, 189, 209, 219), overlays 1 to 4, castoverlay, removerlay and pgsvoverlay all negative, stat negative, setfunc and remfunc 0 or -1, group 0, and not named by any skills row at the offsets above. There is no relaxed pass: the whole flag field must be zero, skill and missile negative, canstack 0, and every other column of the row at its compiled empty value (the table above: itemtrans 0xff, cltevent negative, sounds, itemtype, gfxtype, gfxclass, colour, light, functions and vfxweaponstate 0); when no row qualifies the plugin logs the reason for the closest rows (state, first failing column, its offset and value; `explainNoPick`, also printed by `cartographer check`) and leaves player stats off. The static list also holds 11 (`0x44dddd`) and 92 `death_delay` (`SetStateBit(unit, 0x5c, 1)` at `0x448e42` to `0x448e4e`). The refusal rule below is the run-time backstop for states hard-coded in `D2RCore.dll` or granted by item properties.
  - **The Skills-table check is not enough.** Excluding states named by `aurastate`, `passivestate` or `state` in the loaded Skills table (SDK table 12, full compiled rows) does not catch states hard-coded in `D2RCore.dll` (the constant-id scan covered `D2R.exe` only) or states granted by item properties (the `state` stat, id 98, takes the state as its parameter). So the pick should also exclude the static list above and rely on the refusal rule for the rest.
  - **No plugin-owned state row.** A plugin cannot add a state: SDK `custom_tables.h` registers a plugin's own tables (`d2rloader/<plugin-id>/<name>`, read back by `copyRows`), not rows in the game's `states.txt`; and a plugin does not depend on the mod's files (AGENTS.md).
- **Open point for the code change (list flags).** The engine makes its state lists with `NewStatList` flags 2 (`0x339dd3`; `0x213100`, `0x14-0x12`; `0x1c6b07`, `0xc-0xa`) and gives them the `0x2f7a50` callback. The plugin's list uses flags 0 and no callback (`0x49bef0` is a flags-0 precedent, but it sets no state bit). Whether that matters for the sender or for removal was not read.
- **Needs game (item 1 answered, one run):** (1) with a chosen `S`, the character sheet moves (Strength 30 to 70, Cold Resistance 0 to 30) and returns when the bit is cleared: seen in single player with state 39 on the test character (second check below); (2) the client builds a list from `0xa8` for a state with no `stat` and no `setfunc` (the client handler of `0xa8` was not found and not read); (3) at most 16 entries go out per state (`0x2f64e0 ..., 0x10`), so a plan with more than 16 player stats would lose the rest on the client (handled by the cap in the issue #104 closing note, section 7; the loader also puts its `jmp [rip+x]` at the entry of `0x2f64e0` (the list copy), `0x2f4960` (the stat walk) and `0x2f2cb0` (`Attach`), besides `0x53a210`, so the 16-entry cap and the Send Bits filter are facts of the original bodies read here and the loader's replacements may differ); (4) timing: whether the state set in the tick hook goes out in the same update or the next; (5) whether a Save and Exit with the state on writes the state or its list (the save writer was not read; keep the test-slot rule); (6) whether the game clears `S` by itself (death, area change, `remhit`, curse or group replacement) and so detaches and frees the plugin's list; (7) a client that joins or comes into view later gets the state, through the loader's replacement of `0x539890` (the all-states sender, not read); (8) other players' clients in multiplayer see the change; (9) a save-clean check where the save writer is confirmed to run (the second check's save was unchanged, but the slot's loader log showed D2RLoader cancelling `cartos` saves earlier that day, so it does not show the writer ran with the list attached).

**Route B: also attach on the client's unit (fallback, not recommended).** The client's unit can be found with `0x9a5d0(id, type)`: a stub that hashes `type * 0x400 + (id & 0x7f) * 8` into a global table and jumps to a chain walk at `0x9f270`; the client packet handlers use it (`0x1c9410`: `byte [pkt+8]` type, `dword [pkt+9]` id). Its 36-byte prefix matches twice, so a longer one would be needed (not recorded). The same four calls (`NewStatList`, `SetListStat` with the client unit's data flag, `Attach`) work on that unit; the client builds its own state lists with them (`0x1c6ac0`, `0x213000`), and no state is needed for the totals. Against it: the client's data belongs to the client thread and the plugin ticks on the server's (a race on the chain); it shows only the local player's own sheet and nothing for other players in multiplayer; the plugin would have to remove the client list on every exit path itself. Use it only if Route A fails in game.

**Rejected.** (a) `SetUnitStat` on the base list (`0x2f7d10`): the base is what the save writer writes (check 5 above) and the value would stay after the map. (b) A new hook on the sender: not needed, the game's own sender already ships a state with its list. (c) Sending the attribute packets `0x1d` to `0x1f` (`0x4782b0`) ourselves: they set a base value on the client, which the plugin would have to track and undo. (d) The SDK: it has no stat, list or packet service (`network.h` is a message channel between plugins, `data_tables.h` reads tables only).

**Implemented (unit tested; the sheet effect seen in game once, in the second check below).** `stat_apply.cpp` resolves `SetListState`, `SetStateBit`, `StateOn`, `FindStateList` and `DataTablesBase` by signature (`ResolvePart`; `SetListState` uses its whole 9-byte body as the signature); player stats are on only when all are found, the PlayerTick part is on and the SDK DataTable service is there. Per player apply: pick `S` (SDK/`dt` cross-check, above); refuse when `StateOn(unit, S)` is nonzero or `FindStateList(unit, S)` is not null (logged once per map, player stats off for that map); `NewStatList`, `SetListStat`, `Attach`, `SetListState`; then `FindStateList(unit, S)` must return the plugin's list (checked before the bit, so a mismatch never toggles it), else the list is detached and freed and the refusal logged; then `SetStateBit(unit, S, 1)` last. `S` is stored with the list in the plugin's own record. Removal: the chain check, then `FindStateList(unit, S)` must still return the list (else the list is forgotten, not freed, and nothing is touched: the game may have freed it and reused the address), then `SetStateBit(unit, S, 0)`, `Detach`, `Free`. When no row has every column empty, the pick refuses and player stats stay off. Every failure leaves player stats off and logs; no version gate. The list flags stay 0 with no callback (the open point above).

**Next step (test slot).** Items 2 to 9 above are still open (a joining client, multiplayer, more than 16 stats, an autosave inside the map, death and curses, a save-clean check). If the chosen state misbehaves, the log line names it and `signatures.txt` cannot override the pick; change the static list or the pick rule from what the run shows.

### Game check 2026-11-12 (second): player stats on the character sheet (issue #104)

Run after PR #131 (the pick uses the compiled empty values). Cartographer was built from origin/main at or just after 08274e0 (#131) with `tools\build.ps1` and the DLL copied into the slot's plugins folder; the run's notes do not record the exact commit. Slot 11, test mod `D2RToolsTest`, test save `cartos` (Barbarian level 1, Normal), D2R 3.3 build 93847, D2RLoader 1.3.1-beta, `core-maps` moved out, vanilla `itemstatcost.txt` in the test mod, `PlayerStats=1`, one map `StatsMap` with Players rows `strength` +40 and `coldresist` +30. One run, single player. Levels: live game (this check); the unit tests and clean build of the same code ran before it.

| Check | Level | Result |
|---|---|---|
| `cartographer check` | live | `player stat state, bank 1 (232 states): state 39 would be picked`, the same for banks 2 and 3. |
| Row bytes | live | On entering the map: `statprobe: state 39 carries the player's list (every column of its row is empty): bank 3, 232 states, 429 skills; row bytes 2700ffff...`. The 68 bytes equal the predicted row above byte for byte (`27 00`, 14 x `ff`, 8 x `00`, `ff ff`, 18 x `00`, `ff`, 3 x `00`, `ff ff`, 6 x `00`, `ff ff ff ff`, 8 x `00`). So the live table holds what the `.bin` holds. |
| Totals on the server | live | `player 1 entered the map: totals [0=30 43=0] -> [0=70 43=30], list on chain: yes (3 -> 4 lists)`. |
| Character sheet inside the map | live, **works** | Strength 70 (shown in blue) and Cold Resistance 30% inside the map; Strength 30 and Cold Resistance 0% in town before. Dexterity, Vitality, Energy, Life, Mana and the other resistances unchanged. The attack damage read 1-2 in town and 1-3 inside the map (the Strength bonus). |
| After leaving | live | The town portal back gave `left the map: list removed (chain 4 -> 3 lists)`, and the sheet read Strength 30 and Cold Resistance 0% again. A second entry applied the same totals again. |
| Save and Exit inside the map | live | `cartos.d2s` is byte for byte the backup (SHA-256 equal). Its modification time did not change at the exit (last written at the game start), so this run does not show that the save writer ran with the list attached (the slot's loader log showed D2RLoader cancelling `cartos` saves with `encoding-error` earlier that day, which could also explain the unchanged file, and the run's own loader log was not kept); the earlier check in this section (before the state route) saw a rewrite without the +40. |
| Visible effect of state 39 | live | None seen: no overlay, icon, colour or light on the character in the map (screenshots of the run inside the map showed none). |
| Crash | live | No new crash file and no `[ERROR]` line in the Cartographer log (the loader log of the run was not kept). The 6 crash reports in the slot are the earlier ones. |

Result: Route A works for the player's own sheet in single player. Still not checked: a client that joins or comes into view later, other players' clients, more than 16 player stats, an autosave inside the map, what the game does with state 39 on death or a curse, monsters on the same state, and a second character.

**Dump scan for hard-coded states (review of PR #131, static).** Every `E8` call to `SetStateBit` `0x3354c0`, `StateOn` `0x3351b0`, `FindStateList` `0x2f5940`, `SetListState` `0x2f72f0`, `GetStateRow` `0x21b0c0` and `0x336280` in the dump (stamp `0x6ab3782c`) was taken with the last `mov edx, imm32` within 40 bytes before it. No call passes `0x27`, `0x74`, `0xe6` or `0xe7` to a state function; the known ones are found (`0x5c` at `0x448e4e`, `0x79` at `0x339e1e` and `0x339ded`). Four calls with `0x27` (`0x34a602`, `0x351443`, `0x3514a6`, `0x42f9c2`) go to `0x336280`, which bounds its argument to 1..`0x2a` and reads `[dt + idx*8 + 0x2c8]`, the per-flag bit arrays: its argument is a flag index and `0x27` is flag 39 `meleeonly`, not state 39. Limits: only about half of the call sites set `edx` by an immediate; `D2RCore.dll` is not in the dump; record-style callers (like `0x433d20` with `+0x6c = 0x38`) were not scanned. The run-time refusal rule stays the backstop.

### Issue #104 closing note (2026-11-12, desk work)

The issue's symptom (sheet unchanged) is fixed by Route A (PR #113, #131) and was seen working in game (second check above, PR #137): the stat list needs a state, because the game's sender ships a list only with its state. The one further change is the 16-entry cap (static, from the `0x2f64e0 ..., 0x10` call; the loader's replacement of that body was not read): `buildPlan` keeps at most 16 player stats and lists the rest as skipped, so the server totals and the sheet agree. Unit tested (`statplan_tests`), not seen in game. Still open and needing a game: a joining client, other clients in multiplayer, an autosave inside the map (the save writer confirmed to run with the list attached), death and curses with state 39, a second character. In-game check for the cap: a map with 17 Players rows, enter; the log plan shows 16 and one skipped line, and the sheet shows the first 16.

## 8. Outdoor builder engine checks (issue #83)

Static answers for the twelve open questions of plan section 7 ("Still to check, in Ghidra or in game") of
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md). Everything is read from the build 93847 dump
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

## 8a. Outdoor build hook (issue #141)

Code: `plugins/cartographer/src/engine/outdoor_hooks.{h,cpp}`, the pure plan in `src/outdoor/outdoor_plan.*`. Build 93847 (stamp `0x6ab3782c`). Unit tested (the plan, the cache, the seed arithmetic, the grid comparison) and built; the hooks themselves have never run in a game.

### Hook lengths (derived 2026-11-12)

Method: disassemble the part's entry with capstone from the 93847 dump. A patch must end on an instruction boundary and hold no relative operand (call, jmp, jcc, RIP-relative), because the loader copies the bytes to a trampoline. The "max patch" is the offset of the first relative operand; `tools/check_maze_signatures.py` now holds it per part, checks that the signature's bytes stop at or before it where it is a number, and checks that the hook lengths below end on an instruction boundary and are not over it.

| Part | RVA | Hook length | Max patch | First relative operand | Used as |
|---|---|---|---|---|---|
| `LevelBuild` | `0x3271c0` | 14 | 48 | `call 0x367110` | hooked once; shared with the maze hook (below) |
| `OutdoorAct5Builder` | `0x3fe2e0` | 16 | 37 | `jne rel32` (`0F 85`) | hooked; stamps the plan |
| `OutdoorEntry` | `0x3ee320` | not hooked (the spike hooked 19) | 90 | `call` | called |
| `PieceStamp` | `0x3ecfc0` | not hooked | 61 | `call` | called, once per stamp |
| `VoidCell` | `0x3ede00` | not hooked | 39 | `call 0x3faa50` | called, once per void cell |

### How it works

- One level build hook. `0x3271c0` is already hooked by the maze steering, so `LevelBuildHook` there calls `OutdoorLevelBuild` first. The outdoor build installs its own hook (14 bytes) only when the maze hook never tried (`MazeLevelBuildHookTried`); when the maze hook was tried and refused, the outdoor build stays off with a log line.
- `OutdoorLevelBuild` touches the Outdoor slot level only (`IsSlotLevel`: slots.txt `Outdoor` level id). Any other level, in any act, runs the game's own build.
- Preconditions, all read-only, otherwise the game builds its own level with the reason in the log: `OutdoorBuild = 1`, every part on, level type 3, drlg and the outdoor block (`level+0x18`) readable, no rooms yet, a map opened (`OutdoorMapOpened`), the slot row's OffsetX, OffsetY, SizeX, SizeY in `levels.txt`, `LevelType` equal to the tileset's (`OutdoorLevelType`), and a plan that passed `planStamps` and `checkStampPlan`.
- Then: write the rectangle (origin from the slot row, size from the plan; both are 0 for a slot when the build starts, section 4 S1), reseed (`[level+0x1e4] = int32(drlg[0x860]) + levelId`, `[level+0x1e8] = 0x29a`), call `OutdoorEntry`. Its Act 5 branch reaches our `OutdoorAct5Builder` hook, which, only when the thread's build context names this level, calls `PieceStamp(dataFlag, level, cx, cy, lvlPrest, file, 0)` per stamp and `VoidCell(level, x, y)` per void cell. Then the level type is set to 0 (no branch of the dispatch matches, so only the native tail runs, section 4 S1), the original `LevelBuild` runs, and type 3 is restored.
- Afterwards the built grid is read back (`+0x08` piece grid, `+0x58` flag grid through the row index) and compared with the plan (`compareGrid`); a difference is logged, not repaired. If the Act 5 branch was not taken (the slot level is not in Act 5 in `levels.txt`) the plan was not stamped, and the log says so.
- The plan is cached by (seed, tileset, shape, size, origin, limits, options), four entries, so the host's and a client's builds of one map share one computation. Failed plans are cached.
- `OutdoorProbe = 1` logs `[outdoor probe]` lines: the level before and after, the grid as characters, the differences, and the rooms with their pieces. Both settings default to 0.

### After our build, and what is not hooked when the setting is off

- **Second reseed.** The original's prologue (`0x3271e2..0x3271f0`) reseeds the level RNG again after our build returns control to it, so the level RNG ends at the fresh seed, not where a generator left it. The tail does not read it (section 8 Q5); the last fallback of `0x3da650` does. It is the same on the host and on a client.
- **`OutdoorBuild = 0` installs nothing.** `InstallOutdoorHooks` finds the parts (for `cartographer check`) but hooks neither `0x3fe2e0` nor, when the maze steering did not take it, `0x3271c0`, when the setting is 0 or slots.txt has no Outdoor slot (one log line says so). The game then runs as without the outdoor code; the setting is read at load, so a restart turns it on.
- **Act 5 only.** `PrepareSlot` refuses, before any write or engine call, unless the Outdoor slot row in levels.txt has `Act` = 4 (Act 5) and an id above 137 (a vanilla level or a town is never rebuilt). Otherwise `0x3ee320` would dispatch to a vanilla Act 1-4 builder (`0x3ee484..0x3ee4db`) on our level and rectangle. A missing `Act` column also refuses. The game then builds the level itself.

### Slot row requirements (found by the outdoor spike, PR #168)

- The Outdoor slot's levels.txt row needs `Vis0` to `Vis7` = 0. With `-1` the game crashes at image `+0x3268fb`. The installer writes the row (issue #144) and is not changed here; this is recorded for that change.

### Entry, boss and event rooms (issues #143 and #145): written, not verified in game

- After a build that matched the plan, `ArrangeRooms` reads the level's room list and reduces three plan cells to one room each with `chooseCellRoom` (the room wholly inside the cell nearest its centre; a cell is 32 x 32 tiles, a room 8 x 8): the entrance cell, the event cell and the boss cell. The S2 spike (PR #168) saw the arrival land in the entry cell only when the spike set `0x30000` on a room of it.
- Arrival: `planArrival` gives the entrance cell's room `0x30000` in room `+0x50` and clears the bits on every other room, as the maze `MarkArrival` does (same three guard parts `ArrivalFinder`, `ArrivalRoomFinder`, `ArrivalFlagTest`, checked only; a missing one leaves the arrival to the game). Refused, with the reason in the log, when the build is not the plan, a room lies outside the plan's level rectangle or the cell holds no room. `OutdoorProbe = 1` logs the player's first room in the slot level and whether it is in the entrance cell (`[outdoor probe] arrival`), driven from the same tick as the maze probe.
- Boss and event: the chosen rooms' exact DRLG rects are what `OutdoorBossRoom` / `OutdoorEventRoom` return; `event_apply.cpp` uses them for the Outdoor slot level exactly as it uses the maze's (the tracker compares the room rect), so the map Boss (the `bosses.txt` entry, one monster at the room centre) and the event go in that one room. No event cell: the event falls back to the distance rule.
- Not done: population per clearing. The S2 spike's clearing monsters came from rows cloned from Act 5 row 111 (the level's own monster data), so the layout does not decide them; the slot level's `Monsters`/`Density` (section 15) are the only control for now.
- Needs a game: the portal arrival without a manual mark, the Boss standing in the boss clearing (server room scale), and the event room firing there.

### Open

- The void marker is the `VoidCell` path (piece 0, flag `0x100`). Whether that leaves a walkable-free hole, or a fill ring is needed, is issue #142; the S2 spike (section 16) saw the void edge as acceptable in game (dark rock, no hole). The whole feature stays off until the builder itself is checked in game.
- The entry room: whether the plan's entrance cell is where the player arrives is also #142 (section 16: a portal arrival landed in the entry cell, with a room flag the spike set).
- A Jungle map opens through `portal.cpp` (#144, see "Opening an outdoor map" below); that path has not run in a game.
- `VoidCell` and `OutdoorEntry` were only ever reached by the spike (a hooked entry, direct stamp calls); calling `VoidCell` directly is untested.
- Nothing here has run in a game.

### Opening an outdoor map (issue #144)

A Jungle map opens through the Outdoor slot from its item (`portal.cpp`: `OutdoorProblem`, then `OutdoorMapOpened` before the portal, `OutdoorMapCancelled` if none opens). It needs `OutdoorBuild = 1` (the hooks are not installed otherwise). `tools/install.ps1` writes the level "Cartographer Outdoor": Act 5, DrlgType 3, LevelType 21, 384 x 384 at (6000, 3000), copied from level 111, Vis0..7 = 0, no lvlprest or lvlmaze row (none was needed by the spike, section 16). Desk work only; the open path has not run in a game. Multiplayer: the host sends the outdoor seed through map sync (see "Multiplayer layout sync" above). The installer sets `SubType` and `SubTheme` to -1 on the Outdoor row only (the Maze row is unchanged), so the Siege sub-level set of level 111 is not carried over. Before the portal opens, `OpenMapPortal` makes the plan (`OutdoorPlanProblem`, the same cached plan the build uses) and refuses with its reason if it fails, so the game's own Act 5 builder never runs on a Jungle-typed slot. After the slot level has been built with a map open (`OutdoorSlotBuilt`, a `SlotBuildState` like the maze's, cleared on leaving the game), or while it is being built, a second outdoor open is refused with a message. The installer's repair path for an existing "Cartographer Outdoor" row fixes Quest, LevelGroup, Entry, Warp, SubType, SubTheme and Vis (Act, DrlgType, LevelType, size and offsets are refused by the plugin at open instead).

### Multiplayer layout sync (issue #146)

Question: what does a remote TCP/IP client know when it builds the outdoor slot level, and can the plan be a pure function of that? Static, build 93847 (stamp `0x6ab3782c`), dump `D2RLoader_6ab3782c_dump.exe`. Not run in a game.

| Fact | Evidence | Level |
|---|---|---|
| The level reseed `0x367110` is `mov [rcx], edx; mov [rcx+4], 0x29a; ret`: the level RNG pair is only what the caller passes, with no time, thread or server read | capstone on the dump, `0x367110..0x367119` | static |
| The act seed reaches `drlg+0x860` the same way on both sides: act creation `0x2ef1c0` passes its third argument (`r8d`, via `ebp`) as `r9d` to `0x326ba0`, which reseeds the DRLG with it (`0x326e99`) and stores the first draw, `low32(seed * 0x6ac690c5 + 0x29a)`, at `drlg+0x860` (`0x326f03`). So `drlg+0x860` is a pure function of the act seed; the server passes `game + 0x12c`, the client the packet `0x03` seed (section 12, Q2). (The `call 0x367110` at `0x2ef3b3` and `[rsp+0x60]` are in a separate function starting at `0x2ef340`, which has no direct callers; they are not the act seed path.) | capstone on the dump: `0x2ef1c0`, `0x326ba0` (`0x326e99`, `0x326f03`), `0x2ef340`; section 12 | static. Closes the earlier "not re-traced" caveat; the act seed being equal on both sides is section 12, Q2 |
| `LevelBuild` runs on the host's server DRLG, the host's client DRLG and a remote client's DRLG, each against its own level record | section 12, Q2 | static |
| A remote client therefore has, when it builds the level: its own tables and the map sync message (maps row, slot level, opening, layout seed). It does not have the host's job id (`g_nextJobId`), the host's `seedSalt` (tick, player id, opening count) or any other host process state | `OutdoorMapOpened`, `MapSyncMapOpened` | code |

Decision: the stamps' file draws depend on nothing else. The level build still reseeds from `[drlg+0x860] + [level+0x1f8]` (`0x3271dc..0x3271f0`) after our stamps, and the native tail's last fallback (`0x3da650`) draws from that; those draws depend on the act seed, equally on both sides (previous row).

- The plan is a function of `MapRequest` = (tileset, shape, size, seed, origin, size limits, options, shapes.txt) and the built-in catalogue. Origin and limits are the slot row's `levels.txt` values and the rest the map's `maps.txt` row, so host and client agree when their tables agree (the same condition as the maze half). The job id and the salt only produce the seed on the host (`mixSeed(salt, id)`); a client takes the host's seed as it is (`OutdoorMapOpened(..., fixedSeed)`).
- The one other input was the level RNG seed, `int32(drlg[0x860]) + level id`, which draws each stamp's DS1 file when `pinFiles` is off. It now comes from the plan's hash (itself a function of the request) and the level id through `src/outdoor/outdoor_seed.*` (`slotRngSeedFor`), so it does not rely on the act seed being equal on the two sides. The second word stays `0x29a`. Section 8a "Second reseed" still holds.
- Wire: map sync wire version 2 adds the flag `kFlagOutdoor` (2), never together with the maze flag; the seed is the open outdoor map's seed (`OutdoorOpenSeed()`). The client applies it with `OutdoorMapOpened(map, tables, 0, seed)` from its own `maps.txt` row, and skips, logging the reason, when: the slot level or the map kind differs, the outdoor build is off or not live, the host sent seed 0, or the Outdoor slot level already started to build here (`OutdoorSlotRanAny`; no rebuild). Pure decision in `src/mapsync/mapsync.cpp` `decideApply`, unit tested.
- The host's call is in `portal.cpp` (`NoteMapOpenedForClients`): `MapSyncMapOpened(..., maze, maze ? MazeOpenSeed() : outdoor ? OutdoorOpenSeed() : 0, outdoor)`, with `outdoor = MapIsOutdoor(map)`. The client classifies a map with the same `MapIsOutdoor`, so both agree (a Jungle map's name still syncs).

Needs a game (deferred by the owner until the plugin is complete): a TCP/IP host and a remote client with identical tables open one Jungle map. Confirm (1) both logs show the same `outdoor slot level N: planned ... plan <hash>` line and `built as planned`; (2) the room lists (count and rect of each room, `OutdoorProbe = 1`) are equal; (3) the client log shows `the next build of the Outdoor slot level here uses the host's layout seed` before its first build of the slot level, and, if the client builds first, the skip line and the stock layout instead; (4) in a single-process host both DRLGs print one plan line (cached).


## 9. Map events: entry message, decision, Boss and Horde (issue #92)

**Status:** written and unit tested; signatures checked statically on the build 93847 dump (`check_portal_signatures.py`); **not run in a game**. Code: `plugins/cartographer/src/engine/event_apply.cpp`; pure logic (spawn name lookup, placement, event room choice, spawn plan, entry lines, banner timing) in `src/eventplan/`, the decision in `src/events/`.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer map events (`src/engine/event_apply.cpp`) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | `PopulateRoom` `0x50acf0` (hooked, `(game, serverRoom)`), `SpawnMonster` `0x492cf0` (called), `SpawnSuperUnique` `0x499ba0` (called). `UnitRoom` `0x34b440` and `RoomLevelId` `0x2efc10` are the portal's parts, found again as `EventUnitRoom` and `EventRoomLevelId`. Fields read: server room `+0x18` is the DRLG room, `+0x80/+0x84/+0x88/+0x8c` the server room's x, y, w, h (section 6, from `0x2f0680`); DRLG room `+0x60/+0x64/+0x68/+0x6c` x, y, w, h in tiles (section 1a, `ProbeRooms`); the game's once-per-game superunique bitmap at `game+0x1d8` (section 6) |
| Which hook | Section 6 Q3 names `0x48f190` as the common point and says it runs before the populator, so the spawn has to follow the population. This change hooks the **populator `0x50acf0`** after its original. It has exactly two callers (`0x4042e8` in the room activation `0x4042a0`, `0x40881c` in the per-game loop `0x408670`), once per room that is not yet populated, so one hook covers both activation paths and runs after population. Hooking `0x4042a0` and `0x408670` after their originals would also work, but twice, and for rooms populated earlier. Not `0x48f190`: it runs before population |
| Guard | `PopulateRoom`: 43 bytes, 8 masked; the 12-byte prefix (`4C 8B DC 53 56 48 81 EC B8 00 00 00`: `mov r11,rsp; push rbx; push rsi; sub rsp,0xb8`, whole instructions, no relative operand) alone matches 3 sites (`0x50acf0`, `0xfdd760`, `0x1b636d0`), so the signature runs on through the `mov rax,[rip+..]` (the first relative operand, at +12) to the `cmp [rip+..],0; mov rsi,rdx; mov rbx,rcx`, unique at `0x50acf0`. The hook cuts the first 12 bytes, which D2RLoader re-checks at the entry. `SpawnMonster`: the full 55-byte prefix of section 6, no `??`, unique at `0x492cf0` (the 29-byte short form also matches `0x492aa0`). `SpawnSuperUnique`: `40 55 53 56 57 41 56 41 57 48 8D AC 24 48 FF FF FF 48 81 EC B8 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4`, unique at `0x499ba0` (without the leading `40` it matches `0x499ba1`). A part that does not match once is logged and left off |
| Contract | At map open the event is decided with `events::decide` (the map's `EventChance` or the global one, the map seed of issue #92, which is per opening: `mixSeed(tick, player id, opening count)`, salted for the event stream). It is kept only if it can be spawned (Boss or Horde, `Spawn` found in `monstats.txt` `Id` or `superuniques.txt` `Superunique`, a Class that is in monstats, the spawn parts on). Each populated room of the slot level goes to `EventRoomTracker`: a maze map's event room is the plan's event cell (the DRLG rect recorded after the slot build by `maze_hooks.cpp`, `MazeEventRoom()`); a preset map (or a maze map whose event room is not known) takes the first populated room as the entry room and the first later room whose centre is at least `eventplan::presetEventDistance(levelW, levelH, entry, rooms)` from it (Chebyshev). That is `min(minEventDistance(levelW, levelH), the farthest room's distance from the entry)`, where `minEventDistance` is a third of the level's larger side, 1 to `kMinEventDistance` = 24 (24 when the size cannot be read), and the farthest room comes from a walk of the level's room list (`maze_hooks.cpp` `LevelRoomRects`, issue #121); at least one room always qualifies. If the list cannot be read, or holds no other room, it is `minEventDistance` alone. The level size is read from the DRLG room's level record (room `+0x90`, level `+0x2c/+0x30`, static, section 5; the level record's size is the `levels.txt` row's: `0x327650` copies SizeX to `+0x2c` at `0x32769a` and SizeY to `+0x30` at `0x3276a9`, so a mod that edits the slot row changes the distance without changing the map); the choice and the size are logged as `eventprobe: level L is W x H tiles and has N room(s); the event room is the first room at least D tiles from the entry room (..)`, or, when the list cannot be read, as a warning `eventprobe: the room list of level L could not be read; level is W x H tiles, the event room is the first room at least D tiles from the first one`. **Banner (issue #115):** the entry banner shows the map's name only; the event's Name and Message are added once the event room is chosen and something spawned (a banner follows at once if the player is already inside, else the entry tick includes it), a Shrine counts as spawned only when `CreateObject` returned a unit, so an event that cannot spawn is never announced. A banner that follows at once shows only the event lines (no repeated map name). Written, not verified in game. At that room, after the original returned, a Boss spawns its monster (a superunique through `0x499ba0`, unless its once-per-game bit is already set, then its Class as a plain monster) at the room centre and `Count` copies of its Class around it; a Horde spawns `Count` copies (a superunique as its Class). Spots come from the **server room rect** at `+0x80` (the rect the spawn search and the point-in-room test `0x2f0680` use, so its units are the spawn code's): the centre first, then a grid two tiles apart and two tiles in from the edge, where a tile is `server.w / drlg.w` server units (5 when it is subtiles, as `spotScale` computes; 1 if the rects are equal). No tile-to-subtile conversion is assumed. Every monster is `SpawnMonster(game, room, x, y, monstatsRow, 1, 0xa, 0)`, the arguments the superunique code itself uses, flags 0 so the game's normal path (and with it the map stats hook, section 7) runs; a superunique is `SpawnSuperUnique(game, room, x, y, row, 0)`. Once per map open; state is cleared on `GameLeft` with a generation counter. The `Drop` column is not applied in 6a. **Spawn names:** since issue #95 `Spawn` is resolved at map open from the game's loaded tables (section 11) and only falls back to the active mod's `monstats.txt` (`Id`) and `superuniques.txt` (`Superunique`, `Class`) when that route is off. In the fallback the row index is the parsed row count, **assumed** to be the game's row number: `ParseTsv` drops blank lines, so a file with one would shift every later row (needs game item 8). With the fallback, a missing `monstats.txt` leaves no Boss or Horde events; a missing `superuniques.txt` leaves only the superunique names unresolved (plain monster events still spawn). `cartographer check` and the report say which. **Threads:** the populator hook (spawn), the map-open decision (called from the portal's map open) and the player tick (entry message arming) run on the game thread; the banner draws on the overlay thread and reads a copy of the lines under `g_bannerLock`. **Superunique edge:** the game skips its once-per-game bitmap when the row's `+0x29` is set or the global `DAT_142aa6a74` is non-zero (`0x499c5e`); the plugin's own `superSeen` pre-check does not, so such a repeatable superunique spawns as its Class after the first time, though the game would have allowed it. Harmless, and not fixed |
| Entry message | The SDK route: the Overlay service (`registerFrameCallback`, phase `AfterGameUi`, `measureText` and `drawText`), fading in, holding and fading out over about six seconds; the lines are the map's `Name`, then the event's `Name` and `Message` when an event fired and is applied. The overlay draws on the local client only. Without the service (or if drawing fails) the lines go to the console (`WriteConsoleMessage`). It is shown from the player tick, once each time the local player's room level becomes the slot level. The chat packet fallback `0x477a30` is not used: the local routes need no network, and the packet route would only matter for multiplayer sync, which is out of scope |
| Evidence | `dump_dis.py ... dis 0x50acf0 / 0x492cf0 / 0x499ba0` (prologues), `pat` for the uniqueness of each signature, `check_portal_signatures.py` (all three rows ok on the dump); the callers of `0x50acf0` and the argument registers from section 6 |
| Validation | Unit tests (`eventplan_tests`: name resolution, placement, event room tracker, `farthestRoom`, Boss and Horde spawn plans, the superunique bitmap bit, `minEventDistance`, entry lines, banner line gating, entry tracker, banner alpha), signature check on the dump, a clean Release build. **Not run in a game** |
| Failure mode | The populator hook goes in at load only when `PopulateRoom`, `SpawnMonster` and `EventRoomLevelId` were all found; otherwise no event is spawned or announced, and the map still opens with its stats and its entry message. A missing `SpawnSuperUnique` spawns a boss as its Class. A map whose Spawn cannot be resolved logs why and has no event. A hook that D2RLoader installs without an original is logged as an error and leaves room population skipped until the game is restarted (the same hazard as the other hooks here) |
| Coexistence | A new hook on `0x50acf0`, which nothing else here hooks; the original always runs first. Spawned monsters pass `0x494a60`, so the map's monster stats reach them |

### Needs game (settled by the `eventprobe:` log lines)

1. **Units of the server room rect.** Each populated room of the map level logs `eventprobe: room #n of level L populated: DRLG (x,y) w x h tiles, server (..) ..., k server unit(s) per DRLG tile`. Spawn spots are placed directly in the server rect (by static reading the spawn code's own units, from `0x2efb40`), scaled by that k; check that the spawned monsters stand in the event room, and that k is 5 (subtiles) or 1. The log now says why k is what it is ("the rects are the same size", "the server rect is a whole multiple", or "NOT a whole multiple, so 1 is a fallback"), because a 1 alone is ambiguous.
2. **Event room.** Maze: `eventprobe: maze event room (plan room n) is DRLG room ..., rect (..)` after the build, then a populated room with the same rect logs `event room from the plan` and `THIS IS THE EVENT ROOM`. Preset (issue #121, written, not verified in game): at the first populated room (the entry room) the level's room list is walked through the guarded walker (`maze_hooks.cpp` `LevelRoomRects`) and the event distance is `min(minEventDistance, Chebyshev distance of the farthest room from the entry room)` (`eventplan::presetEventDistance`; `farthestRoom`: largest Chebyshev distance between centres, then largest squared distance, then list order; the entry's own rect is skipped). The tracker fires on the first populated room at or past that distance, so the event lands where players walk and not only in the farthest dead end, and the farthest room always qualifies. The log shows `eventprobe: level L is W x H tiles and has N room(s); the event room is the first room at least D tiles from the entry room (..)`, then `event room by distance` and `THIS IS THE EVENT ROOM` at the room that fires; check that it is far from the portal. If the list cannot be read a warning says so, and if it holds no other room the log shows N = 1; either way the distance is `minEventDistance` alone: the first room at least `minEventDistance` (a third of the larger side, 1 to 24) from the entry, which on the 20 x 28 test map is 9 and has a one-tile margin (issue #115). The room list offsets, all read on the build 93847 dump: level `+0x10` first room and room `+0x48` next, in `0x3275b0` (`mov rbx,[rbx+0x10]` at `0x3275fa`, rect test on `rbx+0x60`, `mov rbx,[rbx+0x48]` at `0x327622`), and `0x360ba0` adds a room at the head (`mov [rdx+0x48],rax` / `mov [rcx+0x10],rdx`); the DRLG room `+0x60/+0x64/+0x68/+0x6c` rect is the one the tracker already used (section 5). The walk is bounded (4096 rooms) and every read is checked with `VirtualQuery`; an unreadable room abandons the walk. Whether a rebuilt level (leaving and re-entering) populates the same rooms, and that the event does not spawn twice.
3. **Spawns.** `event '..' (Boss): spawned N of M ... (k as a superunique, f refused)`. Every monster shown in play with the right class, level and the map's stats; the boss at the room centre with its minions around it. `superunique row R at (x,y): returned P, once-per-game bit now set` settles the return value and the bitmap mask (inferred as `1 << (row & 7)`). A second map with the same superunique in one game should log `already spawned this game` and spawn its class. `SpawnMonster`'s seventh argument is a free-spot search radius, not a mode (item 4), so `0xa` stays; check that ordinary monsters spawned with it look right.
4. **Position validity.** `SpawnMonster`'s p7 (`0xa`) is the engine's own free-spot search radius, not a mode (static: p7 goes to request `+0x2c`; `0x493cc0` at `0x493f13..0x493f34` searches only the exact point when p7 is negative, and with p7 >= 0 rings out from 3 to `p7*3` subtiles for a free position inside the room rect from `0x2efb40(room)`, server room `+0x80`). So `0xa` turns the search on (about 30 subtiles) and `-1` would turn it off and make walls and stacking worse; `FindFreeSpot` is not used. Check that monsters do not stack on walls or each other. The ring search can put a unit in a neighbouring room that is not yet active; the game's own packs share that risk, and the check is that nothing crashes or sits unreachable.
5. **Entry message.** The overlay banner appears on arrival through the portal (map name, then the event name and message), fades out, and appears again on re-entering the level; the console fallback when the overlay service is absent. It must not show in other levels.
6. **Rate.** Over many map opens the share with an event is near `EventChance`, and the row drawn follows `Weight`.
7. **Leaving the game.** After `GameLeft` and a new game there is no banner and no event from the old map, and no spawn in a level that was not opened.
8. **Row index of the `.txt` fallback.** When the game's tables are not used, the `monstats.txt` / `superuniques.txt` row index is the parsed row count; check it against the game's row for a mod whose file has a blank line (the `spawn '..': monstats row N from the game's table` line gives the game's own row to compare with).

### Game checks of 2026-11-12 (issue #92)

Live, slot 11 (test mod D2RToolsTest, save `cartos`), build 93847, D2RLoader 1.3.1-beta; one operator run on the Barbarian in `cheat god` after one death. The map was the slot level 138 (a copy of Colossal Summit, 20 x 28 tiles), opened with `cartographer open`. The event chance was 100 percent, so the rate (item 6) was not checked. Not checked on Steam build 94995. Item numbers are those of "Needs game" above.

| Item | Result |
|---|---|
| Decision | Logged with the seed: `event for 'EvMapA' (seed 17327224837595650, chance 100%): Boss 'Game Boss' (Bishibosh) minions 4 at (0,0)`. |
| 1. Server rect units | Every room logs `5 server unit(s) per DRLG tile: the server rect is a whole multiple`; the event room was DRLG (5000,2024) 8 x 4 tiles, server (25000,10120) 40 x 20. The monsters stood in it (screenshot after `cheat goto 138,25020,10130`: about 10 Fallen next to the portal). |
| 2. Event room | Preset: **never fires at the committed distance 24** in this 20 x 28 tile map (no room is 24 tiles from the entry room by Chebyshev distance), so no event spawned in the first run. With the distance lowered to 12 for the check (scratch build, not committed) room #4 was chosen (`event room by distance: THIS IS THE EVENT ROOM`) in every run. The shipped maps are larger, but a small preset map got no event; filed as #115 and fixed in code by scaling the distance to the level size (written, not verified in game). The maze event room from the plan was not run. A rebuilt level was not checked. |
| 3. Spawns | Boss: `spawned 5 of 5 in the event room (25000,10120) 40 x 20 (1 as a superunique, 0 refused by the game)`, then `superunique row 0 at (25020,10130): returned 000000016EA9D830, once-per-game bit now set`. Horde of 3: `spawned 3 of 3 ... (0 as a superunique, 0 refused)`. The class (monstats row 58, `fallenshaman1`) matches the table. The map's stats reached them (statprobe: `maxhp 1280 -> 2560`, `list on chain: yes`). The bitmap mask `1 << (row & 7)` was checked for row 0 only. A second map in one game (`already spawned this game`) was not run. |
| 4. Position validity | Radius `0xa` kept; monsters did not stack on walls or each other in the screenshots (one view, not a stress run). |
| 5. Entry message | The overlay banner logs `entry message shown on the overlay (3 line(s))` with the map name, the event name and the message, and the draw calls return `Success` for every line. The banner text did **not** show in the harness screenshots (`PrintWindow` does not capture the overlay), so what it looks like on screen, and the fade, were not seen. Re-entry and other levels were not checked. |
| 7. Leaving the game | After Save and Exit and a new game, `cheat goto 138` shows no monsters, no banner and no `eventprobe` lines (the level had no event because no map was opened). |
| 8. txt fallback | Not exercised: with the mod's `superuniques.txt` removed for one game the route stayed on (`superuniques row 0 from the game's table`) and the Horde spawned as before, which shows the game's route does not need the mod's file. The txt route itself (route off) was not forced. |

Fix made to run these checks: see section 11, "Game checks of 2026-11-12" (the monstats `Id` is at row `+2`, not `+0`).

### Map Boss (issue #152)

**Status:** written and unit tested (`eventplan_tests`: `planMapBoss`, the tracker on the plan's boss rect); **not run in a game**. No new address: it reuses the populator hook `0x50acf0`, `SpawnMonster` `0x492cf0` and `SpawnSuperUnique` `0x499ba0` of this section, with their guards and their per-part off switch (the populator hook needs `PopulateRoom`, `SpawnMonster` and `EventRoomLevelId`; a missing `SpawnSuperUnique` spawns the boss as its Class). If those parts are off the map Boss is logged and left off; the map, its event and its stats still work.

- **What:** the `Boss` column of `maps.txt` (a monstats `Id` or a superuniques name, resolved like an event's `Spawn`, section 11) is spawned once per map open: one monster at the centre of the boss room, no minions and no `Drop` (those belong to the Boss event). It is armed at map open whether or not an event was drawn. Flags 0, so the map's stats reach it.
- **Where:** a Cave or Crypt maze: the plan's boss room, the far cell of the main path (`Plan::bossRoom`, `Role::Boss`). `maze_hooks.cpp` records its DRLG rect after the slot build next to the event room (`MazeBossRoom()`, log `eventprobe: maze boss room ...`) and the populate hook fires an `EventRoomTracker` on that rect. A layout with `BossRoom` off has no boss room: the map Boss is left off, with a warning. There is no Preset code, since Preset is being removed.
- **Boss and Boss event together:** both are kept. The event goes in the event room (partway along the main path), the map Boss in the boss room (the far end); they are different cells of the plan. A superunique that both name spawns once per game, then as its Class.
- **Dedicated boss entries (issue #170):** `Boss` no longer names a vanilla boss. It names a `bosses.txt` entry; `tools/install.ps1` adds one `superuniques.txt` row per entry keyed `cartoboss:<Name>` (Class = the entry's Base, Mod1-3, MinGrp/MaxGrp = Minions, TC copied from the Act 5 Super C classes), found through the same name map and spawned through the same `0x499ba0` path as before. No new address. If the name map has no such row the Base is spawned as a plain monster, and if `0x499ba0` returns null `SpawnOne` already spawns the row's Class plain; the log says which. Unit tested (`tables_tests`); the installer was run twice on a copy of the vanilla tables (second run changed nothing). **Needs game:** the row is found by the name map, the shown name, mods and minions, and that the spawn returns a unit. **Review follow-ups (fix cycle 1):** a `Base` whose monstats `boss` column is 1 is refused (plugin and installer); the installer checks `Base` against `monstats.txt` and `Mods` against `monumod.txt` before writing and rebuilds its `cartoboss:` rows each run. **Open (needs game or RE):** `0x499ba0` reads and sets the once-per-game bit at `[game + 0x1d8 + (row >> 3)]` (`0x499c6d` read, `0x499d97` write) with no bound except the table row count, and the dump does not show the bitmap's length. Vanilla needs 9 bytes (66 rows); a row numbered 66 and up writes past that. Classic D2 reconstructions use a 64-byte list (rows below 512), which is inference only. Until it is known, `bosses.txt` is capped at 16 entries (`MaxBosses` in `tables.h`, `$MaxBosses` in `install.ps1`; 16 rows is 2 more bytes, row 81 at most). Boss `Name` (superuniques loader `0x3a1d20`, converter `0x308be0` to a string index at table load): done by a registered key, see the #174 note below (needs game).
- **Boss names (issue #174), route chosen (desk only):** the row's `Name` cell is now `cartographer:boss_<Name>` (installer; `Superunique` stays `cartoboss:<Name>`, which is what the plugin finds the row by, so the shown name never affects the lookup). The plugin registers a strings file `data/local/lng/strings/d2rloader/cartographer/bosses.json` with one record `boss_<Name>` per `bosses.txt` entry (display text = the Name, all 13 languages, same text rules as the map names) through `ResourceService::registerResource`, in `D2RLoaderLoadPlugin` right after the first table load and before any other part (`src/engine/boss_names.cpp`, pure `BossStringsJson` in `src/mapname`, unit tested). A table reload re-registers when the names changed. Stale keys: the registration is rebuilt from the current `bosses.txt`, and the installer rebuilds its rows, so a removed entry loses both. Vanilla rows and strings are never touched. **Key format (fix cycle 1):** the local key is `boss_` plus the Name with every byte other than A-Z a-z 0-9 written as `_` and two uppercase hex digits (`Shoreline Warden` -> `boss_Shoreline_20Warden`), the same function in `src/mapname` (`BossLocalKey`) and `tools/install.ps1` (`Get-BossLocalKey`), so the strings file and the table cell are ASCII whatever the bosses.txt encoding; one-to-one, no spaces or colons. **Follow-ups (issue #186):** the superuniques row key is escaped the same way (`cartoboss:Shoreline_20Warden`; plugin `BossKey`, installer `Get-BossEscaped`; `EscapeKeyBytes` in `core/text.h`), so a non-ASCII Name never puts a non-UTF-8 byte in a table cell; the lookup (`0xa121f0`) is unchanged and a miss still fails open to the plain Base. The installer trims the Name and Base cells of only space, tab, CR and LF (not .NET `Trim()`, which also strips U+00A0, U+0085, ``, ``), like `Trim` in `core/text.h`. Installer run twice on a scratch copy of the vanilla 3.3.0 tables with a Latin-1 `bosses.txt` (`René the Lost`, `Nbsp `, ` Lead`): rows `cartoboss:Ren_E9_20the_20Lost`, `cartoboss:Nbsp_A0`, `cartoboss:_A0Lead`; second run changed nothing. A Latin-1 Name is still worth one game check.
  - **Why this route.** `LOCALIZATION.md` (SDK pin 717f727): a table cell `namespace:Key` selects that package's string, and plugins own the namespace equal to `PluginInfo.id`; the loader assigns the two-byte references when the table loads. A mod-side strings json was rejected: a mod's `namespace:Key` record is an override and needs an existing target, a mod namespace needs an opt-in in `metadata.json` (and rebuilt BINs), and native loading needs numeric ids and whole-file copies of D2R files. The MapNames route (#124) cannot be reused: a superuniques row has no inline key text to overwrite at run time, and its `Name` is resolved once at table load (loader `0x3a1d20`, converter `0x308be0`, build 93847).
  - **Evidence:** installer run twice on a scratch copy of the vanilla 3.3.0 tables (`levels`, `lvlmaze`, `superuniques`, `monstats`, `monumod`) with the example `bosses.txt`: the first run wrote one row `cartoboss:Shoreline_20Warden` with `Name` `cartographer:boss_Shoreline_20Warden` (escaped form, see Key format); the second run changed nothing (file hashes equal). `tools\build.ps1 -Plugin cartographer -Test` passes.
  - **Needs game (not verified):** (1) that D2RLoader has the plugin's registered strings when it compiles or loads `superuniques.txt` (the plugin registers during its own load; the order against the table load is not documented). If it is later, the cell would read as an unknown key. (2) A mod that ships compiled `.bin` tables needs them rebuilt after the installer; the offline compiler may not see a runtime-registered plugin key (it says it keeps references to other packages for validation only when it has the complete set). (3) Mixed case in the key (`boss_Shoreline_20Warden`) resolving as a table cell key (the escape leaves no space or colon). Check: run the installer, restart, `cartographer check` ("Boss names: 1 registered"), open the map and read the boss's name over its head and on the health bar. If it fails, the fallbacks are a mod string file with the mod's own namespace, or a plain numeric-id native record.
- **Outdoor:** waits on issue #145. The hook point is in `OnPopulated` (`event_apply.cpp`), where `bossPlan` is read from `MazeBossRoom()` for a maze; an outdoor map reads its stamp plan's boss rectangle there, and the tracker and `SpawnMapBoss` are unchanged.
- **`Monsters`:** not part of this change. The monster pool is written into the slot level's region record, with Density and Uniques (and Level in the levels row), in issue #156 (section 15).
- **Needs game:** `eventprobe: the map Boss 'X' goes in the plan's boss room`, then `room #n ... is the boss room` and `map Boss 'X': spawned in the boss room`; the boss stands in the far room with the map's stats; with a Boss event on the same map both appear in their own rooms.

## 10. Red portal tooltip and area name (issue #90)

Static answers for issue #90: show each map's own `Name` on the red portal tooltip and as the area name inside the map, instead of the slot
level's string ("Colossal Summit"). Read from the build 93847 dump with `dump_dis.py` (Capstone); nothing was run in a game and no code changed.
Each claim is marked **static** (read from bytes or tables), **inference** (follows from static facts, not observed) or **needs game** (with the
exact check). The portal is opened by `portal.cpp` through `OpenRedPortal` `0x5da800` with a level id only; no string is passed (static).

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #90): per-map name on the portal tooltip and in the map; no hook exists yet |
| Module | `D2R.exe` (`0x3e2b736` and `0x3e2b730` are D2RLoader `FF 25` thunks into `D2RCore.dll`: call, never hook) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` (same as sections 1a to 8) |
| Location | RVAs in the table below. Levels row: `0x18c` bytes, vector at `tables + 0x1460` (count `+0x1468`), `tables` from `0x300a90(dataFlag)`. String fields: Waypoint at `+0xec`, **LevelName `char[40]` at `+0xfd`**, LevelWarp `+0x125`, LevelEntry `+0x14d`. LevelName is a text key (a string), not a numeric id (static: loader descriptor table `{+0 name, +8 type, +0xc size, +0x10 offset}`). Objects row stride `0x168`, table `+0x1528`, SubClass `+0x127` (bit 4 = portal; objects 59 and 60 have SubClass 4 in the 3.3.0 `objects.txt`) |
| Guard | None added: no signature is written to `part_signatures.h`. Each masked prefix below was checked unique on the dump (a byte search with `??` as a wildcard, the same test as `pat`). Where a short prefix matched siblings it was extended until it matched once |
| Rule | The 14-byte rule of section 6 (a 14-byte patch at the entry, rounded up to whole instructions, must contain no relative operand): "hookable" passes, "call-only" does not. No hook is proposed or written here |
| Evidence | `dis <rva> [count]` for each function; `pat "<hex>"` for uniqueness; a scan of `E8` targets for caller counts |
| Validation | Static only. Not run in a game; not checked on Steam build 94995 |
| Failure mode | Not applicable (no code). A future hook would fail open: if the prefix does not match exactly once, leave the names alone |

### Parts

| RVA | Role | Masked prefix (1 match each) | Hook class |
|---|---|---|---|
| `0x32c4a0` | Levels row getter `(dataFlag, levelId) -> row*`; 46 callers. Three sibling getters (`0x331e50`, `0x331f50`, `0x332050`) share its first 46 bytes, so the prefix is extended to 82 bytes | `48 89 5C 24 08 55 56 57 48 83 EC 30 48 63 DA 0F B6 F1 E8 ?? ?? ?? ?? 48 8B E8 85 DB 7E 34 40 0F B6 CE E8 ?? ?? ?? ?? 48 8B B8 68 14 00 00 4C 63 C7 4C 3B C7 75 04 85 FF 79 14 48 8D 4C 24 58 C6 44 24 58 00 E8 ?? ?? ?? ?? 84 C0 74 01 CC 3B DF 7C 18` | hookable (15-byte cut ends at `48 63 DA`; first relative operand at `+0x12`) |
| `0xc7a30` | LevelName text getter: `E8` to the row getter, then reads the key at `row + 0xfd`; 11 callers | `48 83 EC 38 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8D 88 FD 00 00 00 80 39 00` | call-only (relative `call` at `+4`) |
| `0xc17e0` | Object hover text builder `(out buf, unit)`; the portal tooltip is built here. One caller, `0xc0916`, inside `0xc0420`. `.pdata` continues at `0xc182a` (the portal branch is in the continuation chunk) | `40 55 56 41 56 48 8D AC 24 20 FF FF FF 48 81 EC E0 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 00 00 00` | hookable (20-byte cut, no relative operand) |
| `0xc0420` | Unit label builder, calls `0xc17e0`; 3 callers, all in `0x880578` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D AC 24 10 F6 FF FF 48 81 EC F0 0A 00 00` | hookable (15-byte cut) |
| `0x14751e0` | Automap info text builder (string "DesecratedAutomapLevel"); no direct `E8` caller (called indirectly; inference) | `4C 8B DC 55 53 57 49 8D AB 28 F9 FF FF 48 81 EC C0 07 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4` | hookable (20-byte cut, no relative operand) |
| `0x34ad40` | Portal destination level id (loader-widened to 16 bits). It loads `objData = [unit + 0x10]` at `0x34ad7a`, then `jmp 0x3e2de40` into D2RLoader code, which returns `objData[+0x08] \| objData[+0x78] << 8` does `add rsp, 0x20` (the trampoline's own stack adjustment) and jumps back to `0x34ad86` (`pop rbx; ret`); the caller does `movzx edx, ax` (static). The prefix is unique only because of the loader's `E9` byte (`E9 rel32` after `48 8B 43 10`): the same bytes without it also match `0x34acf0` and `0x34adf0`. So this part is loader-patched and fragile: the key may shift with the loader version, and an unpatched `D2R.exe` would not match | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 01 CC 83 3B 02 74 14 48 8D 4C 24 30 C6 44 24 30 00 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B 43 10 E9` | call-only (relative `jne` inside the 14-byte cut; loader-patched tail) |
| `0x38fd00` | Objects row getter `(id)`, stride `0x168` | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30 8B DA E8 ?? ?? ?? ?? 48 8B F0 8B FB 48 3B 98 30 15 00 00 72 14` | hookable (15-byte cut ends at `48 83 EC 30`) |
| `0x1471bc0` | Party info text builder (strings "PartyInfoText", "LocationText"); calls `0xc7a30` at `0x1471dcd`. Not in the earlier table; checked here | `48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E0 FE FF FF 48 81 EC 20 02 00 00 0F 29 B4 24 10 02 00 00` | hookable (14-byte cut ends at `push r14`, no relative operand) |
| `0x1e9b40` | Waypoint panel builder (not a path for the portal) | `48 89 74 24 20 55 57 41 54 41 55 41 56 48 8D AC 24 80 FD FF FF 48 81 EC 80 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4` | hookable (21-byte cut: the `lea rbp,[rsp-0x280]` starts at `+13` and is 8 bytes; no relative operand) |
| `0x3e2b736` | Text lookup by key (`string_view*`), thunk into `D2RCore.dll`; 1217 callers | thunk (`FF 25 rel32`), not signable | call-only |
| `0x3e2b730` | Text lookup by numeric id (`0x150d` is the missing-text id), thunk into `D2RCore.dll` | thunk, not signable | call-only |

### Q1. Where does the red portal tooltip get its text?

- **Static.** In `0xc17e0` (continuation chunk at `0xc182a`). `0xc186a` calls the objects row getter `0x38fd00`, `0xc186f` reads SubClass `+0x127` and `0xc1876` tests bit 4. When it is set, `0xc1882` calls `0x34ad40`, which returns the destination level id (loader-widened to 16 bits: `objData[+0x08] | objData[+0x78] << 8`, `objData = [unit + 0x10]`; see Parts), and `0xc188e` gets the levels row from `0x32c4a0`. The LevelName key is read inline at **`0xc189f`** (`lea rcx, [rax + 0xfd]`, only when the row is not NULL; a NULL row takes `0x3e2b730(0x150d)` at `0xc18e5`). The key is looked up through `0x3e2b736` (`0xc18c6`) and copied with `0x1227840` into the output buffer (`0xc18f5`).
- **Owner suffix (static).** After the copy, `0xc18fa` calls `0x8d4b0` (true when the global at `0x2a233f4` is zero: `cmp dword ptr [rip + 0x2995f3d], 0` at `0x8d4b0`); if it returns true the builder jumps to the end and appends nothing. Otherwise (what the global means was not read; probably the multiplayer or online state, inference) it takes a name from `0x136670(unitId)` (a lookup in a global linked list by unit id at `+0x80`/`+0x84`), falling back to `0x1cb060(unit)` (`objData + 0x34`, the owner name stored on the portal, optionally passed through `0xa9bb0`), copies it with `0x1227840` into a 128-byte buffer, formats it with `0xbf7b0` and the format string `" (%s)"` into `[rsp+0x50]`, and appends it at `0xc1a95`. So a second part `" (<owner>)"` follows the level name when that global is non-zero. What the player sees in single player stays for the game (needs-game item 4).
- **Other branch (static).** SubClass bit `0x40` starts at `0xc19b3`: `0x34b440` gets the unit's room, `0x9a1b0(unit)` (`0xc19c7`) returns a text that `0xc19f9` copies into the output through `0x1227840`, `0x2efc10` gets the room's level id, `0x32c4a0` the levels row, and it appends two fixed key strings (5 and 7 characters) and then the LevelName key at `row + 0xfd` (`0xc1a5c`, looked up at `0xc1a8d`). This is **also a LevelName reader**, keyed on the level the object sits in: such an object inside the slot level would show the slot's name too. Bit 7 set (`js 0xc1b43`) skips the text.
- So yes, the portal tooltip is the target level's LevelName lookup, but it does **not** go through the LevelName getter `0xc7a30`. A hook on `0xc7a30` would not change the portal tooltip.
- **Callers of `0xc7a30` (11, static, classified by the enclosing `.pdata` function and its strings).**

  | Call site | Function | What it is |
  |---|---|---|
  | `0x147535b` | `0x1475301` (chunk of `0x14751e0`) | automap info text (string "DesecratedAutomapLevel") |
  | `0x1471dcd` | `0x1471bc0` | party info text ("PartyInfoText", "LocationText") |
  | `0x13b11fa` | `0x13b11e1` (chunk of `0x13b1100`) | progress and location list printer, level kind |
  | `0x1ea1a6` | `0x1e9b40` | waypoint panel builder |
  | `0x8e1c9` | `0x8e010` | script data model exporter ("Game.GameType", "Game.LevelId", "Game.Level", "Game.Room", "Game.Position.X"...): it calls `0xc7a30` for the unit's level id (`0x2efc10`) and copies the name into a string (`0x76210`, `0x12d3660`). The destination field was not followed. **This is the one that might feed a layout or banner** (inference: a "Game.*" data model could be what a layout binds to); needs game |
  | `0x165992` | `0x1656c0` | debug warp text ("Warp Pos: ...", "DRLG ID: %lu", "Mode: %s") |
  | `0x7c03a7` | `0x7c0305` (chunk of `0x7bfec0`) | no strings in the chunk; its neighbours are the Desecrate debug windows, so probably the same family (inference) |
  | `0x7c23aa` | `0x7c22b0` | Desecrate debug window ("%s (%d)", "DesecrateDiff#BaseLevel") |
  | `0x7c315e`, `0x7c31f7` | `0x7c2df1` (chunk of `0x7c2a00`) | Desecrate debug window ("Spawn Herald", "Zone ID", "Waypoint Level"...) |
  | `0x7c4632` | `0x7c4557` (chunk of `0x7c4470`) | Desecrate debug window ("Area", "Selected: %s") |

  None of these shows the entry banner text by its strings; `0x8e1c9` is the only one that could carry a current-level name to a layout, and it was not traced further.
- That the red portal object takes the SubClass-4 branch is inference (objects 59 and 60 have SubClass 4 in `objects.txt`; `portal.cpp` opens the portal through `OpenRedPortal`, which was not traced to its object id). **Needs game:** count hits on `0xc189f` while hovering the red portal.

### Q2. Where does the area name shown on entering a level come from, and the automap title?

- **Automap title and party text: static.** `0x14751e0` (automap info, call at `0x147535b`) and `0x1471bc0` (party location, call at `0x1471dcd`) call `0xc7a30`, so the LevelName getter is where their text comes from. `0xc7a30` is call-only (relative `call` at `+4`), so it cannot take an entry hook; reaching these two means hooking their own entries (both hookable; `0x14751e0` has no direct caller) or changing what `0xc7a30` reads (the row key, Q4 option 4). Both are inference; no hook is proposed. The progress and location list printer `0x13b1100` prints a group (kind 0) or a level (kind 1) from the levels row and the level group row (`0xc7a00` / `0x32c5a0`, table `tables + 0x1480`), also by key.
- **Area name banner on entering a level: not found.** No C++ reader of `row + 0xfd` outside the functions above turned up (scan of every caller of the row getter for `0xfd]`). The banner is probably driven from the layout or from `D2RCore.dll` (inference). The data model exporter `0x8e1c9` (Q1 callers table) is the one `0xc7a30` caller that might be involved. **Needs game:** enter the slot level with a log on `0xc7a30` and on `0x3e2b736` calls; if neither fires when the banner shows, the banner is in D2RCore and only a text replacement can change it.

### Q3. Does the server or the client know the target? Is a client-side hook enough in single player?

- **Inference** from the static reads: the tooltip builder reads the destination level id from the object data (`0x34ad40`: `objData[+0x08] | objData[+0x78] << 8`, with `objData = [unit + 0x10]`) and then the levels row itself; no server text is involved. The client therefore decides the text, and in single player the client and server share the process, so a hook in the client builder is enough. Nothing in the dump suggests the text is checked against the server.
- The slot level id is the same for every map, so the text cannot be told apart by level id alone: the hook must know the active map's `Name`. The plugin already has it (the draw).
- **Needs game:** open two different maps in a row and hover the portal; if the text does not change, a cache is involved.

### Q4. Safest hook point for the slot level id only

- **Slot level key is shared (static).** After PR #91 the slot level keeps Colossal Summit's name string, so a hook keyed on the text key (`0x3e2b736`) would also change the real Colossal Summit unless the slot gets a unique key first.
- **Options.**
  1. **`0xc7a30` (LevelName getter): not hookable at the entry.** Call-only (relative `call` at `+4`; the 24-byte prefix is unique), so no entry hook is proposed. It covers the automap title and the party text and **misses the portal tooltip** (Q1). Its text would have to be reached through the callers' entries (`0x14751e0`, `0x1471bc0`, both hookable) with the row key swapped around the original call (option 4), or by a mid-function patch at a call site; both are inference and not designed here.
  2. **`0x3e2b736` (key lookup thunk).** Call-only and a thunk into D2RCore with 1217 callers: a hot path, and the key alone does not say which level it is for. Not recommended.
  3. **`0xc17e0` (portal tooltip builder, hookable).** Wrap it: call the original, then if the unit's destination (`0x34ad40`, the loader-widened 16-bit level id) is the slot level id, overwrite the output buffer with the plugin's name. It sees the destination and runs only when the tooltip is built. The original appends the owner suffix `" (%s)"` itself (Q1, static), so the wrapper must replace only the level name part and keep the suffix. Hooking `0x34ad40` for the id is not advised: its prefix is unique only through the loader's `E9` byte (fragile), and `0xc17e0` already receives the unit. Covers the portal only.
  4. **Write the slot row's key.** Write the map's key into the slot row's `+0xfd` when the map opens; no code hook, and it would change the portal, automap and party text together (and any bit `0x40` object in the slot level, Q1). Needs the levels row to be writable and found (row getter `0x32c4a0`, hookable, or through the table); it is outside the SDK's read-only data table contract (`data_tables.h` rows are read-only, game-thread-only). **Needs game:** write a different key (at most 39 characters) at the slot row's `+0xfd` and hover the portal; check the page is writable.
- **Recommendation (inference):** option 3 for the tooltip; for the automap title and party text, option 4 if the writable-row check passes (otherwise wrap the `0x14751e0` and `0x1471bc0` entries and swap the key around the original, unverified). If the banner is in D2RCore (Q2), only option 4 or a text replacement reaches it.

### Q5. Does the SDK offer a string or localisation route that avoids an engine hook?

- **Static (SDK pin 717f727).** `LocalizationService` is lookup-only: it resolves a key to text and does not replace an engine string. A plugin's strings live in the plugin id's namespace. `ResourceService::registerResource` can register `data/.../d2rloader/<plugin-id>/...` bytes at runtime, which could carry strings (unverified that a runtime-registered string file is read by the lookup). `SharedEventService` has an item tooltip event and UI-message listeners only; there is no area name or portal tooltip event.
- **Inference (not read from the engine):** a LevelName key is `char[40]`, and if the engine lookup `0x3e2b736` resolves a `plugin-id:Key` string the way the plugin's own `getStringByKey` does, the whole key must fit 39 characters. The header documents only plugin-local keys for `getStringByKey`, plus the `d2r:` and `d2rloader:` prefixes, so whether `0x3e2b736` accepts a plugin prefix is **needs game**. The header also says "Package JSON defines strings by Key": that is a possible route for a fixed name (a key defined once in a package JSON), not for a per-map name.
- So the SDK has no per-map area or portal text route (static). A string table can give the slot level one fixed name, not a per-map name, unless the plugin rewrites the registered string at each map open (**needs game:** register, replace the resource at the next map open, and check the tooltip updates).

### Needs game

1. Does the red portal tooltip run `0xc189f`? Count hits while hovering the red portal. (Also: does the `0x8e1c9` data model exporter feed the entry banner? Log its call when entering the slot level.)
2. Does the area name banner call `0xc7a30` or `0x3e2b736`? Log both when entering the slot level.
3. Is the slot row writable? Write a different key at `+0xfd` and re-hover.
4. What does the owner suffix `" (%s)"` look like in single player (the `0x8d4b0` global decides whether it is appended)? Compare a stock portal tooltip with a hooked one.
5. Does the text cache between two maps? Open two different maps in a row.

### Game checks of 2026-11-12 (issues #87 and #90)

Live run, slot level 138 ("Cartographer Preset", LevelName key `ColossalSummit`, as installed by `tools\install.ps1` from origin/main 5c2ba9a). D2R 3.3 build 93847,
D2RLoader 1.3.1-beta, test mod and test save, single player, Normal difficulty (the two test maps had `Difficulty` set to Normal and the map was opened with
`cartographer open Shorelines`; one map per game). Screenshots of the game window only, read by hand; no hook or probe code was run. The two memory writes
below were made from outside the game with `WriteProcessMemory` into the loaded levels row (a test technique, not plugin code).

| Check | Result | Evidence |
|---|---|---|
| #87: red portal tooltip in town | Reads "Colossal Summit", not "Missing String"; no `" (owner)"` suffix in single player (needs-game item 4) | `hover_portal_town` |
| Entry banner | "Entering The Colossal Summit" | `enter_*` |
| Automap title inside the map | "Colossal Summit" | `automap_inside` |
| Return portal inside the map | "Harrogath" | `hover_portal_inside` |
| Leave, then enter again | Town banner "Entering Harrogath"; the red portal stays and still reads "Colossal Summit"; entering again works | `back_in_town`, `reenter_1`, `reenter_tip` |
| #90: the map's own `Name` ("Shorelines") | Appears nowhere: not on the tooltip, the banner or the automap. Still needs a runtime hook | all of the above |
| Item 3: is the slot row writable? | **Yes.** Writing `Harrogath` over the row's LevelName (`+0xfd`) changed the automap title inside the map to "Harrogath" at once, with no reload | `automap_after_name_write` |
| Item 1 (indirect): does the tooltip read the row key? | **Yes, live, no cache.** After the same write, the portal tooltip in town read "Harrogath" | `town_tip_after_write` |
| Banner source | The banner follows **LevelEntry** (`+0x14d`), not LevelName: writing `EnteringHarrogath` over it changed the banner on the next entry to "Entering Harrogath" | `banner_after_entry_write1` |
| Item 4: owner suffix | None in single player | `hover_portal_town` |

So the tooltip, the automap title and the banner all read the slot level's own row strings at the time they are shown. Writing existing string keys into LevelName
(`+0xfd`) and LevelEntry (`+0x14d`) changed the tooltip, the automap title and the banner live, in single player, with no hook; the tooltip is read when hovered, the
banner when entering. The row was found by a memory scan of the game process (one copy of the table; row 137 and row 138 are `0x18c` apart). The writes were made from
outside with `WriteProcessMemory`; whether the same write works in process was not tried ("writable" is shown only for that). That a plugin could find the row through the
table or the row getter is inference.

**Not shown:** the map's own name. Only existing keys were written, never a map's `Name`. Section 3 already shows that a LevelName that is not a key reads "Missing String"
(the first tooltip, #87), so a free-text `Name` such as "Shorelines" is expected to do the same. A string key per map is therefore a prerequisite for this route
(Q5: a runtime-registered string or a resolvable prefixed key; unverified). Option 4 of Q4 as written covers `+0xfd` only; the banner also needs `+0x14d`, so the route is two writes.

Static facts (dump `D2RLoader_6ab3782c_dump.exe`, stamp `0x6ab3782c`): the levels.txt loader builds its column descriptors on the stack, `0x20` bytes each
`{+0 name, +8 type, +0xc size, +0x10 offset}`; the names are loaded at `0x3300d4` (LevelName), `0x3300e2` (LevelWarp) and `0x3300f0` (LevelEntry), and the fields are stored at
`0x33024d..0x3302ad`:

| Column | type | size | offset |
|---|---|---|---|
| LevelName | 1 (string) | `0x27` | `0xfd` |
| LevelWarp | 1 | `0x27` | `0x125` |
| LevelEntry | 1 | `0x27` | `0x14d` |

Each field is `0x28` apart, a `char[40]`: a text key of at most 39 characters plus the NUL, not an index.

Conditions for the write route to be safe (for #90's design, not checked in game):

- Size: write at most 39 characters and always write the NUL; refuse, or fall back to the shared key, for a longer key (a `plugin-id:` prefix counts toward the 39).
- Restore: the row is process-wide table data and outlives the game. Save the original 40 bytes of both fields and write them back on map close, on `GameLeft` and on plugin
  unload, also for the Maze slot (139); otherwise the next game's slot (and `cheat goto 138`) shows the last map's name.
- Threads: write on the game thread when the map opens, before the portal exists, so no reader sees a half-written key. One copy of the table was found; a second data bank was not checked.
- Multiplayer: each process has its own tables. A host write changes only the host's text; a TCP/IP client keeps "Colossal Summit" unless its own plugin writes the same key, which needs the
  map identity sent to the client (#98 sync, section 12).
- Free text: needs a key (see above).

**Not checked:** hit counts on `0xc189f`, `0xc7a30` and `0x3e2b736` (items 1 and 2 directly; no probe code exists, only behaviour was observed); whether the
`0x8e1c9` data model feeds the banner (the banner is explained by LevelEntry, so probably not); two maps in two games (item 5; with one shared row key the text
was the same for every map anyway); a free-text name; the item right-click path (the right-click on a map scroll did nothing, cause not found; the log gave no gate line, so the item path was not confirmed here); a map opened in Hell; TCP/IP and a second player (the owner suffix there).
Test set-up notes: the spawned test item was a copy of the Identify scroll row, so it reads "Scroll of Identify". The character save failed every 30
seconds with an encoding error after an assert on a stat index (`BC_ASSERT nStat`). The cause is not shown. A missing `itemstatcost.txt` in the test mod is an unlikely guess (a mod without the table gets the
game's base table; the file only makes Cartographer skip map stats, and `PlayerStats` is off by default). To tell, rerun the same save and mod without `cartographer.dll`: if the assert stays,
it is setup or save state; if it goes, it is Cartographer. Not looked into; no Cartographer code was changed.

### Mod-free route for a per-map name (issue #90, desk work 2026-11-12)

The live checks above settle that the game reads the slot row's LevelName (`+0xfd`) and LevelEntry (`+0x14d`) keys when it shows the tooltip, the
automap title and the banner, and that a write there takes effect at once. What was missing is a **string key that resolves to the map's own
`Name`, without the active mod shipping a string file**. Evidence levels: **dump** (read from the build 93847 dump), **SDK** (read from the pinned SDK 717f727
headers and guides, not run), **needs game** (not shown). Nothing here was run in a game.

| Candidate | Result |
|---|---|
| (a) SDK string service that registers a key at runtime | **Route chosen.** SDK: a plugin owns the string namespace `PluginInfo.id` (`cartographer`) and defines text by a JSON array of `{Key, <language>...}` records at `data/local/lng/strings/d2rloader/<plugin-id>/<any>.json` (`LOCALIZATION.md`). `ResourceService::registerResource` takes a full virtual path under `.../d2rloader/<plugin-id>/...` and bytes from memory (`resources.h`: path "beginning with `data/`", copied before it returns, 64 MiB per resource). So the plugin needs no file in the mod: it builds the JSON from `maps.txt` and registers it. `LOCALIZATION.md` says "the full `namespace:key` must fit that field" for text fields that store the key in a fixed-size field; LevelName and LevelEntry are `char[40]` (dump, section 10 above), so the row value would be `cartographer:N<i>` (name) and `cartographer:E<i>` (entry), 15 to 18 characters. That the engine's lookup resolves a key written into a row after the tables loaded is **not** something the SDK shows (needs game, below) |
| (b) Hook the game's key lookup `0x3e2b736` | **Rejected.** Dump: `0x3e2b730` and `0x3e2b736` are `FF 25 rel32` loader thunks (bytes `ff 25 da f6 ff ff`, `ff 25 dc f6 ff ff`) into `D2RCore.dll`: not hookable by the 14-byte rule and not signable (the thunk bytes repeat across the whole thunk table: a `FF 25 ?? ?? ?? ??` pattern gives 865 matches). `D2RCore.dll` is not in the dump, so the lookup's code cannot be read or signed. The key alone also does not say which level it is for, and 1217 callers make it a hot path |
| (c) Hook the tooltip builder `0xc17e0` and whatever feeds the automap and banner | **Rejected as the main route, kept as the tooltip-only fallback.** Dump: `0xc17e0` is hookable and its prefix `40 55 56 41 56 48 8D AC 24 20 FF FF FF 48 81 EC E0 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 00 00 00` matches once (`.text 0xc17e0`, checked again 2026-11-12). It only covers the tooltip. The automap title (`0x14751e0` via `0xc7a30`) and the banner (LevelEntry) have no hook that was found, and the output buffer's encoding after `0x1227840` was not read. It also needs a new engine hook with its own guard, where the chosen route needs none |

#### What the chosen route does (written, not verified in game)

- Off by default: `MapNames=1` in `cartographer.txt` turns it on. Nothing is registered or written while it is `0`, so the working "Colossal Summit" text is untouched.
- At load and on every table reload the plugin registers one strings file: a `N<i>` record (the map's `Name`) and an `E<i>` record ("Entering " and the `Name`) for every map,
  with the same text in all 13 languages (a record with a missing language does not fall back to English). Names are trimmed, control characters become spaces, invalid UTF-8 becomes U+FFFD (so one bad name cannot spoil the whole file) and they are cut to 60 bytes at a whole UTF-8 character.
- When a map opens, on the game thread and just before `OpenRedPortal`, the plugin (1) finds the map's index in the registered list, (2) asks the localization service for both local keys
  and requires the exact registered texts back (so the loader is known to hold them before the game is pointed at them; otherwise it logs and leaves the slot's name), (3) gets the slot row
  from the levels row getter `0x32c4a0` with the data flag read from `game + 0x106` (the bank choice is **inference**, see Risks), (4) checks both fields look like text keys (a NUL inside, printable ASCII), (5) saves the original 40 bytes of both (refusing a row that already holds a `cartographer:` key, so the plugin's own key is never taken as the original),
  and (6) writes the two keys (NUL-filled to 40 bytes; a key of 40 or more characters is refused). Every page the 40 bytes touch is made writable for the copy only if it is not. Maze maps use slot level 139 the same way.
- The originals are written back on `GameLeft`, on plugin unload and when `OpenRedPortal` opens nothing. Only the first copy of a level is kept as the original. A failed open puts back only what that open wrote (the level's original if it was the first write, else the previous map's keys). A restore goes through the row pointer saved at write time, after a memory check, and touches a field only if it still holds the key the plugin wrote; it does not call the game's row getter, which could fault at process exit. If a write back fails, the saved original is dropped and the registration is kept as it is.
- A table reload (`cartographer check`) while a slot row holds a `cartographer:` key does not unregister or replace the strings file: the change (a new list, or `MapNames=0`) is kept pending and applied after the rows are back (on `GameLeft` or a failed open), so the open map's tooltip, title and banner never lose their text or show another map's. The SDK calls are made outside the plugin's lock.
- Every step fails open with a log line; the slot level keeps its own name.

| Field | Record |
|---|---|
| Part | `LevelsRow` `0x32c4a0` (levels row getter `(dataFlag, levelId) -> row*`), called only, never hooked. Signature (82 bytes, 12 masked) is the one in the Parts table above, added to `part_signatures.h` and to `tools/check_portal_signatures.py`: it matches once on the dump (`.text 0x32c4a0`); the first 46 bytes are shared with the sibling getters (`0x331e50`, `0x331f50`, `0x332050`; the 30-byte prefix also hits `0x32c5a0`), the first relative operand is the `E8` at `+0x12`. `python -I plugins/cartographer/tools/check_portal_signatures.py <dump> --stamp 0x6ab3782c`: "all signatures ok" |
| Wired to | `plugins/cartographer/src/engine/map_names.cpp` (parts, services, row writes), `plugins/cartographer/src/mapname/` (pure: keys, text, JSON, 40-byte writes, restore bookkeeping; `tests/mapname_tests.cpp`) |

#### Risks and what is still needed from the game

- **needs game (the one unproven link):** that the game's key lookup behind `0x3e2b736` resolves a `cartographer:N0` key found in a levels row. `LOCALIZATION.md` only says the full `namespace:key` must fit a fixed-size field; it does not say a key written into a row after the tables loaded resolves. The lookup lives in `D2RCore.dll`, which is not in the dump. If it does not, the tooltip, title and banner would read "Missing String" for every map while `MapNames=1` (hence off by default). Check: set `MapNames=1`, `cartographer open <Name>` and hover the portal.
- needs game: that a string file registered at runtime (not a file in an MPQ) is picked up by `getStringByKey` and the engine. The plugin verifies the first half itself before every write (`map names:` log lines, "D2RLoader does not return the registered text yet"); the engine half is the check above.
- needs game: that the row page is writable in process (the live write of 2026-11-12 used `WriteProcessMemory`, which ignores page protection); the plugin lifts the protection for the copy if needed.
- needs game: the entry banner format for a long name; the tooltip's `" (owner)"` suffix in TCP/IP (none in single player).
- Multiplayer: as above, a TCP/IP client keeps "Colossal Summit" unless its own plugin writes the same keys (needs the map identity from the #98 sync).
- **Known limit, one row per slot:** a second map opened through the same slot in one game rewrites the slot row, so the first map's portal (if it still stands) and the automap show the newest map's name. Not redesigned.
- **Data flag, inference:** the plugin picks the levels bank with the server's data flag (`game + 0x106`). The readers pick theirs by themselves; the tooltip uses the object unit's flag (`0x34a0e0`, `unit + 0x1bd`, at `0xc185d` and `0xc188a`). That both give the same bank in single player is inference, not shown. If they differed, the write would land in a bank nobody reads (the slot keeps its name).
- Restore by saved row pointer: if the game freed its levels table and left the heap untouched, bytes that still hold our key would be written into freed memory. `Readable` and the exact 40-byte key match make a stray write very unlikely, and the game loads the table once and tears it down at exit, so this is accepted.
- Restore on `GameLeft` assumes the event fires on a thread that may touch the table; one copy of the table was found in the earlier check, a second data bank was not (the plugin uses the game's own data flag).

#### Game check 2026-11-12 (MapNames)

Live check of `MapNames=1` (PR #124, off by default) on D2R 3.3 build 93847, D2RLoader 1.3.1-beta, slot 11 (test mod `D2RToolsTest`), test save `cartos` (Barbarian level 1, Normal), single player. `core-maps` moved out, the mod's own `levels.txt` plus the installer's "Cartographer Preset" (level 139), vanilla `lvlprest.txt`, `lvlmaze.txt`, `itemstatcost.txt` and a `misc.txt` with the `cm0` item, one map `Shorelines` (Preset, Corridor, Normal, no event, no stats), `MapNames=1`. Two games, same process.

**Result: the one unproven link holds.** (live game) The key lookup resolves `cartographer:N0` and `cartographer:E0` written into the slot row after the tables loaded.

- Log, at load: `map names: registered 1 map name(s) as strings`; `cartographer check` reports `Map names: on (written, not verified in game)` and `part LevelsRow: found by signature`.
- The log lines quoted below (and the `put back` restore line) were read during the run but were not kept as evidence; no log file or screenshot is stored with this PR.
- Log, on `cartographer open Shorelines`: `map names: level 139 row 0000000125941524: writing 'cartographer:N0' and 'cartographer:E0'`. On leaving: `map names: level 139 text put back`. The same pair appeared in the second game.
- The `getStringByKey` self-check logs nothing when it passes (it logs only on failure), so this run shows it passed by the absence of the failure line and by the write that followed.
- Portal tooltip (hover in town): `SHORELINES`. Not "Missing String", not "Colossal Summit".
- Entry banner on arrival: `Entering Shorelines` (red text, first screenshot after the load).
- Automap title inside the map, top right: `SHORELINES`, above the difficulty and mod lines.
- Second game in the same process: the tooltip read `SHORELINES` again after the row had been put back and written anew.
- No crash file, no error or warning line from the plugin.

Levels kept apart: unit tests and a clean build (`tools\build.ps1 -Plugin cartographer -Test`, 12 of 12) were earlier runs, not part of this PR, and were not re-run for it; live game, one map name, one language (English), one character, two games. Not checked: the stock name before a map is opened (no screen shows the slot level then; the `put back` line is the only evidence of the restore), a long map name on the banner, TCP/IP (the host's name on a client, #130), a second map in the same game (the one-row-per-slot limit above), other languages, the levels bank of the data flag (the write landed in the bank the tooltip and automap read, which settles that inference for single player), Steam build 94995.

## 11. Event Spawn names from the game's compiled tables (issue #95)

**Status:** written and unit tested (issue #95, code in `plugins/cartographer/src/engine/event_apply.cpp`, pure logic in `src/eventplan/`); the signatures were checked statically on the build 93847 dump (`check_portal_signatures.py`, both rows ok); **not run in a game**. The analysis below is static research from the dump. Context: section 9 resolves an event's `Spawn` against the mod's `monstats.txt` `Id` and `superuniques.txt` `Superunique` columns (`ReadSpawnNames`, `plugins/cartographer/src/plugin.cpp`); issue #95 asks whether the game's own loaded tables can do it, so a mod that ships those tables in another form (or none) still resolves. Labels: **static** is read from the dump; **inference** is reasoned from it; **needs game** is a check to run.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer event `Spawn` resolution (`EventsMapOpened`, `engine/event_apply.cpp`; `ReadSpawnNames` is the fallback) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | Data tables base `0x300a90(dataFlag)` (cl, below 4). MonStats row getter `0x976e0 (dataFlag, index)`, SuperUniques row getter `0x397220 (dataFlag, index)`. Name to row lookup `0xa121f0 (map, name, logOnMiss) -> row, exactly -1 on a miss (set at 0xa12385)`. Engine resolvers `0x3a9050 (dataFlag, name, out)` and `0x3a9150`. Table fields (with `dt` the base): `dt+0xf58` monstats row vector (64-bit count at `+0xf60`, row size `0x1fc`), `dt+0xf70` monstats name map pointer, `dt+0x1078` superuniques row vector (64-bit count at `+0x1080`, row size `0x3c`), `dt+0x1090` superuniques name map pointer; also `dt+0xf90` monsounds and `dt+0xfd0` monplace maps |
| Which hook | None. The route is reading rows (SDK) and, for superuniques, one call-only lookup |
| Guard | Masked prefixes below, each unique by `pat`. `0x976e0` is hookable (15-byte cut, no relative operand) and `0x300a90` hookable (16 bytes); `0x397220` (relative `call` inside the first 14 bytes), `0xa121f0` (`mov rax,[rip+..]` at +13) and `0x3a9050` / `0x3a9150` (`je rel32` at +3) are call-only |
| Contract | See Q1 to Q4. In short: re-read `*(dt+0x1090)` on every call, never cache it, skip the call when it is null, reject an empty name, treat exactly -1 as a miss, call on the game thread (the thread of `EventsMapOpened` is not shown, Q4) |
| Evidence | `dump_dis.py ... dis` of the getters, of the monstats and superuniques table loaders (found by the unique store of the monstats row size, `0x1fc`, to `[rsp+0x28]` at `0x3a0af9`; the superuniques store of `0x3c` at `0x3a2173` is **not** unique, the same `C7 44 24 28 3C 00 00 00` bytes match 5 sites: `0x3a2173`, `0x5bec88`, `0x5befb1`, `0x5cfa94`, `0x1b6ffd3`; that loader was found through its descriptor strings instead) and of the resolvers; `pat` for every other prefix |
| Validation | Static only. **Not run in a game** |
| Failure mode | If a guard does not match, or the self-check of Q4 fails, keep the `.txt` route of section 9 as it is today |
| Where it runs | Not in `LoadAll()` (`plugin.cpp` `ReadSpawnNames`, which runs at load and `Reload()`, possibly off the game thread and before the tables exist). The lookup belongs in `EventsMapOpened` (`engine/event_apply.cpp`), before the call to `resolveSpawn` at line 411, with the `dataFlag` read from the player unit (`StartMapEvents` has it at `engine/portal.cpp:191`); not inside the pure `resolveSpawn` (`eventplan/eventplan.cpp:49`), which has no unit. See Q4 |
| Coexistence | No hooks. A call-only use of `0xa121f0` reads a hash map on the game thread |

Masked prefixes (all checked unique on the dump with `pat`; `??` is a relative or absolute operand):

| Address | Role | Prefix | Class |
|---|---|---|---|
| `0x976e0` | MonStats row getter | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30 48 63 F2 E8 ?? ?? ?? ?? 48 8B F8 48 8B DE 85 F6 78 09 48 3B 98 60 0F 00 00 72 18` (the shorter form up to `48 8B F8` matches 14 sites) | hookable |
| `0x397220` | SuperUniques row getter | `40 53 48 83 EC 30 48 63 DA E8 ?? ?? ?? ?? 48 8B CB 48 3B 98 80 10 00 00` | call-only |
| `0xa121f0` | name to row lookup | `40 55 56 57 41 54 48 8B EC 48 83 EC 78 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 E8 44 89 45 AC` | call-only |
| `0x3a9050` | by-name resolver (MonPreset `Place`) | `4D 85 C0 0F 84 ?? ?? ?? ?? 48 89 5C 24 10 57 48 83 EC 20 33 C0 41 C6 40 01 00 66 41 89 40 02` | call-only |
| `0x3a9150` | by-name resolver (MonPet `monster`) | `4D 85 C0 0F 84 ?? ?? ?? ?? 48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 40 49 63 D9 49 8B F0` | call-only |
| `0x300a90` | data tables base `(dataFlag)` | `48 83 EC 28 0F B6 C1 48 89 44 24 38 48 83 F8 04 72 19 48 8D 44 24 38 48 8D 4C 24 40 48 89 44 24 40 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B 44 24 38 48 8D 0D ?? ?? ?? ?? 48 03 C0 48 8B 04 C1 48 83 C4 28 C3` (three sibling accessors share the first 40 bytes; the tail tells them apart) | hookable |

### Q1. Compiled monstats record: where is the `Id`, and how does the game look a row up by `Id`?

- **Row and getter (static).** `0x976e0` calls `0x300a90`, compares the index with the 64-bit count at `dt+0xf60`, and returns `[dt+0xf58] + index * 0x1fc`.
- **The `Id` text is stored inline in the row (static plus inference).** The loader (chunk `0x39db7f`) describes its columns in 0x20-byte entries `{name, type, size, row offset, extra}`. The `Id` entry has type `0x13` and size `0x2f`, row offset 0, and `extra` is the address of the map slot `dt+0xf70` (not the map itself). Reading: a 47-byte inline key text (46 characters plus the terminator). The next column, `BaseId`, is at `+0x32`. The type, the size and the offset field (0) are static; that type `0x13` means "inline key text", and that D2RCore puts it at row `+2` (after a u16 row index), is **inference**, shown live (dump of `+2`, 47 bytes) (it fits the toolkit's `knowledge/tables.md` note of a 47-character `Id` key, and the flag bytes at `+0x3c/+0x3e/+0x3f` in `monster-count-addresses.md`, which sit just after it). No hash or string-table index of the `Id` is stored in the row.
- **Who fills the map (inference, not in the dump).** Both table loaders (monstats and superuniques) hand their descriptor arrays to `0x3e2b544`, a D2RLoader `FF 25` thunk (`FF 25 4E F6 FF FF`, in `.maho`; the same kind as `0x3e2b5c8` in section 1) into `D2RCore.dll`. The rows, and the inserts into the maps behind `*(dt+0xf70)` and `*(dt+0x1090)`, happen in D2RCore, so the fill depends on D2RLoader's D2RCore, not on `D2R.exe`. That the map holds `Id` to row index, and that the stored keys are lower-cased, are therefore **inference** from the descriptors and from how the lookup is used. **Static in `D2R.exe`:** map creation (`0xa11fa0`, stored at `0x3a0a83` for monstats and `0x3a2103` for superuniques; it initialises the default `[map+0x3c]` to -1), the registration (the descriptor `extra` is the slot address) and the lookup.
- **Lookup by name (static).** `0xa121f0(rcx = map, rdx = name, r8d = logOnMiss)` lower-cases only the query (`0xa25650`), hashes it, compares exactly (`0x12d3ee0`) and returns `[node+0x30]` in `eax`. So case-insensitivity holds only if D2RCore inserted lower-cased keys (inference above). A miss returns exactly -1 (`0xa12385`; every caller does `test eax,eax; js`); `logOnMiss = 0`, or a name starting with `*`, skips the log. A null name **or an empty string** returns the map's default `[map+0x3c]` (-1 as initialised by `0xa11fa0`; whether D2RCore changes it is unknown), so the plugin must reject an empty `Spawn` before calling. A name longer than 15 characters makes the lookup allocate a temporary string from the game allocator (`0x76210`, freed through `0x12063c0` at `0xa123cf`): fine on the game thread, one more reason not to call it elsewhere. The same function serves monsounds (`dt+0xf90`), superuniques (`dt+0x1090`) and monplace (`dt+0xfd0`).
- **The engine's own by-name users (static).** `0x3a9050` (MonPreset `Place`) and `0x3a9150` (MonPet `monster`) call `0xa121f0` for superuniques, then monstats, then (`0x3a9050` only) monplace, and store a kind (2, 1, 0) and the row in a small out record. They are load-time callbacks and log on a miss, so they are not proposed. They store the kind differently: `0x3a9150` as a `u16` at `+0` with the row at `+2`, `0x3a9050` as a byte at `+1`. I did not find the cheat or console `mspawn` resolver in `D2R.exe` (the toolkit's `cheat-commands.md` says the same); it is probably in D2RCore (inference).
- **A runtime caller (inference on what it is).** `0x409bbc`, inside the chunk of `0x409ad0`, calls `0xa121f0(*(dt+0xf70), name, 0)` with `dt = 0x300a90(byte [r14+0x106])`, at runtime and not at load. Its AI-mode name table at `0x1d14e20` holds "regular", "idle", "wandering" and others, so it looks like a runtime spawn text parser; what it parses is inference. It is the evidence that the map outlives the load. The engine's other runtime caller guards the map first: `0x36005c` does `mov rcx,[rbx+0xf70]; test rcx,rcx; je` before the call.
- **Null map (static).** `0xa121f0` with a null map is a crash, not an assert: it calls `0xa0ffc0`, may `int3`, then falls through to `cmp [rsi+0x20]` on null, an access violation. `0x3a9050` does no null check either (it loads `[dt+0x1090]` and calls). Each table load stores a new map from `0xa11fa0`, so a cached pointer goes stale after a reload. **Contract:** re-read `*(dt+0xf70)` / `*(dt+0x1090)` on every call, never cache the pointer, and skip the call when it is null, as the game's own caller does at `0x36005c`.
- **Needs game:** that the map is still alive and filled after the load (the lookup is called outside the loader); `0x409bbc` above is the inference for "yes".

### Q2. The same for superuniques (`Name` key, `Class` field)

- **Row and getter (static).** `0x397220` returns `[dt+0x1078] + index * 0x3c`, with a null return for an index past the 64-bit count at `+0x1080`.
- **The `Superunique` key is not in the row (static).** The loader marks the key column type `0x11` (size 0, lookup only) and registers it into the map at `*(dt+0x1090)`; the row has no key text. So the name cannot be found by reading rows: only through the map with `0xa121f0`, or from the txt file.
- **Fields in the row (static, from the loader and two consumers).** `Name` is a string-table index, a `u16` at `+2` (descriptor type `0x18`, through the string-table callback `0x308be0`). `Class` is at `+8`, resolved at load through the monstats map into a monstats row index (inference for the exact width; the consumers of `+8` treat it as a monstats row). The `u16` at `+0` is unproven (inference: the row's own id).
- **Lookup by name (static).** `0xa121f0(*(dt+0x1090), name, 0)`, as in Q1.

### Q3. Does the SDK `DataTableService` give enough, the row count plus a raw row pointer, to read those fields without engine calls?

- **MonStats: yes.** `getTable(ctx, bank, TableId::MonStats, &view)` (22; `bank` is the `Bank` the data flag maps to, see Q4) gives `{rows, rowCount, rowSize}`. Check `rowSize == 0x1fc` (the header says to check it before casting), then compare the 47-byte inline `Id` at row `+2` (corrected from 48 bytes at `+0`; static: loader `0x39db30` gives the `Id` column type `0x13`, length `0x2f`, offset 0, with `BaseId` next at `+0x32`; the `+2` is D2RCore's type-0x13 layout, inference, shown live) with the name, case-insensitively, over `rowCount` rows. No engine call of its own (picking the `Bank` with the data flag calls `0x300a90`, see Q4). **Inference:** the SDK's `rows` is the same vector as `dt+0xf58`; the rowSize check is the guard. A linear scan once per map open is cheap.
- **SuperUniques: not for the name.** `getTable(ctx, bank, TableId::SuperUniques, &view)` (28) gives `0x3c`-byte rows, but the key is not in them (Q2). Once the row is known they give `Class` (`+8`) and `Name` (`+2`). `findRowById` supports neither table (`data_tables.h`).
- Rows are read-only, game-thread-only and expire on the next table load (the header's contract), which suits a once-per-map-open read. Off the captured game thread the SDK returns `Busy`.
- `Class` is at most 4 bytes: the next descriptor sits at `+0xc` (type 2). MonSound (type `0x15`, `+0x1c`, `dt+0xf90`) follows the same index-through-a-map pattern.

### Q4. The safest route, and its guard

- **Recommendation (inference).** Keep the section 9 `.txt` route as the fallback and put the game's tables first:
  1. **Monstats `Id`:** the SDK table (no engine call, no signature), guarded by `rowSize == 0x1fc` and non-null `rows` for a non-zero count.
  2. **Superuniques `Name`:** one call-only lookup, `0xa121f0(*(dt+0x1090), name, 0)` with `dt = 0x300a90(dataFlag)`, found by the unique prefixes above, skipped when `*(dt+0x1090)` is null and with an empty name rejected first. It needs struct offsets, so it is guarded by the self-check below. If the SDK later offers a name lookup for these tables, use that instead.
  3. **Class and display name:** after the row is known, read `Class` (`+8`) and `Name` (`+2`) from the SDK rows (`rowSize == 0x3c`).
- **Where it runs.** Not in `LoadAll()`: that runs at plugin load and `Reload()` (possibly from the console thread), before the tables exist, and `ReadSpawnNames` is only the txt route. The lookup runs at map open, in `EventsMapOpened` (`engine/event_apply.cpp`) before the `resolveSpawn` call at line 411, with the `dataFlag` read from the player unit (`engine/portal.cpp:191` passes the player in; the byte is at `UnitDataFlagOffset`, `engine/stat_apply.cpp:111`); `resolveSpawn` (`eventplan/eventplan.cpp:49`) stays pure. The self-check needs a unit, so it runs at the first map open. As built, the result is cached per (SDK `TableView.revision`, unit `dataFlag`), read from a MonStats view at every map open (no listener; see "What was built"), not per revision alone: a Classic game and a LoD/RotW game can follow each other with no table reload, and the same revision then pairs a different `dt`. The map pointer is never cached.
- **Which table, and how the two keys pair.** `dt = 0x300a90(dataFlag)` takes a `dataFlag` below 4. The plugin already reads it from the unit as a byte at `UnitDataFlagOffset` `0x1bd` (`stat_apply.cpp:111`; the game's own helper is `0x34a0e0`). The SDK `getTable` takes a `Bank` (`Classic` 1, `Lod` 2, `Rotw` 3, `data_tables.h`). The header does not say how a data flag maps to a `Bank`, so the pairing is **not shown statically**; the self-check below proves it: use the `Bank` whose view matches the `dt` of the unit's data flag, and if none matches, the route stays off.
- **Self-check (the guard that matters), per revision.** With `dt = 0x300a90(dataFlag)` and `view` from `getTable(ctx, bank, TableId::MonStats, &view)`, require `view.rows == *(dt+0xf58)` and `view.rowCount == *(dt+0xf60)` (the counts at `+0xf60` and `+0x1080` are 64-bit; the SDK `rowCount` is 32-bit, so read them as 64-bit and compare after checking they fit); with the SuperUniques view, require `view.rows == *(dt+0x1078)` and `view.rowCount == *(dt+0x1080)`. This proves the bank / data flag pairing and the `dt` offsets directly, with no engine call. Then, before trusting `0xa121f0`, look up every monstats row's own `Id` through `*(dt+0xf70)` and require the returned index to equal the row's index (duplicates in different case aside). A mismatch or a null map turns the superunique route off and logs why. Do not call `0x3a9050` / `0x3a9150` (they log on a miss and are load-time callbacks) or the row getters (the SDK already gives the rows).
- **No hook is needed.** Which thread runs `EventsMapOpened`, and whether it is the thread the SDK treats as the captured game thread, is not shown; see Needs game.

### What was built (issue #95)

Everything below follows the recommendation of Q4; the items under "Needs game" stay open.

- **Parts.** `DataTablesBase` (`0x300a90`) and `NameToRow` (`0xa121f0`) are two new called-only parts of the events group, with the masked prefixes of the table above in `core/part_signatures.h` (each unique on the dump, checked by `check_portal_signatures.py`; per-part guard through `ResolvePart`, so a part that does not match once is logged and left off). The row getters and the by-name resolvers are not used. No hook.
- **Where.** `EventsMapOpened` (the portal passes the player unit) reads the data flag (byte `0x1bd`), asks `RouteFor(dataFlag)` for the verdict and resolves before the pure `resolveSpawn`, which stays as it was and is only the fallback.
- **Monstats.** The SDK `getTable(MonStats)` view, scanned for the inline 47-byte `Id` at row `+2` (`findMonsterRow`, case-insensitive, an empty name or empty cell never matches, a field without a NUL is not text). Checked on every call against the live table (`checkTable`).
- **Superuniques.** `NameToRow(*(dt+0x1090), name, 0)`, the map pointer re-read on every call, skipped when null, an empty name rejected, exactly -1 a miss (`superRowFromLookup`: any other negative or a row past the table is also treated as not found, and logged). `Class` is read as 16 bits at row `+8` (the width is inferred) and kept only if it is a monstats row.
- **Self-check** (`EvaluateRoute`, per data flag): `dt = DataTablesBase(dataFlag)` for a data flag below 4; the SDK `Bank` (1 to 3) whose MonStats view has rowSize `0x1fc`, rows equal to `*(dt+0xf58)` and count equal to the 64-bit `*(dt+0xf60)` is chosen (none: the whole route is off); for superuniques the same against `+0x1078`/`+0x1080` with rowSize `0x3c`, a non-null `*(dt+0x1090)`, and the round trip: every distinct non-empty monstats `Id` looked up through `*(dt+0xf70)` must return its own row (`checkRoundTrip`). A failed part logs why (`spawn names: ...`) and turns that part off: the monstats table can stay on with superuniques off.
- **Cache.** `RouteCache` keyed by (SDK `TableView.revision`, data flag), the revision read from a MonStats view at every map open (`TablesRevision`); no `DataTablesLoaded` listener is used. A newer revision drops the older entries. Nothing is cached when no view gives a revision (0), and a verdict made while the SDK was busy is never cached. The map pointer is never cached.
- **Fallback.** The `.txt` route (`resolveSpawn` over `ReadSpawnNames`) is used when the monstats route is off, when the table cannot be read at the time, and, with only the superuniques route off, for a name that is not a monster in the game's table. A name the working routes do not have is simply unresolved (no event). When a superunique is found in the txt while the monstats route is on, its `Class` is looked up by `Id` in the game's monstats table (`GameMonsterRow`) instead of by txt row count; if that `Class` is not in the game's table, the name is treated as not found (logged).
- **Order differs from issue #95's text.** #95 asked for the mod's txt as a first source if present; this puts the game's tables first and the txt files second, as Q4 recommends: the loaded tables are what the game spawns from, so they cannot disagree with the game, while a txt may be absent or differ from what was compiled.
- **Repeated monstats `Id` (decided).** The round trip accepts either copy's row for an `Id` that appears in more than one row (`idRowAccepted`): which copy D2RCore's map keeps (first or last) is not known, and one odd mod row should not turn the superuniques route off. The monstats scan (`findMonsterRow`) still returns the first row with the `Id`, so a repeated `Id` is ambiguous in the game too; treat it as a mod mistake. Needs game: see item 10.
- **Superuniques map values.** The monstats round trip cannot prove the superuniques map (its key is not in the rows). Instead, a looked-up row is used only if its `Class` (row `+8`) is a monstats row, and, where the mod's `superuniques.txt` has the name and a `Class`, the `Id` at the game's `Class` row must equal (case-insensitive) the `Class` name the txt gives (names are compared, never row numbers); a mismatch logs and falls back to the txt files. A stock superunique with no txt is checked only by the range test.
- **Superuniques row index (review nit 3, answered).** The map's value cannot be round-tripped like monstats: the `Superunique` key is not in the row (Q2), so there is no name to feed back. What guards it instead: `superRowFromLookup` bounds the value by the row count (exactly -1 is the miss), the row's `Class` must be a monstats row, and the `Id` there must match the txt's `Class` name where the txt has the superunique. Game check of 2026-11-12: `Bishibosh` gave row 0 and class row 58 (`fallenshaman1`), the same as the txt. A map that returned something other than a row index would fail the `Class` test or the txt agreement and fall back to the txt files; a stock superunique with no txt has only the range test.
- **Txt fallback and `Class` (review nit 4, done).** With the monstats route on and superuniques off, a superunique found in the txt takes its `Class` row from the game's monstats table (`GameMonsterRow`), not from the txt row count.
- **Ambush, Treasure and Shrine events (review nit 5).** They are applied now (section 9; `event_6b.cpp`), so the CHANGELOG line "not applied yet" is not restored.
- **Unit tests** (`eventplan_tests`): name matching, the self-check decisions, the superunique lookup result, and cache keying. The engine calls are not unit tested.

### Needs game

1. Does `*(dt+0xf70)` resolve a stock `Id` to its row index at map open (the self-check), and `*(dt+0x1090)` a stock superunique name? Log the index and compare with the SDK row.
2. Is the miss value exactly -1 at runtime, with `logOnMiss = 0` printing nothing, and does an empty name return -1?
3. Does the 47-byte `Id` at monstats row `+2` (static: type `0x13`, length `0x2f`, `BaseId` at `+0x32`; the `+2` is inference, shown live) match `monstats.txt` for a mod that renames monsters, and is `Class` at superuniques `+8` a monstats row index (compare with the txt's `Class`)?
4. What is the `u16` at superuniques row `+0`?
5. Does `getTable` return `Busy` on the thread that runs `EventsMapOpened` (the server player tick)? If so, the self-check and the revision read (`TablesRevision`) fail there, nothing is cached and the txt files are used; the check would have to run on a thread where `getTable` succeeds.
6. Does the self-check (`view.rows == *(dt+0xf58)` and the same for `+0x1078`) pass for the `Bank` the unit's data flag implies, and which pairing is it?
7. Does a stock monster `Id` resolve with a lower-case and an upper-case query alike (proves D2RCore inserts lower-cased keys)?
8. The log lines to read for items 1 to 7: `spawn names: data flag N, table revision R: monstats from the game's table on/OFF (SDK bank ..), superuniques by the game's name map on/OFF; <why>`; `spawn '..': monstats row N from the game's table` and `superuniques row N from the game's table, class monstats row M`. A round trip that fails logs `N of M monstats Ids did not map back` and the superuniques route stays off (a mod with an odd `Id` row, for example, would do that).
9. Is the `TableView.revision` non-zero and stable between two map opens (log shows the same `table revision R`, and the self-check line appears once), and does it change after a table load? A revision of 0 only means the check repeats at every map open.
10. A mod with a repeated monstats `Id`: which row does the map give (first or last)? Does the superuniques `Class` agree with the txt for a stock superunique (no `which the mod's superuniques.txt does not agree with` warning)?

### Game checks of 2026-11-12 (issue #106)

Live on slot 11, build 93847, same setup as section 9's checks. Item numbers are those of "Needs game" above.

- **Q1 correction (a bug that blocked the run).** The compiled monstats `Id` is at row `+2`, 47 bytes; the `u16` at row `+0` is the row's own index (dumped in game for rows 0 to 5). The first version read the `Id` at `+0` (an invisible first character), so the round trip failed: `722 of 722 monstats Ids did not map back to their own row (first: row 1 gave -1)` and the superuniques route stayed off. Fixed (`kMonStatsIdOffset`, `findMonsterRow`, the round trip and the Class check); the tests put the `Id` at `+2` and the longest `Id` is 46 characters.
- **1. Self-check.** `*(dt+0xf70)` resolves every stock `Id` to its row (`spawn names: data flag 3, table revision 1: monstats from the game's table on (SDK bank Rotw), superuniques by the game's name map on`) and the superuniques name `Bishibosh` gives row 0.
- **2. Miss value.** The failed round trip above reported `-1` for the misses, and a miss printed nothing from the game.
- **3. Class width.** `Bishibosh` (row 0) has class row 58, which is `fallenshaman1` in the table and in the txt: `spawn 'Bishibosh': superuniques row 0 from the game's table, class monstats row 58`. The `u16` at superuniques row `+0` (item 4) was not read.
- **5. `getTable` on the event thread.** Not `Busy`: the self-check line appears at the first map open and the route is on.
- **6. Pairing.** SDK bank `Rotw` (data flag 3).
- **7. Case.** Only the case-insensitive round trip passed (the txt's `Id` case and the game's lower-case keys agree); an upper-case query alone was not run.
- **9. Revision.** `table revision 1` at every map open of every game; the self-check line appeared once per game.
- **8 (row index of the txt route) and 10 (repeated monstats `Id`).** Not run (no blank-line or repeated-`Id` mod file was arranged). With the mod's `superuniques.txt` removed the route stayed on (section 9's checks); the txt route itself was not forced off.

## 12. Multiplayer map sync engine checks (issue #98)

Desk research for phase 7 (multiplayer): four questions about syncing a drawn map to a TCP/IP or open-game client. Read from the build 93847 dump
with `dump_dis.py` (Capstone) and from the pinned SDK (`third_party/PluginSDK` at 717f727); nothing was run in a game and no code changed. Each claim is
marked **static** (read from dump bytes or tables), **SDK** (read from the pinned headers), **inference** (follows from those, not observed) or
**needs game** (with the check). `D2RCore.dll` is not in the dump, so anything the loader or core does on the wire is not covered. Battle.net and the
outdoor builder are out of scope.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer phase 7 (multiplayer map sync); part 1 (the host tells clients the open map) is built, see "What was built (issue #125)" at the end of this section |
| Module | `D2R.exe`, D2R 3.3 build 93847, stamp `0x6ab3782c`, under D2RLoader 1.3.1 |
| Location | The table below. Final addresses are found by byte signature |
| Guard | Every prefix in the table below was re-run with an overlapping search over the dump's executable sections (`??` is a relative operand) and matches exactly once. The first draft's `0x86bc0` prefix matched 4 times and `0x490300` 3 times; both are now the longer unique forms. `0x4475c0` is not a hook target here (see Q4) |
| Evidence | `dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]` and `pat "<hex>"`; callers found by scanning `E8`/`E9` targets. SDK headers `network.h`, `lifecycle.h`, `lifecycle_events.h`, `threads.h` and `examples/network-ping` |
| Validation | None in a game. Static and SDK reading only |
| Failure mode | If a check below fails, multiplayer stays off for Cartographer: the plugin refuses to open a map for a remote client and logs why |
| Coexistence | The packet table entries that are D2RLoader thunks into D2RCore (ids 6, 24, 96, 130, 135, 149, 150) are not touched. `NetworkService` is the loader's own route and needs no hook. `0x44db20` is already hooked by Monster Count and Better Auras, so any Q4 fallback hook there must chain, not replace |

| RVA | Role | Prefix |
|---|---|---|
| `0x58f680` | Portal operate (object operate slot 15, server side) | `48 89 4C 24 08 55 53 56 57 41 56 41 57` |
| `0x592c40` | Object operate dispatcher (table `0x23a51c0`) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 48 8B F1` |
| `0x129930` | Client packet `0x03` (LoadAct) handler | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 8B 79 02` |
| `0x129b80` | Client packet `0x07` handler (lazy room build trigger) | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 30 0F B7 69 01` |
| `0x86bc0` | Client act load (`isClient = 1`, seed from the packet) | `48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 98 00 00 00 44 8B A4 24 00 01 00 00` |
| `0x86e20` | Client unit placement (second lazy room build trigger) | `48 89 5C 24 18 55 57 41 54 41 56 41 57 48 83 EC 40 48 8B D9` |
| `0x2ef1c0` | Act creation (runs the DRLG for the act) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 50 44 0F B6 F1` |
| `0x48aa50` | Server act creation (`isClient = 0`, seed from `game + 0x12c`) | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 56 48 83 EC 60 0F B6 EA` |
| `0x2ef320` | World room lookup stub, tail-jumps to `0x328680` (callers `0x86ebe`, `0x86f98`, `0x129bf1`) | `48 8B 44 24 30 48 85 C0 ?? ?? 48 8B 40 18 48 8B 52 70 48 89 44 24 30 ?? ?? ?? ?? ?? CC CC CC CC 48 89 5C 24 08` |
| `0x4817f0` | Server send `(clientCtx, buf, len)` | `40 55 56 57 48 83 EC 40 49 8B E8 48 8B F2` |
| `0x44db20` | Unit death (`D2Game\src\Unit\SUnitDmg.cpp`) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50 48 8B E9` |
| `0x4475c0` | `(game, info, flag)`, called from the death path and 46 other places; returns 0 unless the unit at `[info + 8]` (the victim) has type 1 (a monster; a player is 0) | `40 53 55 56 41 55 48 83 EC 38 48 8B 5A 08` |
| `0x490300` | Missile owner resolver | `48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 48 85 D2 75 20 48 8D 4C 24 38 88 54 24 38 E8 ?? ?? ?? ?? 84 C0 74 1F CC` (the longer form recorded in [dps-counter.md](dps-counter.md)) |

Already recorded elsewhere and relied on here: maze level build `0x3271c0` and room lookup `0x327400` (section 5), spawn calls `0x492cf0` and `0x499ba0` and the room
populator `0x50acf0` (section 9), the once-per-game superunique bitmap at `game + 0x1d8` (section 6).

### Q1. How can a plugin send a small message host to client and back?

- **SDK (primary route).** `NetworkService` (`network.h`) is the supported route for private plugin messages. A plugin registers a channel with a 64-bit
  compatibility token, connects to the host (`connectToHost`), sends to the host (`sendToHost`) or to one client peer (`sendToClient`), and receives on host and
  client callbacks. A payload is at most 224 bytes. `getPeerPlayer` maps a peer to a player and must be called on the game thread. The `network-ping` example is
  the pattern to copy. No engine address or hook is needed for it.
- **SDK limits.** Not available on Battle.net (out of scope). Plugin roles are Client, Server or Shared; Cartographer is Shared with native hooks, and Shared
  plugins must match between host and client. `runOnGameThread` returns Unavailable on a remote TCP/IP client (`threads.h`: a remote client has no local
  authoritative game thread), so client side work has to run from a hook or a tick the plugin already owns, not from a hop the SDK cannot give. `connectToHost` is called only after entering a game and may return `Busy` on a TCP client until the host's transport packet has arrived, so the client retries; the plan cannot reach the client earlier than that. Per the SDK README, Shared plugins must match in TCP/IP before a client can join at all, so a client without the plugin never gets as far as a map (this mostly answers Q3.3; the rest is the match check itself, **SDK**).
- **Static (fallback, the game's own packet path).** The client packet handler table is at RVA `0x22bbc40`, stride `0x18`, entries `{handler, size, third}`,
  index = classic packet id. Most entries have the handler first and a zero third field, but ids 12 to 16, 23, 36, 76, 77, 103 to 111, 113 and 114 hold the stub `0x78d20` in the first field and the real handler in the third; ids 110 and 111 (size 1) have the stub in the third field too, so they have no real handler (**static**). Seven entries are D2RLoader thunks into D2RCore (ids 6, 24, 96, 130, 135, 149, 150); the loader's own transport packet id was not
  located (D2RCore is not in the dump). The server sends with `0x4817f0(clientCtx, buf, len)`. A plugin-made packet would need a free packet id, a handler table
  entry and a size, which collides with the loader's thunks and with other plugins. **Not recommended**; consider it only if the SDK route fails a check below.
- **Needs game.**
  1. A Shared plugin with a channel on a host and one remote TCP/IP client: does the connection complete, and how many `Busy` retries does `connectToHost` take on a TCP client, and what does the connection event say when the client
     has a different compatibility token (reject reason)? (A client without the plugin is expected to be refused at join by the SDK's Shared rule; confirm.)
  2. A host to client message of 224 bytes: is it delivered intact, and on which thread does the client callback run?
  3. Does the host see the client's reply on its server thread, and can `getPeerPlayer` be called there?
- **Recommended route.** `NetworkService`, one channel, a token derived from the plugin's wire version. The host sends the map plan (seed, map id, a few bytes)
  before the client builds the level; the client acks. Fall back to "map off" for a client whose connection is rejected.

### Q2. Where does a TCP/IP or open-game client build the slot level, and do the maze hooks run on the client, the server, or both?

- **Static: both sides run their own DRLG.** Each act is created by `0x2ef1c0`. The server calls it from `0x48aa50` with `isClient = 0` and the seed at `game + 0x12c`.
  The client calls it from the packet `0x03` handler `0x129930` through `0x86bc0` with `isClient = 1` and the seed taken from the packet. Level seeds are
  deterministic from the act seed: `drlg + 0x860 + level id` through `0x367110`. The same level id and act seed give the same layout on both sides, which is the
  property the plan cache in `maze_hooks.cpp` ("per game, so the server's build and the client's build get the same plan") relies on.
- **Static: client rooms are built lazily.** On the client a room is built through `0x2ef320` -> `0x328680` -> `0x327400` -> `0x3271c0` when the client needs it. Two
  triggers were found: the packet `0x07` handler `0x129b80` and the unit placement function `0x86e20` (the stub's callers are `0x129bf1`, `0x86ebe`, `0x86f98`).
  `0x3271c0` itself has exactly 5 direct callers (`0x32705e`, `0x327525`, `0x3609cf`, `0x3da6c3`, `0x3dd5f6`; E8/E9 scan, **static**). Which of them can run on a client was not worked out beyond the two
  triggers above, so "other triggers" is bounded by those 5 plus any indirect call; classify the other three before relying on "plan before the first lazy build".
- **Static consequence.** `LevelBuild` (`0x3271c0`) and the maze hooks run on **both** the server and the client, each against its own DRLG instance. In single player
  both DRLGs live in one process, so the hook runs twice for one level (already true today). In a TCP/IP game it runs once per DRLG: the host has both, a remote client
  has only the client DRLG. A remote client therefore has the plan only if the plugin put it there before the first lazy build, i.e. before the packet `0x03` / `0x07`
  handlers or the unit placement run for the slot level.
- **Not located.** The server side sender of packet `0x03` (LoadAct) and of packet `0x07`: callers of `0x4817f0` with those ids were not found (only unrelated ids
  `0x21` and `0x1d` to `0x1f`); they may go through D2RCore or a table. The receiving handlers are static, so the answer above stands, but the exact host moment at
  which a client is told to load the act is unknown, which matters for when the plan message must be sent.
- **Inference.** The host must send the plan for the destination map when a client is about to enter a map portal (Q3 gives the moment), and the client's plan cache
  must be keyed like the host's. The host's own build is unchanged.
- **Needs game.**
  1. Log `0x3271c0` entry (level id, thread) on a host and on a remote client while the client enters the slot level; expect host server DRLG, host client DRLG and remote
     client DRLG.
  2. Does the remote client reach `0x3271c0` for the slot level before or after the plugin message arrives? If before, it builds the stock slot layout.
  3. Does the lazy build ever run on a thread other than the client main thread?
  4. Log `0x4817f0` ids when a client enters an act and when it enters the slot level, to find the LoadAct sender.
  5. Log which of the 5 callers of `0x3271c0` fire on a remote client for the slot level.

### Q3. How can the portal entry be held until a client confirms?

- **Static: the gate point is the server portal operate.** Objects 59 and 60 (the red portal) use OperateFn 15, slot 15 of the table at `0x23a51c0`, which is the
  server function `0x58f680`, reached through the dispatcher `0x592c40`. It runs where the object is operated (the host). It takes one pointer, the operate context the
  dispatcher builds on its stack (`lea rcx, [rsp+0x20]; call rdi` at `0x592cbf`): `[ctx + 8]` is the object (its `[unit + 0xc]`, read through `0x34ab60`, must be 2, `0x58f69c`)
  and `[ctx + 0x10]` is the operating player (`0x34b9d0` must return 0, a player, `0x58f6b5`; that check is an assert, an `int3` at `0x58f6cf` when the type is not a player and `0x592580` is true, not a refusal). Only the object check (`[unit + 0xc]` not 2, `0x58f6a7`) takes the refusal path at `0x58fcd4`, which calls `0x491960(player, 0x14, player)` (the third argument is also the player) and
  returns 0 (`r15d`), the existing way the game declines an operate; that call is probably what the client sees (**inference**, callee unread).
- **Inference (route).** Hook `0x58f680` (the 13-byte prefix above is unique). For the red portal and a client the plugin has not marked as synced: send the plan over
  `NetworkService` (Q1), return 0, and when the client acks, mark it synced and let the next click through (or call the original). Hooking the dispatcher `0x592c40`
  also works but sees every object operate.
- **Route: player to peer.** The gate sees a player unit, but `sendToClient` needs a `PeerHandle`, and `getPeerPlayer` resolves peer to player only from a scheduled host
  game-thread callback (**SDK**). So on each Connected event the host queues `getPeerPlayer` and keeps a player id to peer map; a player with no entry is treated as not synced
  (the gate refuses, or the host's own player skips it).
- **Needs game.**
  1. The context layout above (object `[ctx + 8]`, player `[ctx + 0x10]`) is **static**; confirm in a game by logging both at the hook.
  2. Does a refused operate (the `0x491960(player, 0x14)` call) show the player anything, and does the client retry or must the player click again? A "waiting for client" line through section 9's chat
     path (`0x477a30`) is the likely feedback.
  3. A client that never acks (not running the plugin): timeout, then stock behaviour or refusal? A Shared plugin must match to join (Q1), so this is
     mostly a client that connected but never acks (version skew or a lost message).
  4. The host's own player can skip the wait (same process, the plan is already there).
- **Recommended route.** Gate at `0x58f680` with an ack from `NetworkService`, one pending entry per portal and client, with a timeout.

### Q4. What does kill credit need for monsters spawned by map events in multiplayer?

Result: mostly **needs game**. The death path was read far enough to say which unit is which, not far enough to say who is credited.

- **Static: the death function.** `0x44db20(game, victim, killer, flag)` (`rbp = game`, `rbx = victim`, `rdi = killer`, `r14d = flag`). In the block `0x44dc01` to `0x44dd01`:
  `0x34ab60(victim)` returns `[unit + 0xc]`, which is not the unit type (probably the mode); the block is skipped when it is 0 or 0x11 (`0x44dc09`, `0x44dc19`). Then
  `0x588b00(killer, victim, game)` when the killer is non-null, and a check that `0x34b9d0(victim)` returns 1 (`0x44dc3d`; `0x34b9d0` returns `[unit + 0]`, the unit type, and
  type 0 is a player, type 1 a monster, as `0x58f680` requires a type 0 operator). So the rest of the block runs for **monster victims only**.
- **Static: the death info record.** `0x4471e0(victim, 0, &out)` fills `out` (`dword +0 = 0`, `qword +8 = victim`, written at `0x447276`); `[rsp + 0x30]` (`out + 0x10`) is set to the
  killer at `0x44dcf4`. The killer owner fields (`[killer + 0x38]`, `0x341a30`, `0x341a20`, `0x349b70`, `0x34ae80`, `0x341950`) are resolved just before.
  The call at `0x44dcfc` is `0x4475c0(game, &out, 1)`, where the first argument is the game, not a player.
- **Static: `0x4475c0`.** It loads the unit at `[info + 8]` (the victim) and returns 0 unless its type is 1 (a monster) (`0x4475df`, `0x447600`); the rest of the body (stat `0x36` is read at `0x447630`)
  was not read, and it has 47 direct call sites, so it is a general routine, not something specific to this path. It is **not** shown to select a player, and nothing here says
  who is credited with experience or drops. The earlier "find the player the kill counts for" reading did not follow from the bytes and is withdrawn.
- **SDK.** The `MonsterDeath` listener (server thread) gives the killer identity only, with no owner resolution (a missile or minion shows as itself), so it is not a credit
  mechanism.
- **Static: spawn path.** The map events spawn through `0x492cf0` / `0x499ba0` (section 9). The victim is an ordinary server monster made by the game's spawn core; nothing in
  the death block was found that checks how it was spawned.
- **Static: bosses.** The superunique spawn is gated by the once-per-game bitmap at `game + 0x1d8` (per game, not per player), so one superunique can be spawned once per game: a
  design limit for several maps in one game, not a credit item.
- **Inference (unverified).** Monsters spawned on the host are real server units and replicate through the normal unit packets, so credit, experience, drops and quest flags are probably decided
  server side and need no plugin message. What is open is display: the plugin's monster stats (section 7) are applied on the server unit, and whether a client shows them depends
  on whether those stats replicate.
- **Needs game.**
  1. Kill a map event monster as a non-host client, directly, with a minion and with a missile: experience, kill count and drop owner (this is the only way to settle credit).
  2. Do the monster stats from section 7 reach a remote client's display (health bar, hover text), or only the server's numbers?
  3. Does the monster level override (section 6) change the experience award for a client far from the monster?
- **Recommended route.** Spawn on the host only through the section 9 calls and leave credit to the engine, verified by the three checks. If credit is wrong for minions or
  missiles, read the rest of `0x4475c0` and the owner chain of the block above (and `0x490300` for missiles) before touching anything; a hook on `0x44db20` is the fallback and must chain with Monster Count and Better Auras.

### Needs game (summary)

1. Q1: channel connect and reject behaviour with a remote TCP/IP client; 224 byte delivery; callback threads.
2. Q2: `0x3271c0` hit order on host and remote client for the slot level; does the plan arrive before the remote client's first lazy build; the LoadAct sender.
3. Q3: `0x58f680` context fields (confirm `[ctx + 8]` and `[ctx + 0x10]`); refusal feedback and retry; a client that never acks; the host's own player; the player to peer map.
4. Q4: credit and drops for a remote client (own attack, minion, missile); stat replication to clients' displays; range effects.
5. Ack content: the client solves the layout from its own tables, so its ack should carry a hash of the solved plan and the host can refuse a client whose `maps.txt` or pieces differ (the phase 7 done check, two clients show the same layout).
6. A client that joins, or rejoins, after the map was opened.
7. The client's plan cache surviving the level rebuild on a later visit (plan section 1, pd2-resurrected#930).
8. Which of the 5 callers of `0x3271c0` run on a remote client.

### Recommended route (all four)

`NetworkService` for the plan message (Q1); keep the maze hooks as they are on both sides (Q2) with the plan in the per-game cache before the client's first build; gate the
portal entry at `0x58f680` until the client acks (Q3); spawn events on the host only and leave credit to the engine, verified by the Q4 checks. The unlocated pieces (LoadAct sender,
retry behaviour, stat replication) are needs-game items and do not block writing the plugin code.

### What was built (issue #125, phase 7 part 1): written, not verified in game

The smallest slice section 12 supports: the host tells joined clients which map is open. Nothing here was run in a game.

- **Route.** The SDK `NetworkService` only (Q1). One channel (`localChannelId` 1), compatibility token `"CART"` plus the wire version, so a host and a client on different message versions never connect. **No new native address, no hook, no new byte signature**: nothing was added to `part_signatures.h` or `tools/check_portal_signatures.py`. The gate `0x58f680`, the LoadAct sender and everything else marked unproven above is untouched.
- **Messages** (little endian, `src/mapsync/mapsync.h`). `Hello` (client to host, 2 bytes: wire version, 0). `MapOpen` (host to client, 20 bytes): wire version, flags (bit 0 = maze map), `maps.txt` row (u16), hash of the map's name (u32, FNV-1a, case folded), slot level (u16), opening count (u16), layout seed (u64). Decoding refuses a wrong size or version, an unknown flag, a slot level or opening of 0, and a preset map that carries a seed. A preset map has no layout seed, so it sends 0; the per-player stat and event seeds are drawn on the host and are not sent.
- **Host** (`src/engine/map_sync.cpp`). After a map opens through either path (item or console) the identity is stored and sent to every client that said hello. A client is known only from its `Hello`, not from the host's connection event: whether the host's `connectionState` callback fires per peer is not in the SDK text, so the code logs it and does not rely on it. A `Hello` queues a send through `runOnGameThread` (the pattern in the SDK `network-ping` sample), so a client that joins after the map opened is told too (late join). A peer that answers `NotConnected`, `StaleHandle` or `NotFound` is dropped. At most 16 clients are kept; when the list is full the oldest entry is dropped for a new client. The host's own process never connects to itself (the connect loop, the hello and an incoming `MapOpen` each re-check the server tick, so a late tick still wins).
- **Client.** At `LocalPlayerReady` a connect loop starts on the UI thread (`runOnUiThread`, queued again each frame, since `runOnGameThread` is Unavailable on a remote client). It waits 3 s, then calls `connectToHost` every 1 s, up to 30 times, and stops at once if the server tick part has run (a host or single player: no connection is made), if the host rejects it for any reason but `HostUnavailable` (the reason is logged; `HostUnavailable` keeps the schedule running, as the SDK README says to retry), or on `Incompatible`, `WrongRole`, `Unsupported` or `OwnerInactive`. It also asks `getChannelInfo` before each attempt, so a missed connection event does not stall it. On connect it sends `Hello` once. A `MapOpen` is decoded, then checked against the client's own `maps.txt`: the row must exist and its name hash must match (else the table differs between host and client and the message is logged and dropped), and an older opening than the one kept is ignored. The kept identity (`MapSyncRemoteMap()`) is what later parts will read. Nothing else uses it yet.
- **Fail open.** Every callback and the install are wrapped; a missing or short Network or Thread service, or a refused channel, turns only the sync off with a log line (`cartographer check` prints it). The portal and the map open do not depend on it. A client without the `PlayerTick` part never gets `LocalPlayerReady` from Cartographer and so does not start (the same part the console open needs).
- **Single player and a host with no client.** `MapSyncMapOpened` finds no client and sends nothing (one log line); no connection is made because the server tick shows within a frame, long before the 3 s delay.
- **Evidence.** `tools\build.ps1 -Plugin cartographer -Test`: build clean, 11 of 11 tests pass, including the new `mapsync_tests` (round trip, fixed wire bytes, every truncation and single-byte change, hello, the client's check with its refusals leaving the kept map unchanged, the client list limit (oldest dropped when full), the rejection rule, the connect schedule). `check_portal_signatures.py` and `check_maze_signatures.py` against `D2RLoader_6ab3782c_dump.exe` (`--stamp 0x6ab3782c`): all signatures ok, as before.
- **Reading of the SDK that differs from the first draft of this section.** The README says local games and the TCP/IP host also connect "inside the same process", so `connectToHost` would succeed on a host and in single player; the code avoids that by connecting only when no server tick has run, which is an inference (a process that has the server ticks at once), not something the SDK states. Q3's "queue `getPeerPlayer` on each Connected event" was not built (the portal hold is out of scope here); the host's peer to player map stays a later item.

#### Needs game (part 1)

1. A remote TCP/IP client with Cartographer connects: how many `Busy` attempts, and the log line for it. A client with a different token, and a client without the plugin: the rejection reason shown, and that the join itself is refused.
2. The host's `connectionState` callback: does it fire per client, with a peer handle (logged as `connection event`), and on which thread do the host and client message callbacks run?
3. The `Hello` reaches the host, the reply reaches the client, and the log lines on both sides match (`told client`, `the host's open map`).
4. A client that joins after the map was opened is told (late join); a client that leaves and rejoins.
5. Whether the identity reaches a client before that client first builds the slot level (Q2, needs-game item 2).
6. Whether `runOnUiThread` runs on a remote client at a rate that makes the 1 s retry work, and that the loop ends when the game is left.
7. A host with two clients: both are told, and one leaving does not stop the other.
8. Single player (also paused in the first seconds) and a host with no client are unchanged: no `map sync: ... connectToHost` line, no extra delay.

### What was built (issue #130, phase 7 part 2): written, not verified in game

The client applies the identity part 1 keeps. Scope is what the sections above show the client must do itself; nothing here was run in a game. **No new native address, no new hook, no new signature** (nothing added to `part_signatures.h` or the checkers).

- **Why only these two.** Each process has its own tables (section 10, "Multiplayer"), so the host's write of the slot row's `LevelName` and `LevelEntry` does not reach a remote client: the client must write its own (a). Both sides run their own DRLG and the client builds the slot level lazily (Q2), so the client's build needs the same `{maps.txt row, seed}` as the host's to give the same plan (b). Map stats and events are drawn and spawned by the host (sections 7 and 9) and reach the client as ordinary units; kill credit (Q4) and the portal hold (Q3) are separate.
- **When.** An accepted `MapOpen` (a new identity, not a resend or an older opening) queues one apply step on the UI thread (`runOnUiThread`, the route the connect loop uses; `runOnGameThread` is Unavailable on a remote client). The step re-checks the server tick (a host or single player process applies nothing), the game generation (a step from a left game does nothing), and that the row still has the sent name hash. It runs once per opening (`AppliedOpening`).
- **Decision** (pure, `src/mapsync/mapsync.cpp` `decideApply`, unit tested). Facts the glue reads from the client's own side: the slot level its `slots.txt` gives for the map's kind equals the sent slot level; its row is a maze map exactly when the host's flag says so; `MapNames`; maze steering live; the Maze slot's tileset fits; the slot build state. A slot level or kind mismatch skips both halves (the client's tables do not describe the host's map). Otherwise the name half runs when `MapNames=1` here, and the layout half runs for a maze map with a non-zero seed, live steering, a fitting tileset and no slot build yet. Every skip is logged with its reason and shown in `cartographer check`.
- **(a) Name.** `MapNamesBeforeOpen(map, tables, slotLevel, dataFlag)`, the same code the host path uses (same keys, same saved originals, same restore on `GameLeft` and unload). It needs a levels-table data flag, and a remote client has no `game + 0x106`; the flag is taken from the arguments of the level build the existing `LevelBuild` hook sees (`MazeLevelDataFlag()`, last value, cleared on leaving a game; on a remote client every build is its own). With no build seen yet, the name is skipped and logged; when the maze parts are not all hooked the `LevelBuild` hook is off and no flag ever exists, so the name half logs that as its own reason ("the data flag is not available because the LevelBuild hook is off") instead of "no level build has been seen yet".
- **(b) Layout.** `MazeRemoteMapOpened(map, tables, hostSeed)` installs the maze job through the same `SetJob` as `MazeMapOpened`, but with the host's seed as it is (the host mixes a salt with the job id; the client does not repeat that). The existing plan cache, `LevelBuild` hook and steering then run unchanged. It refuses, changing nothing, when steering is off or the slot level was already built or is being built here, **with or without a map open** (the new `SlotBuildState::ranAny()`; `blocked()` ignores a build with no job, which is right for the host's later open but not for a client that must not swap plans under an existing level). That case is logged and the client keeps the stock layout; no rebuild is attempted.
- **Single player and the host** are unchanged: the message callback and the apply step both return when the server tick has been seen, and a host never connects. The `LevelBuild` hook only stores its data flag argument (one atomic store).
- **What this does not check.** The plan is built from the client's own `maps.txt` and `shapes.txt` row; only the row index and the name are compared with the host's, so a client whose tileset, shape, size or `shapes.txt` differs from the host's gets a different maze. The plan hash in an ack (needs-game item 5 above) is still open.
- **Evidence.** `tools\build.ps1 -Plugin cartographer -Test`: build clean, 12 of 12 tests pass, including new `mapsync_tests` cases (a good maze identity applies both halves, a preset one the name only, a slot or kind mismatch skips both, each maze refusal leaves the name alone, MapNames off, the skip sentences, an opening applied once) and a `maze_tests` case for `ranAny()`. `check_portal_signatures.py` and `check_maze_signatures.py` against `D2RLoader_6ab3782c_dump.exe` (`--stamp 0x6ab3782c`): all signatures ok.

#### Needs game (part 2)

1. The data flag: is the one the client's `LevelBuild` hook sees the one `LevelsRow` needs for the client's tables (the `map sync: ... its name is set ... (data flag N)` log line), and is a build always seen before the host's map arrives? If not, the name is skipped. A wrong flag only leaves the stock name (the write lands in a bank nobody reads); the restore stays exact because it goes through the saved row pointer.
2. Does the identity reach a remote client before its first build of the slot level (Q2 item 2)? If not, the client logs "built here before the host's map arrived" and shows the stock maze.
3. The thread of the message callback (the apply is queued to the UI thread either way) and whether writing the row there tears with the tooltip, automap or banner reads.
4. The client's own portal tooltip, automap title and entry banner show the map's name; the name is back after leaving the game.
5. Host and client mazes match for the same `maps.txt` (same seed, same plan, compare the `plan` hash line in both logs).
6. A client with `MapNames=0`, a client with a different `slots.txt`, and a second map opened in the same game.

## 13. Ambush, Treasure and Shrine events: static checks (issue #102)

Desk research for phase 6b (section 12 is the multiplayer research, PR #101). Build 93847, `D2RLoader.exe` stamp `0x6ab3782c`
(`C:\ghidra-work\D2RLoader_6ab3782c_dump.exe`), read with `tools/dump_dis.py` and a private disassembly script. **Nothing here
was run in a game.** Evidence levels: **dump** (read from the disassembly), **SDK** (read from the pinned headers),
**inference** (reasoned, not shown), **needs-game**. Every address is an RVA; its byte prefix is masked (`??` is a rel32 or
RIP-relative displacement) and matches exactly once in the dump's executable sections (overlapping search, re-run after the last edit of this section), except `0x402f50` (see the table).

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer phase 6b: Ambush waves, Treasure flee and `Drop`, Shrine placement, boss `Drop`; no code exists yet |
| Module | `D2R.exe`, D2R 3.3 build 93847, under D2RLoader 1.3.1 |
| Build identity | Image stamp `0x6ab3782c` |
| Location | The table below. Final addresses are found by byte signature |
| Guard | Every prefix in the table matches once (`0x402f50` has none, see its row). Hook candidates are rated with the section 6 rule (whole instructions, at least 14 bytes, no relative operand in them): `0x447b80` (15 bytes), `0x58e2f0` (16), `0x5930b0` (18) and `0x592c40` (23) pass; `0x441300` and `0x50acf0` have a RIP-relative instruction early (it starts at `+0x14` and `+0xc`; its displacement at `+0x17` and `+0xf`) and stay call-only or use the long masked prefix |
| Contract | Per row of the table and Q1 to Q4 below |
| Evidence | `dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]` and `pat "<hex>"`; callers by scanning `E8`/`E9` targets; SDK `MonsterDeathListener`, `TreasureClasses` rows, `ItemService` (`item.h`) |
| Validation | None in a game. Dump and SDK reading only |
| Failure mode | A part whose prefix does not match is left off and logged; the others keep working. Ambush: with no `MonsterDeathListener` the first wave spawns and later waves are skipped (no clear detection). Treasure: no `0x597030` match, the monster does not flee; no `0x441300`/`0x4404f0`/`0x397290` match, no extra drop. Shrine: no `0x490410`/`0x402f50` match, no object. Boss `Drop`: no `0x447b80` match, no extra drop; the stock drop is untouched |
| Coexistence | Nothing else in this repo hooks `0x447b80`, `0x58e2f0`, `0x5930b0`, `0x592c40`, `0x597030` or `0x4a2690`. `0x50acf0` is already hooked by Cartographer. A hook on the death or drop path must chain, as Monster Count and Better Auras do for `0x44db20` |

| RVA | What (level) | Masked unique prefix | Use |
|---|---|---|---|
| `0x50acf0` | room populator, already hooked in `event_apply.cpp` | `4c 8b dc 53 56 48 81 ec b8 00 00 00 48 8b 05 ?? ?? ?? ?? 48 33 c4 48 89 84 24 80 00 00 00 83 3d ?? ?? ?? ?? 00` (as section 6) | hooked |
| `0x4022a0` | handler registered under the command string `SpawnObject` (lea at `0x400e6b`, string at `0x1d13628`): the object spawn recipe (dump) | `40 55 41 56 41 57 48 83 ec 60 4c 8b f1` | call-only |
| `0x402f50` | free-spot search near a point (dump: call shape only, body unread); two direct callers, `0x401ecc` and `0x40239f` (in `0x4022a0`) | none: it is byte-identical to `0x418d00` for 452 bytes, differing only in rel32 operands, so no masked prefix is unique. Find it as the call target at `0x40239f`. The unmasked 24-byte form `40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 ec 98 00 00 00 48 8b 05 5d` is unique today but moves with the code | call-only |
| `0x490410` | object create used by `SpawnObject` (dump: arguments only) | `40 55 53 57 48 8d ac 24 70 fc ff ff` | call-only |
| `0x38fc70` | `(dataFlag)` returns the objects row count `*(dt+0x1530)` (dump) | `48 83 ec 28 e8 ?? ?? ?? ?? 8b 80 30 15 00 00` | call-only |
| `0x4a2690` | AI set-mode `(game, monster, [monData+0x30], mode)`; `r9` is the mode, range-checked `< 0x12`; returns early when stat `0x36` is set on the monster (dump) | `48 85 c9 0f 84 ?? ?? ?? ?? 55 53 56` | call-only |
| `0x597030` | **flee setup**: sets state 56 through a record and calls `0x4a2690` with mode `0xb`; 3 callers (dump) | `48 89 5c 24 10 48 89 74 24 18 57 48 81 ec 80 00 00 00 48 8b f1` | call-only |
| `0x596d40` | **not** the flee setup (an earlier draft said so): a spot and room search (`0x509cc0`, `0x38dfb0` twice, `0x491ca0`), ends at `0x59702e`; 4 callers (dump) | `48 89 5c 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 ec 40 4c 8b f9` | not used |
| `0x5cd060` | AI step that returns a monster to mode 0 once state 56 is gone (dump) | `48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 48 89 7c 24 20 41 54 41 56 41 57 48 83 ec 40 49 8b 68 10` | not used |
| `0x48b890` / `0x48b720` | AI event 2 scheduling (dump) | `40 53 56 41 54 41 56 41 57 48 83 ec 20 45 8b f1` / `48 83 ec 48 8b 84 24 80 00 00 00` | not used |
| `0x34c2c0` / `0x34c730` | the two target checks `0x597030` bails on: `0x34c2c0(unit)` nonzero, or `0x34c730(unit, 0, 1, 1, 1)` zero, five arguments (dump: shapes only) | `48 83 ec 28 48 85 c9 75 1d 88 4c 24 30 48 8d 4c 24 30 e8 ?? ?? ?? ?? 84 c0 74 4d` / `48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 57 41 56 41 57 48 83 ec 20 45 0f b6 f1` | call-only |
| `0x447b80` | monster death drop `(game, victim, arg3, info)`; reaches `0x441300` at `0x447e17`; 2 direct callers (dump) | `48 8b c4 44 89 40 18 53 55 56 57 41 54 41 56` (as section 6) | hookable by the rule (15 bytes) |
| `0x441300` | drop roller for a monster unit (dump) | `40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 ec 98 00 00 00 48 8b 05 ?? ?? ?? ?? 48 33 c4 48 89 84 24 88 00 00 00 4c 8b bc 24 18 01 00 00` (as section 6) | call-only here |
| `0x4404f0` | generic TC roller, 2 callers (`0x44148c` in `0x441300`, `0x59333d`); contract in Q2 (dump) | `4c 8b dc 55 53 49 8d ab 08 f7 ff ff` | call |
| `0x397290` | `(dataFlag, tcIndex u16, level)` TC lookup, returns the compiled TC pointer (dump) | `48 89 5c 24 08 55 56 57 48 83 ec 30 41 8b d8` | call |
| `0x5930b0` | chest TC pick by act, difficulty, area-level tier; **28 direct callers**, 3 of them inside `0x592cf0`, the function after `0x592c40` (dump) | `40 55 56 57 41 56 48 8d 6c 24 c1 48 81 ec d8 00 00 00` | hookable by the rule (18 bytes), but too broad |
| `0x396430` | `(dataFlag, difficulty, act, tier)` chest TC lookup, returns a compiled TC pointer (dump: shape only) | `48 89 5c 24 08 48 89 6c 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 ec 40 45 8b f9` | call |
| `0x58e2f0` | chest operate handler, slot 4 of table `0x23a51c0` (entry `0x23a51e0`; no direct caller) (dump) | `48 89 4c 24 08 55 48 8b ec 48 81 ec 80 00 00 00` | hookable by the rule (16 bytes) |
| `0x592c40` | object operate dispatcher (section 6) | `48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 57 48 83 ec 50 48 8b f1` | hookable (section 6) |
| `0x349860` / `0x34a330` | unit class `[unit+4]` / unit id `[unit+8]` getters (witnesses; dump) | `48 83 ec 28 48 85 c9 75 1d 88 4c 24 30 48 8d 4c 24 30 e8 ?? ?? ?? ?? 84 c0 74 01 cc b8 ff ff ff ff 48 83 c4 28 c3 8b 41 04` / the same ending `8b 41 08` | call-only |

**Unit fields.** The toolkit lists `+8` as the class id. The dump says `+4` is the class and `+8` the unit id (witnesses above), as already
recorded in [quick-npc.md](quick-npc.md#6-unit-layout) and the registry's unit `+0` / `+4` / `+8` row; nothing new here. The toolkit
is also inconsistent with itself: its RVA table lists `0x349860` as the class and `0x34a330` as unit `+8`, which agrees with the dump and not with its Units table.

### Q1. Ambush: detect arrival, spawn `Waves` waves, tell when a wave is cleared

- **Arrival (inference, from dump and plugin code).** The populator hook `0x50acf0` runs when a room is populated for a
  player, which is when a player is *near* the room, not inside it: fine for placing, too early for "reached the room". The
  per-tick check in `EventsPlayerTick` already finds the player's room for the entry message; fire when the player's server
  room is the event room (or its rect, `+0x80..+0x8c`, contains the player). **Recommended**, one-shot per event, remembered
  in the plugin state next to `spawned`.
- **Spawn (existing code).** Each wave is `Count` calls of `SpawnOne` (`SpawnMonster(game, room, x, y, row, 1, 0xa, 0)`) at
  spots from the event rect, one wave after the previous one is cleared. **New call context:** the later waves move
  `SpawnMonster` (`0x492cf0`) from the populator context (`0x50acf0`) into the `PlayerTick` (`0x42e350`) context. Nothing in the dump
  says the spawn core cares, but it has never been called from there (needs-game).
- **Wave cleared (SDK + inference).** Remember the units the spawns return. `SpawnOne` currently discards the returned unit; the id is the unit's `+8`
  ([unit layout](quick-npc.md#6-unit-layout)). The SDK `MonsterDeathListener` reports id, class
  and level id after the normal drop, on the server thread, with no unit pointer: count tracked ids as they die; the wave is
  cleared when all are accounted for. A monster that leaves the game without dying never reports, so add a time-out per wave (the value is open).
  The alternative, looking units up by id each tick, needs a server unit lookup by `(type, id)` that is **not resolved** here.
- **Evidence:** SDK for the listener, inference for the rest. **Needs game:** the listener's id equals the id at `+8` of the unit the spawn returns;
  the room test fires on walking in; a wave does not trip the room's monster cap; `SpawnMonster` works from the tick context.

### Q2. Treasure: flee, drop from a named TC, chest that drops from `Drop`

- **Flee (dump for the mechanism, needs-game for the effect).** Recommended route: **call `0x597030`** on the spawned monster. It
  is the engine's scare setup `(game, source, target monster, r9, arg5, arg6)`; its 3 callers are `0x45e971`, `0x45fb1b` and `0x583dcf`. In order:
  1. it bails out if `0x34c2c0(target)` is nonzero (`0x597055`) or `0x34c730(target, 0, 1, 1, 1)` returns 0 (`0x597073`; five arguments: `rcx`, `dl`, `r8b`, `r9b` and the byte at `[rsp+0x20]`);
  2. it fills a record on its stack (`+0x48` source, `+0x50` target, `+0x58 = 0x4d`, `+0x5c = 1`, `+0x60 = arg6`, `+0x64 = -1`, `+0x6c = 0x38`) and calls `0x433d20`, an `FF 25` D2RCore thunk (call-only, body not readable here); `0x38` is state 56, so the engine sets the state itself, with `arg6` as a duration-like value;
  3. it calls `0x4a2690(game, target, [[target+0x10]+0x30], r9 = 0xb)` at `0x597155`: the AI mode is **11**, and `r9` of `0x4a2690` is the mode;
  4. it schedules AI event 2 with `0x48b890` and `0x48b720`.

  `0x5cd060` is the other half: when state 56 is **absent** (`0x3351b0(unit, 0x38) == 0`) it calls `0x4a2690(game, unit, ..., 0)` (mode 0) and reschedules. So the flee ends by itself when the state expires. Building state 56 by hand with the section 6 stat-list recipe is neither needed nor the engine's way (it would have no expiry). `0x596d40`, which an earlier draft named the mode 11 setup, is a spot and room search and is not used.
  `r9` and `arg5` are not read by `0x597030` (`r9b` is overwritten at `0x59705b`, no read of the `arg5` slot); only `arg6` is. Callers pass `0x3b5160` skill-calc results for `arg5` and `arg6` (`0x45e971`) or `0x14, 0x14` (`0x583dcf`, with `r9 = 0x82`).
  **Needs game for the flee:** what the two bail-outs reject (`0x34c730` reads stat `0x36`; a flag byte at `+0xf0` of the row `0x975b0` returns (not the monstats row), masked by a global; and, since its last arguments are 1, bits at `+0x3c` and `+0x3e` of the row `0x976e0` returns; Terror-like effects classically skip bosses and uniques, which matters if a Treasure `Spawn` is a superunique); the meaning and unit of `arg6` (frames or ticks); whether `source = NULL` is accepted; whether a monster whose monstats AI does not flee actually runs.
- **Drop on death (dump + SDK).** `MonsterDeathListener` has no unit pointer, so it can only observe the death. To drop a TC at
  the monster use the engine: `0x441300` rolls a drop for a monster unit from a TC index; `0x447b80` is the death caller. The TC
  name goes to the `u16` index through the name map at `*(dt+0xd70)` (**dump**): both `0x3a79c1` and `0x3a94ed` do `mov rcx, [dt+0xd70]; call 0xa121f0` (the lookup section 11 uses) and use the result as an
  index into the `dt+0xd78` vector (`0x150`-byte rows, count at `dt+0xd80`), the same vector `0x397290` indexes (`0x3972aa`, `0x3972dd`). Section 6's "pd2-sourced: inference" for this route is settled here.
  SDK: `TreasureClasses` (76) rows are reachable by row index only (`findRowById`), so a name needs the map anyway.
- **Roller contract (dump).** `0x4404f0(game, a, b, tcPtr, 6 stack args)`: `rcx` game, `rdx` and `r8` the two units (the monster and `[arg+0x10]` on the monster path, `[rdi+8]` and `[rdi+0x10]` on the chest path), **`r9` the compiled TC pointer**, not an index. The pointer comes from
  `0x397290(dataFlag, tcIndex, level)` (monster path, `0x441438`) or `0x396430` (chest path, `0x5932ca`); a null pointer skips the roll. The chest path passes `[rsp+0x48] = 6` (`0x5932fb`) and out buffers through `[rsp+0x38]`/`[rsp+0x40]`, then reads `[rbp-0x11]`. A `Drop` route therefore needs `0x397290` first, then `0x4404f0`, or `0x441300` which does both.
- **Chest `Drop` (dump).** A chest does **not** take its TC from its objects row: `0x5930b0` picks it by act, difficulty and
  area-level tier through `0x396430`, then rolls with `0x4404f0`. A stock chest cannot be told a TC. Route: place a chest, track
  its id, and on open roll the `Drop` TC in addition. **Which hook:** section 6 Q3 already recommends recognising a placed object by unit id in a `0x592c40` hook. `0x5930b0` is not a good choice: it has 28 direct callers
  (3 inside `0x592cf0`, 10 between `0x58e51f` and `0x58e7b9` in or next to the chest handler, the rest elsewhere), so it is the shared object-loot path and a hook there sees far more than chests. Adopt **`0x592c40`** (one hook for chest and any later shrine or well reward), with `0x58e2f0` as the narrower alternative if the dispatcher hook proves too broad.
  Which return value of the dispatcher means "opened" is not read (needs-game).
- **Fallback (SDK).** `ItemService::createItem` with `ItemContainer::Ground` and `Placement::Exact` (`item.h`) places a chosen item at a ground coordinate for a given player. It rolls no TC, but it needs no engine call: combine it with the SDK `TreasureClasses` rows and the tracked spawn spot (`MonsterDeathEvent` has no position). Use it as the `Drop` fallback if the `0x441300` route fails.
- **Evidence:** dump for the flee mechanism, the name map, the roller contract and the chest pick; SDK for the fallback. **Needs game:** flee works with `0x597030` and the chosen args and for how long; `0x441300` with a `Drop` TC from a hook drops at the right place; the
  chest open path reaches the hook with the tracked id.

### Q3. Shrine: place an object by objects row at a free spot; resolve a name

- **Place (dump).** The handler `0x4022a0` is the recipe (it is registered under the command string `SpawnObject`; an earlier draft called it `ospawn`): find a free spot with `0x402f50` (the unit's room from
  `0x34b440`, a pointer to an `(x, y)` pair at `[rsp+0x90]`/`[rsp+0x94]`, radius 5, flag mask `0x3e01`, a pointer for the room it lands in), check the row
  index against the count from `0x38fc70(dataFlag)`, then create with `0x490410(2, rowIndex, x, y, game, room, ...)` (call site
  `0x402423`). The create reads the same `[rsp+0x90]`/`[rsp+0x94]` as `x` and `y`, so `0x402f50` takes the pair in and writes the found spot back (inference).
  Argument order was read at the call sites; the body of `0x402f50` and the meaning of the mask were not.
- **Name to row (not settled).** In the handler the name argument goes through `0x12da3a4` and then a bound check, which looks
  like a plain number parse (the cheat takes an index), **not** a name map. No objects name map was found. Fallback
  (inference): row index equals the line order in `objects.txt` (parse as in section 9), guarded by the line count equal to
  `*(dt+0x1530)`. SDK: `Objects` (54) and `Shrines` (73) exist and are game-thread only; the SDK row layout was not examined; the compiled `objects.bin` rows are described under issue #135 below.
- **Duplicate names (rule).** `objects.txt` names repeat (many chests and shrines share one), so a name is a valid `Spawn` only when exactly one line has it. With two or more matching lines the line-order fallback does not guess: skip that spawn and log the name and the row indexes, and let the event give a numeric row instead (what the debug handler takes). The same rule and the same name route apply to Treasure's "if `Spawn` is an object" test and to the Treasure chest.
- **Evidence:** dump for the placement recipe; **needs-game** for the name route.
- **Needs game:** the `0x402f50` result is clear of walls and objects; the object works (a shrine can be used, a well heals);
  `.txt` order equals the row index; which `Shrines` row a shrine object uses.

### Q4. Boss `Drop`: extra loot from a TC on the boss's death

- **Route (dump, inference).** The same pair as Q2: hook `0x447b80` after the original for a tracked boss id (hookable by the rule, see the table) and call
  `0x441300` once more with the `Drop` TC index. The inner call at `0x447e17` passes, from `0x447b80(game, victim, arg3, info)`:
  `rcx` game, `rdx` victim, `r8 = [info + 0x10]`, `r9` the `u16` TC index (a D2RCore thunk result, `0x3e2b45a`, kept in `r12w`), and on the stack `+0x20 = 0`, `+0x28` a **local** (the result of `0x43e8a0(victim, level id)`, written at `0x447c68`),
  `+0x30 = arg3` (its slot `[rsp+0xc0]`) and `+0x38`, `+0x40`, `+0x48 = 0`. So only `game`, `victim`, `r8` and `arg3` come from the arguments; the TC index and the `+0x28` local are computed inside, and a hook has to compute or choose them itself (inference).
  How `0x447b80` picks the TC for a superunique (through `0x3e2b45a`) was not resolved, which decides whether to add to it or replace it. Without a unit pointer the SDK listener cannot do this.
- **Evidence:** dump for the call shape, inference for the hook. **Needs game:** extra items drop once at the boss with the
  boss's level and no double drop; whether `0x441300` early-outs on a monster that has already dropped.

### Summary

| Q | Answered | Level |
|---|---|---|
| 1 Ambush | yes: per-tick room test, waves tracked by `MonsterDeathListener` ids plus a time-out | SDK + inference |
| 2 Treasure | flee found: call `0x597030` (mode 11, state 56 set by the engine); `0x441300` and the roller contract found; chest route through `0x592c40`; effect unproven | dump + needs-game |
| 3 Shrine | placement recipe found; name to row route open, duplicate rule set | dump + needs-game |
| 4 Boss `Drop` | route chosen, args split, superunique TC pick unresolved | dump + inference |

### Needs game

1. Does the player-in-room test fire on walking into the event room, does the listener id match the spawned unit's `+8`, and does `SpawnMonster` work from the `PlayerTick` context?
2. Does `0x597030` make a spawned monster run: what do its two bail-outs reject (uniques and bosses), what is `arg6` (the duration unit), and is `source = NULL` accepted?
3. Does a second `0x441300` call with a `Drop` TC drop at the right place and level, once, for a boss and a Treasure monster, and does it early-out on a monster that has already dropped?
4. Does a chest open reach a `0x592c40` hook (or `0x58e2f0`) for a tracked chest, which return value means "opened", and can a second TC roll be added there?
5. Is the `0x402f50` spot safe, and does `0x490410` create a working shrine, well and chest?
6. Does `objects.txt` line order equal the row index, and does the count match `*(dt+0x1530)`?
7. What is the objects row layout (the `Id` offset) for an SDK scan, and which `Shrines` row does a shrine object use?
8. Is the thread that runs the tick the one the SDK treats as the game thread?
9. The per-wave time-out value.

### What was built (issue #111, phase 6b first half): written, not verified in game

The Shrine event kind and the Boss `Drop` column. Nothing here has run in a game; the unit tests cover only the pure logic and the signature checker only the dump.

- **Parts** (`core/part_signatures.h`, own group in `engine/event_6b.cpp`, each through `ResolvePart` with its per-part guard, a part that does not match is logged and left off): `DeathDrop` `0x447b80` (30-byte signature, hooked: the first 15 bytes are the guard), `DropRoller` `0x441300`, `DropLevel` `0x43e8a0`, `SpawnObject` `0x4022a0` (the handler, only to find the free-spot call), `CreateObject` `0x490410`. `DataTablesBase` and `NameToRow` are reused from section 11 (the addresses found by `event_apply.cpp` are handed over, not scanned for again). All are checked on the dump by `check_portal_signatures.py` (`--stamp 0x6ab3782c`, "all signatures ok").
- **`0x402f50` has no unique signature** (its prologue is shared). It is resolved as the E8 call target at `SpawnObject+0xff` (the call in the debug handler), guarded by 17 context bytes before the call and 20 prologue bytes at the target; the checker has a `check_free_spot` for it. This is the "call target at `0x40239f`" route; the target is `0x402f50` on this build.
- **Shrine.** `Spawn` is an objects.txt `Class` (tried first; all 587 are distinct, e.g. `ShrineHealth` is row 172) or, when no Class matches, a `Name` (read by `plugin.cpp` into `Tables::objectClasses` and `objectNames`, one entry per row, the `Expansion` marker line left out). A match is valid only when exactly one line has it (exact case first, then any case; a case-insensitive Class match wins over an exact unique Name; a shared Name such as `Shrine`, on 49 lines, is refused with the rows listed) and the file's line count equals `*(dt+0x1530)`; otherwise `Spawn` can be a row number below the game's count (a line named with digits wins). `ParseTsv` drops blank lines, so a file with one fails the count and the Shrine is then given by number. Placement: when the event room is populated, `0x402f50` finds a free spot (in/out `{x,y}` pair, tried at the room centre and a quarter in from each edge with radius 5, then 2, then 1, mask `0x3e01`; see "Fix" below, `&outRoom`, null means none), then `0x490410(2, row, x, y, game, room, 1, 0, 0, 0)`. If the free spot lands outside the event room (`landed != room`), the object is not created and the `NOT in the event room` line is logged, since the neighbouring room may not be populated yet. A Shrine that cannot resolve is logged and neither announced nor spawned. A Shrine's `Spawn` is never looked up as a monster name.
- **Boss `Drop`.** Route chosen: hook `0x447b80` (the original runs first) and call `0x441300` once with the `Drop` treasure class. The TC name goes through `NameToRow(*(dt+0xd70), name, 0)` (count at `dt+0xd80`); index 0 is refused, and the index must fit 16 bits and be below the count. The level argument is the result of `0x43e8a0(victim, level id)` (the game's own local, passed as stack argument 6). For a monster `0x43e8a0` ignores the level id and returns `max(1, stat 0xc)` (dump `0x43e8af`-`0x43e8c2`), so the boss's level is its own stat whatever id is passed, which settles that part of needs-game item 2; the other arguments are forwarded from the hook's own (`arg3` as stack argument 7, the killer read from `info` as the third argument, the TC index as the fourth). The boss is the event's first spawn, remembered by unit id, and rolls at most once. The Drop is disarmed on every map open (a new map, event or not, forgets the previous boss), so a boss from an earlier map no longer rolls; a boss killed after the player left its level, while that map is still the last one opened, still rolls once; any other unit, a non-monster, a missing killer or a disarmed event is skipped. The hook installs only when every part it needs was found (never half on) and chains with any other plugin's hook. The SDK `createItem` fallback was not built.
- **State** (shrine row, tracked boss id, dropped flag) is cleared in `EventsGameLeft`; the hook is installed at load only.
- **Unit tests** (`event6b_tests`): objects name by line order including a blank line, exact then any case, the duplicate-name rule (rows listed), the count mismatch, numeric rows, the TC lookup edge cases, the drop decision matrix and the call-target decoder.
- **Nits on issue #102, now corrected in place** (table, Q2 and Guard above; kept here for the record). (1) `0x34c730` takes five arguments, called as `(target, 0, 1, 1, 1)`; the fifth tests the `+0x3e` byte of the `0x976e0` row, and `+0xf0` comes from the `0x975b0` row, not the monstats row. (2) Record the `0x34e0c0(target, source)` call at `0x5970d1`: whether `source = NULL` is accepted stays an open question for Treasure. (3) The Guard field mixed conventions: `+0x14` is an instruction start, `+0xf` a displacement start; it now gives both for each.

Log lines (all `eventprobe:`): `shrine: Spawn '..': ..` (the name resolution), `shrine: objects row N, free spot near (x,y) from room R`, `shrine: free spot (x,y) in room R (in / NOT in the event room)`, `shrine: created objects row N: unit .. at (x,y)`; `boss drop: Drop '..': name map .., the game has N treasure classes, lookup gave I`, `boss drop: tracking unit ..`, `boss drop: the tracked boss .. died: <reason>`, `boss drop: rolling '..' (treasure class I) for unit id ..: killer .., level value .., arg3 ..`, `boss drop: the roll returned`.

#### Needs game (phase 6b)

1. Does `*(dt+0xd70)` resolve a stock treasure class name (case, the `-1` miss, index 0 refused), and does the count at `dt+0xd80` match?
2. Does the second `0x441300` call drop once, at the boss, at the boss's level, with no double drop and no early-out for a monster that already dropped? Is the killer from `info` the right third argument (`r8`), and is `arg3` right as stack argument 7?
3. Does the death hook see the boss (unit id and `+0` type) and run on the game thread, and does a superunique's own TC still drop?
4. Does `objects.txt` line order equal the row index on the mod in use, and does the count match `*(dt+0x1530)`? Does a numeric `Spawn` work when it does not?
5. Is the `0x402f50` spot clear of walls and objects and inside the event room (the `in / NOT in` log line; a spot outside it is refused, so if `NOT in` is common the radius or centre needs work), and does `0x490410` create a usable shrine, well and chest? Which `Shrines` row does a shrine object use?
6. Does `PopulateRoom` run the Shrine placement once for the event room, and in a Cave or Crypt map at the reserved cell?
7. Multiplayer is not covered; see section 12.

### What was built (issue #118, phase 6b second half): written, not verified in game

The Ambush and Treasure event kinds. Nothing here has run in a game; the unit tests cover only the pure logic and the signature checker only the dump.

- **Parts** (`core/part_signatures.h`, in `engine/event_6b.cpp`, each through `ResolvePart` with its per-part guard; one that does not match is logged and left off): `OperateObject` `0x592c40` (hooked, 16 bytes = four whole instructions, the first relative operand is the call at `+32`), `FleeSetup` `0x597030`, `FleeBlock` `0x34c2c0`, `FleeTarget` `0x34c730`, `TcLookup` `0x397290`, `TcRoller` `0x4404f0`. All six are unique on the dump and checked by `check_portal_signatures.py` (`--stamp 0x6ab3782c`, "all signatures ok"). Reused from the first half: `DropLevel`, `DropRoller`, `DataTablesBase`, `NameToRow`, `SpawnObject`/`FreeSpot`/`CreateObject`.
- **Routing** (`event6b::routeEvent`, pure): Boss, Horde, Ambush and Shrine route to themselves. A Treasure routes to *monster* when its `Spawn` resolves in monstats or superuniques, else to *chest* (its `Spawn` is then an objects.txt name or row, resolved exactly like a Shrine's). A Treasure whose `Spawn` is a misspelt monster therefore ends in the chest path and is refused there with the objects lookup's reason.
- **Ambush.** The room populator hook plans one wave (`Count` spots in the event room, `planEvent`) and arms the Ambush; nothing spawns yet. `EventsPlayerTick` (the existing `PlayerTick` hook, local player only) calls `Event6bAmbushTick` with the player's room; when its server rect (read through `ReadRects`) equals the event room's rect, remembered at arming (no room pointer is kept between ticks, so a freed or reused room cannot be mistaken for it), the first wave is spawned with `0x492cf0` (`SpawnOne`, the same call as a Horde). `Waves` waves run one after another: a wave ends when every unit id it made has been reported by the SDK `MonsterDeathListener` (server thread, after the normal drop) or after a 90 s time-out (`kWaveTimeoutMs`, a guess); the next wave starts 1.5 s later and only while a player is in the room. If the game makes no unit for a wave the Ambush is abandoned with a log. A unit that dies while its wave is still being spawned is remembered and counted when the wave is registered. The Ambush counts as placed, and its banner is shown (late, over the map name), when its first wave made a unit; it is not announced when armed. An Ambush is refused (logged) when the `PlayerTick` or room parts are off, since nothing could start a wave. Without the listener (no Lifecycle service, or it refused) only one wave runs, ending on the time-out. All of it is in `event6b::WaveTracker`, tested with a fake clock.
- **Treasure monster.** The monster (a superunique if named, as for a Boss) is spawned as the event's first spawn. Then `0x597030(game, source, target, 0, 0, 600)` is called: `source` is the player unit the map was opened for, checked again by type and id (else NULL, logged), `600` is the last argument, which the dump shows stored at `+0x60` of a 0x70-byte request struct (its unit is not known). Before the call the two bail-out checks are run and logged (`describeFleeChecks`) so a refusal is explained, and the unit's mode is logged before and after. Its death rolls `Drop` through the Boss Drop hook (`0x447b80` then `0x441300`); a Treasure with no `Drop` still flees.
- **Treasure chest.** Placed by the Shrine code (`Event6bPlaceShrine`), then tracked by unit id. The `0x592c40` hook runs the original and then, for the tracked chest only, compares the unit's mode (`+0x0c`, read as int32; the offset is from the classic layout and is logged on tracking) before and after: if it changed and nothing was rolled yet, `Drop` is rolled once: `0x43e8a0(chest, ...)` gives the level value (for an object the level argument is ignored and the area level of the chest's own room is used), `0x397290(data flag, tc index, level value)` gives the compiled class (null: logged, no roll), then `0x4404f0(game, chest, activator, class, 0, 0, 0, &out[0x30], &zero, 6)`. Stack arg 5 is a **forced item quality**: 0 rolls the quality with `0x4421b0` from the tier (arg 6), a nonzero value goes straight to `0x444680` as the quality (`0x440f9f`..`0x440fde`). `0x5930b0` forwards its own second argument there: the chest handler passes 0, 7 or a variable (`0x58e5a6`, `0x58e709`, `0x58e51f`); only `0x592cf0` passes 4. So the plugin passes 0 (a first version passed 4, which would have made every `Drop` item magic). The tier (arg 6) is derived by `0x5930b0` from the area level and the act (`0x593282`..`0x5932b1`, after `0x34b440`, `0x2efc10`, `0x3e2b5c8`, `0x508f80`, `0x32c700`); that chain is not reproduced, so the tier is `0` (the lowest). Arg 10 is not a flag but the most items written to `out` (`0x44114f`..`0x44115f`; `0x440614` asserts it is above 0); the 0x30-byte buffer holds 6 pointers, so a `Drop` TC that gives more than 6 items is cut at 6. The room-level use is at `0x43e8dd`..`0x43e917`. The "mode changed" test is used because the dispatcher's return value is not understood (it returns 0 when the handler returned nonzero, else 1); the value is logged. A chest with no usable `Drop` is still placed, as the game's own object.
- **State** (Ambush tracker and room, chest id and rolled flag, flee source, an epoch counter) is cleared at every map open (`Event6bMapOpened`) and in `EventsGameLeft`; a wave spawned across such a reset is dropped. Both hooks and the listener are installed at load only; the operate hook installs only when every part its roll needs was found.
- **Call sites in `event_apply.cpp`** are minimal: `Event6bPlacesObject` replaces the Shrine test, `Event6bArmAmbush` before the spawn loop, `Event6bFirstSpawned` replaces the Boss-only call, `Event6bAmbushTick` in the tick. `planEvent` now plans an Ambush wave and a Treasure monster; only Shrine and Invalid still give "not applied yet".
- **Unit tests** (`event6b_tests`, `eventplan_tests`): routing, the wave tracker (wait for the room, clear by ids, gap, last wave, time-out, empty wave, restart/reset), the chest decision matrix, the flee check text, the Ambush and Treasure plans.

Log lines (all `eventprobe:`): `ambush '..': armed N wave(s) of M in room R ..`, `ambush '..': wave N of W: the game made X of M units in room R (game G)`, `ambush: unit id .. of wave N died, K left`, `ambush: a death during wave N was not one of its units ..`, `ambush '..': wave N of W ended: cleared / timed out`; `treasure monster: Drop '..': name map ..`, `treasure monster flee: unit .., source .., duration argument .., 0x34c2c0 .. 0x34c730 .. : <should set the flee / will return without ..>`, `treasure monster flee: the setup returned; the unit's mode A -> B`; `treasure chest: Drop '..': name map ..`, `treasure chest: tracking unit .. (id .., mode ..)`, `treasure chest: the operate call on the tracked chest .. returned R, mode A -> B: <reason>`, `treasure chest: Drop '..' (treasure class I): data flag .., level value .., compiled class ..`, `treasure chest: the roll returned ..`.

#### Needs game (phase 6b, second half)

1. Does `PlayerTick`'s game pointer work for `0x492cf0`, and does the player's room pointer equal the populator's event room (the `armed .. in room R` and `wave 1 ..` lines, `game G`)? Does the listener's id match the spawned unit's `+8`?
2. The per-wave time-out (90 s) and the gap (1.5 s) are guesses; does a wave that the player leaves behind feel right?
3. Does `0x597030` make the monster run? The log says whether the two checks pass (uniques and bosses are expected to be refused by `0x34c2c0`/`0x34c730`) and the mode before and after. Is `source = NULL` accepted, and what unit is the last argument (600 is a guess)?
4. Is `+0x0c` the mode, and does it change when the chest is opened? Which return value of `0x592c40` means "opened"? Does the roll at the `0x4404f0` shape (forced quality 0) drop at the chest, once, with the tier `0`, or must the tier be derived (the engine computes `0..2` from the area level in `0x5930b0`)? Is the quality of the items normal?
5. Does the chest's own loot (the object's row in objects.txt) still drop as well, and is the extra `Drop` not rolled twice if the chest is operated twice?
6. Multiplayer is not covered; see section 12.

### Game checks of 2026-11-12 (issues #111 and #118, phase 6b)

**Level of evidence:** live in game, one operator-style run, six games (one per event, the Shrine twice: by name, then by row). Slot 11 (test mod D2RToolsTest, save `cartos`, Barbarian in `cheat god`), build 93847, D2RLoader 1.3.1-beta, Normal, Cartographer built from `origin/main` at `888ab3f` (`tools\build.ps1 -Plugin cartographer -Test` passed first: unit tests are a separate level and say nothing about these results). The map was the preset slot level 138 (20 x 28 tiles, 12 rooms), a `Level 30`, `EventChance 100` map; one `mapevents.txt` row per game, swapped between games and re-read with `cartographer check`. Monsters were killed with `cheat killmonster`. Not checked on Steam build 94995, not in multiplayer. No crash report was added in any of the six games, and the plugin log has no `ERROR` line; the only `WARN` lines besides the table notes are three refusals: the Shrine's name, then `shrine not placed` for the Shrine row and again for the chest (below).

| Event | Placed | Banner | Result |
|---|---|---|---|
| Shrine (`ShrineHealth`, then row 172) | **no** | one line (map name) only, as the event was not applied | Refused twice, for two reasons below |
| Boss with `Drop` (Bishibosh, 2 minions, `Act 5 (N) Equip A`) | yes, 3 of 3 (inferred: the log gives only the superunique's position, the event room centre), 1 as a superunique | 1 line on arrival, 2 lines (with the event) once spawned | The tracked boss died and the `Drop` rolled once |
| Ambush (`fallen1`, 3 per wave, 2 waves) | yes, armed in the event room | 1 line on arrival; 2 lines logged at the moment wave 1 spawned, not when armed | 2 waves of 3, both cleared, no stray rolls |
| Treasure monster (`fallen1`, `Act 5 (N) Equip A`) | yes, 1 of 1 | 1 line on arrival, 2 lines once spawned | Flee setup ran; the flee itself is not confirmed |
| Treasure chest (objects row 147, `Chest3`) | **no** | one line only | Refused, same reason as the Shrine |

Findings:

1. **Event room rule after #121 (section 9).** Every game logged `level 138 is 20 x 28 tiles and has 12 room(s); the event room is the first room at least 9 tiles from the entry room (5016,2024) 4 x 4`, and room #4 (DRLG (5000,2024) 8 x 4, server (25000,10120) 40 x 20) was chosen in each of the five games where the event was applied ("event room by distance: THIS IS THE EVENT ROOM"; in the first Shrine game, refused before populate, room #4 is logged without that line). So the small map now gets its event with no scratch build (this closes the lowered-distance caveat of section 9 for this map; the maze event room and a rebuilt level are still not run). The distance of 9 is the third of the larger side, as designed.
2. **Shrine and chest are not created: the free spot lands outside the event room.** Both runs logged `shrine: free spot (25026,10114) in room ...EA20 (NOT in the event room)` and `shrine not placed`: the search starts at the room centre (25020,10130) and returns a spot 16 server units up, in the next room, whose y range ends at 10120. It was the same spot in both runs that reached the placement (the Shrine by row number and the chest; the first Shrine run, by name, stopped earlier at finding 3). So `0x402f50` (radius 5, mask `0x3e01`) did not give a spot inside this 8 x 4 tile room. `0x490410` was therefore never called, and needs-game items 5 (spot safe, object works, which `Shrines` row) of phase 6b, and 4 and 5 of the second half (the chest operate hook and its roll) are **not answered**. A fix belongs in the placement (a smaller radius, or a spot taken from inside the room rect), not in the guard: the guard did what it was written to do.
3. **objects.txt line count is one more than the game's row count.** The stock `objects.txt` (copied into the test mod because the plugin reads the mod's file) has 588 data rows (589 lines with the header); the game's count at `*(dt+0x1530)` is 587. The count check therefore failed and a name was refused as designed: `objects.txt has 588 lines but the game has 587 objects rows, so line order is not the row index and 'ShrineHealth' is not trusted`. A number in `Spawn` was accepted (`objects row 172 from the number in Spawn (the game has 587 rows)`). The stock file has an `Expansion` marker line (line 412, data row 410) that the table compiler plausibly skips; that fits 588 against 587 and is settled in "What was built (issue #135)" below; until it is known, no name resolves on a stock `objects.txt`, so the README's "a name used by one line only" does not hold there. Needs-game item 4 of the first half is answered "no" (line order versus count) for the stock file; and the row numbers used (172, 147) were taken as line number minus 2 and were **not** checked against what the game created, since nothing was created.
4. **Boss `Drop`.** `Act 5 (N) Equip A` resolved to treasure class 343 of 1519 (`name map`, `lookup gave 343`), so the `*(dt+0xd70)` map and the count at `dt+0xd80` work for a stock name (first half, item 1). The hook saw the tracked boss (`type 1, id 8`) die and rolled once: `the tracked boss (unit .., id 8, type 1) died: rolled`, `rolling 'Act 5 (N) Equip A' (treasure class 343) for unit id 8 after the game's own drop: killer <player unit>, level value 5, arg3 0`, `the roll returned`. Only one roll line appeared (no double roll), and no crash. The level value was 5, which is inferred to be the boss's own level (the log gives only the number; the map's `Level` is not applied to monsters), so the "level from `0x43e8a0`" part is consistent with it, and the killer from `info` was a real unit. Not settled: where the items dropped, their level and **quality**. On screen the ground labels read only `ITEM LEVEL: 1` (the test slot's loot filter hides names), and the boss's own drop and the room's normal Fallen drops were mixed in, so whether `Drop` items look normal and not all magic **was not seen**. A superunique's own treasure class dropping as well (item 3) was not checked either.
5. **Ambush.** `armed 2 wave(s) of 3 in room ..., the first comes when a player is in that room (time-out 90000 ms, gap 1500 ms)` appeared at populate time and nothing spawned until `cheat goto 138,25020,10130` put the player in the room; then `wave 1 of 2: the game made 3 of 3 units` and the banner's second line (the event) were logged in the same second, after the wave, not at arming. So the per-tick room test works (item 1: the player's room equals the event room, the `PlayerTick` game pointer works for `0x492cf0`, and the SDK listener's ids match the spawned units: `unit id 15 of wave 1 died, 2 left` ... `0 left`, then `wave 1 of 2 ended: cleared`). Wave 2 started 1.5 s later (`wave 2 of 2: ... 3 of 3`), deaths of other units were logged as `not one of its units (class 19)`, and `finished after 2 wave(s)`. The wave count is 2 as configured. The 90 s time-out was not reached, and whether a wave the player leaves behind feels right (item 2) was not tried.
6. **Treasure monster.** The plain `fallen1` (not refused by `0x34c2c0` or `0x34c730`: `passes; passes: 0x597030 should set the flee`), `source` a player unit, duration argument 600. The setup returned and the mode stayed `1 -> 1`. A few seconds after the spawn the event room had no monster in it when the player was warped in, and a Fallen was seen walking away at the top of the screen in the second shot, so it may have run, but the unit was not identified, its position not read, and it was not killed afterwards, so neither the flee (item 3) nor its death `Drop` is confirmed. The mode read right after the call (1) is not the run mode (11 by this section's static reading), so it may be set a tick later.
7. **Banner.** As in section 9, the overlay draws and logs `entry message shown on the overlay`, but `PrintWindow` does not capture it, so the text and its fade were not seen. Timing from the log: the map-name line on arrival, the event's lines when the Boss, Treasure or first Ambush wave spawned (the Ambush not when armed).

Needs game, after these checks: still open are the Shrine and chest creation (finding 2 first), the chest operate hook, the chest's own loot not being lost and no double roll on a second open, the quality of `Drop` items, the flee itself and a Treasure monster's death drop, the object count (finding 3), a superunique's own treasure class, the Ambush time-out, and multiplayer.

### What was built (issue #135, phase 6b placement fixes): written, not verified in game

Two causes found by the game check above; both fixed in the plugin, neither seen in game yet.

**Row count (cause, dump and tables: static).** The stock `objects.txt` has 588 data lines; line 412 is a marker with `Class` = `Expansion` and every other cell empty. The compiled `objects.bin` (D2RLBIN, header count at offset `0x28`) holds 587 rows, which is 588 minus that marker, so the table compiler takes no row for it (inference from the counts; the marker is the only line of its kind). The `*ID` column agrees with the position among the other lines up to line 575 (`ID` = line number - 2 before the marker, line number - 3 after it) and drifts after that, so it is not used. Consequence: the line number minus 2 is the row only before the marker (the earlier rows 172 and 147 are unaffected), and one less after it (`ClothChestR` at line 416 is row 413). `plugin.cpp` now reads `Class` and `Name` and `event6b::dropMarkerLines` drops `Expansion` lines before the lookup; a count that still differs (a modded file with a blank line, say) is refused as before. **Table evidence (static, vanilla 3.3.0 excel and the `0x6ab3782c` dump, from the review of this change):** `objects.bin` stores its rows inline from `0xe51`, `0x168` bytes each, `Class` at +0 and `Name` at +0x40; all 587 rows match the Nth non-`Expansion` line of `objects.txt` on both Class and Name (0 mismatches; counting the marker line instead gives 177). Around the marker: row 409 `PandemoniumBrazier2`, row 413 `ClothChestR`. **Name vs Class:** in the stock file `ShrineHealth` and `Chest3` are `Class` values, not Names; `Name` repeats (`Shrine` on 49 lines, `chest` on 39; only 138 of 587 Names are unique), so the lookup tries `Class` first (rows 172 `ShrineHealth`, 147 `Chest3`) and uses a Name only when it is unique. **Same marker elsewhere:** `monstats.txt` has 752 lines against 751 rows, `Expansion` at data row 410; `superuniques.txt` has 67 against 66, `Expansion` at row 42. `ReadSpawnNames` (the txt fallback, also the txt-only superunique path in `event_apply.cpp`) now drops both through the same helper (unit tested), where before every row after the marker (all Act 5 monsters, superuniques from row 42) was one off.

**Free spot (cause, dump: static).** `0x402f50(room, &xy, radius, mask, &outRoom)` reads the room's rect (`0x2efb40`), then scans squares around the start point: for `k` = 1 to 100 (one step each), offsets `-k..k-2` in steps of 2, negative first, so each ring starts at the upper left and `(x, y)` itself is first tested at `k` = 2; the bias toward negative offsets is why a start in the upper part of a room leaves it early. Nearest-first by square and not by distance. The footprint is centred on the spot. Its two native callers (`0x401ecc`, `0x40239f`) both pass radius 5, with masks `0x100` and `0x3e01`. Each cell is looked up in its own room (`0x38dfb0` finds the neighbour when the cell is outside the rect) and tested with `0x364b20(room, x, y, radius + 2, radius + 2, mask)`, which tests a `(radius + 2)` square footprint (a footprint of 1 or less is one cell); the first clear one is returned together with its room. So the radius is a footprint size, not a search distance, and nothing keeps the result inside the start room. With radius 5 (a 7 x 7 footprint) no clear spot existed within about 15 subtiles of the centre of the 40 x 20 event room, and the first hit was 16 up, in the next room (the game check's `(25026,10114)`). Why the footprint never fitted (walls, objects, or collision not yet built at populate time) was not examined and is **needs game**.

**Fix.** `event6b::freeSpotAttempts` (pure) gives the tries: for radius 5 (the engine's own), then 2, then 1 (a 3 x 3 footprint; 0 is only 2 x 2 and would leave one cell of a 3-wide `Chest3` untested), the room centre and the four points a quarter in from the edges. The engine takes the first try whose result is in the event room (`outRoom` is the event room and `spotInBox` holds for the rect, in the rect's own units); the "NOT in the event room" refusal stays, now after the last try. Nothing new is called, so no new signature. Log lines: `shrine: objects row N, free spot try K: near (x,y) radius R ... gave a spot (x,y) room P (in / NOT in the event room)`. Unit tests: `objectRowNames`, `freeSpotAttempts`, `spotInBox` (`event6b_tests`). `check_portal_signatures.py` and `check_maze_signatures.py`: all signatures ok.

**Needs game:** a stock `objects.txt` Class (`ShrineHealth`) resolves and the row logged matches what the game creates; a spot is found inside the event room (and at which try and radius); the created object works (a shrine can be used, a chest opens and its `Drop` rolls); a chest of 3 x 1 does not sit inside a wall at radius 1.

### Containment: player stats removal and hooks without an original (issue #155): written, not verified in game

No new address. Two changes so the map's effects never reach a vanilla level.

- **Player stats list removal (`RemoveFromPlayer`).** The record of the list (`g_lists`) was cleared first, and a list the game's finder could not return by its state was left on the player: the map's player stats then followed the player into vanilla levels. Now the list is looked up on the unit's chain first. `statplan::decideRemoval(onChain, foundByState, ownedByPlugin)` decides: not on the chain, forget it and touch nothing (it may already be freed); on the chain but the list's own fields are not ours, forget it and touch nothing (the game freed ours and reused the address, a game list sits there now); on the chain, ours, and found by its state: state bit off, detach, free (the old order); on the chain, ours, but not found by state: detach and free and leave the state bit alone (a log line says so). "Ours" is `statplan::ownsList`: list `+0x1c` flags is 0 (the plugin makes its lists with flags 0, the engine's use flags such as 2) and list `+0x20` is the state the plugin gave it (section 6, "Stat list: `+0x1c` flags, `+0x20` state"). The fields are read only after the chain walk found the address, so the memory is live. Chain membership alone is not the check, it proves only that some list lives at that address. `g_lists` is cleared on every path after the attempt; nothing is retried. On the detach-and-free-anyway path the state bit `S` stays set with no list behind it (clearing it could clear a state a game list carries, since the finder returned another list or a part is missing); the next map's `ApplyToPlayer` then finds the state in use and keeps player stats off for that map.
- **Hooks that come back without an original.** `hookguard::classifyHook(installed, haveOriginal)` is now the one decision for the LevelBuild, Maze (ring, entrance and exit, walk), PlayerTick, ItemSpellUse/Check, MonsterSetup, Populate, DeathDrop and Operate installs. Only a Live hook turns its part and feature on and lets later hooks be built on it. The SDK (`hooks.h`, `context.h`) has no call to remove an inline hook, so when the loader installs a hook without an original, `carto::HookOutcome` (`scan.cpp`) writes the part's guard back over the entry: `VirtualProtect` RWX, `memcpy` of the guard (the signature's own exact `hookSize` bytes, which `ResolvePart` refuses to accept with any `??` in that range; `Part::guardExact` records it), restore the protection, `FlushInstructionCache`. The game's own function then runs again; the loader's trampoline is left unused. The write happens at plugin load, inside the install call, before any game thread is in these functions and before another hook is chained on the part. `hookguard::canRestoreHook` decides (NoOriginal only, `hookSize` not 0, fits the 32-byte guard, every byte exact); otherwise, or when `VirtualProtect` fails, it is logged as an error that the hook cannot be taken out and the function stays skipped until the game restarts. Either way the part and its feature stay off. Not seen in game: the loader has not been seen to return no original.

- **Ownership check.** The list's `+0x1c` and `+0x20` are read right after `SetPlayerState` and stored with the list; removal compares the live values with those, not with 0, because `AttachStatList` (a D2RCore thunk, unread) may set flag bits. They are logged in the `statprobe: player ... entered the map` line and in the mismatch warning.
- **Restored hook.** D2RLoader still lists a restored range as hooked for the session (diagnostics show it with call-through No) and refuses other plugins that entry; on unload it restores the same original bytes.

**Needs game:** `PlayerStats=1`, leave the map for a vanilla level, confirm the sheet stats are gone and read the list flags in the `entered the map` probe line; a save and an autosave inside the map before `PlayerStats` can default on (it stays off).

## 14. The map item's own stats (issue #158)

Build 93847, stamp `0x6ab3782c`, static (dump); not run in a game. With `ItemStats=1` the plugin reads the used item's stat list in the item-use hook, before `OpenRedPortal`, and feeds it to the map stat plan in place of the draw. The SDK has no getter for a unit's stats (`item.h` gives only `ItemInfo`), so two parts are called.

| Part | RVA | Role | Signature (offset 0 = function entry) | Guard / evidence |
|---|---|---|---|---|
| `ItemListFind` | `0x2f5b80` | `(unit, state, flags)`: the list finder of section 6. `state` 0 and `flags` `0x40` walk the unit's owner-list chain (`+0x90`) and return the first list with `(list+0x1c & 0x40 & 0xffffdfff) != 0`, or null | `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B F8 8B F2 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 8B CB E8 ?? ?? ?? ??` | whole signature under its mask, unique in the executable sections; `check_portal_signatures.py` ok; body read at `0x2f5b80` to `0x2f5c49` |
| `ItemListCopy` | `0x2f64e0` | `(list, out, max) -> count`: D2RLoader's thunk (`jmp [rip+x]`, four NOPs) into D2RLoader's replacement (`[0x3e2a300] = 0xc0de58328e0`, outside the image; its body was not read) of up to `max` pairs from the list into `out`. The dead body behind the thunk (`0x2f64ea`) is evidence for the original only: it reads the entry array at `list+0x30` and the count at `list+0x38`, copies `min(count, max)` pairs of two int32 (`{stat << 16 \| layer, value}`), returns the count copied | `FF 25 ?? ?? ?? ?? 90 90 90 90 57 48 83 EC 20 49 63 F8 48 8B DA 48 8B F1` (24 bytes, unique) | called only. The caller is the client sender `0x53a210` (call at `0x53a4de`, `r8d = 0x10`), which takes the stat as `sar eax, 0x10` of the first int32 (`0x53a536`); that is the order used here. The layer being the low 16 bits is inference. Why the `max` / 8-byte contract holds for the replacement: the image has 17 live `E8` callers of `0x2f64e0` passing `(list, buf, max)` with buffers of `max * 8` (for example `0x33eab4` and `0x33eb79` with `max 0x1ff`, `0x535178` with `0x200`, `0x423017` with `0x80`, `0x47cb3e` and `0x53a4de` with `0x10`, `0x1685c9`, `0x16941e`, `0x51cc03` and `0x51cf58` with `0x3fe`); a replacement that overran `max` would break the game's own callers. The original asserts (`0x2f1750`, then `int3` if the reporter returns true) when the count is at least `max`, so the plugin passes `max = 0x200` with a buffer of 0x200 pairs (4 KB, as `0x535178` does) and cuts to 16 itself |

- **Which list:** the base magic list, `0x2f5b80(item, 0, 0x40)` (created with flags `0x40` at `Items.cpp:0x1900`, section 6 C). Lists on states 165 to 169 and the runeword list are not read. That a map item's affixes land in this list is inference. The match ignores only bit 13, so a state list created with flags `0x2040` (states 165 to 169) also matches `0x40`, and so would the runeword list if it carries `0x40`; if such a list comes first on the chain it is returned instead of the base magic list. Unlikely for a map item.
- **Limit:** 16 stats (the sender's own). The copy asks for up to 0x200 and the plugin cuts to 16; a longer list is cut and the log says so (exact for 17 or more).
- **Fail open:** a part off, no list, an empty list or no matching stat leaves a normal draw. Correction to section 6 C: the entries are 8 bytes (`{key, value}`), not 16; the plugin never reads the list's memory itself.
- **Mapping (unit tested):** each item stat is matched by id to the first `mapstats.txt` row (built-in pool when there is no file) whose `Stat` has that id in `itemstatcost.txt`; that row gives the target. Stats with a layer, a value of 0 or no row are dropped and logged. Values are used as they are (the game's raw units, as for `SetListStat`).
- **Needs game:** a map item with known affixes (for example `item_maxhp_percent` 30 and `fireresist` -20, both in `mapstats.txt`) opens with exactly those properties in the log line `ItemStats: 2 item stat(s) read, 2 used`; an item with no stats draws as usual; the item is still intact at the read (the read is before `OpenRedPortal`); an item with 16 and one with 17 or more stats (the note fires only above 16); that the returned list is the affix list and not a state list.

## 15. Map monster columns in the slot level's row and region record (issue #156)

Build 93847, stamp `0x6ab3782c`, static (dump); nothing here was run in a game. Code: `plugins/cartographer/src/engine/level_row.cpp` (game glue), `src/levelrow/` (pure: layout, guards, plan, ledger, undo; `tests/levelrow_tests.cpp`). The first route wrote every column into the levels row only; the review showed the spawn code reads a per-game copy, so the route is now two spaces: the row for `Level`, the slot level's region record for `Density`, `Uniques` and `Monsters`.

### The row (`Level` only)

The levels.txt loader (`0x32eac0`..`0x33061a`) builds one descriptor per column, `0x20` bytes each (`+0` name, `+8` type, `+0x10` row offset). `Id` is at `+0x04` (2 bytes); `MonLvl`, `MonLvl(N)`, `MonLvl(H)` at `0x18`, `0x1a`, `0x1c` and `MonLvlEx`, `MonLvlEx(N)`, `MonLvlEx(H)` at `0x1e`, `0x20`, `0x22` (2 bytes each). `Level` writes these six. It works because `0x32c700` reads the row's `MonLvl` live on each call (static). The scan of `.text` for displacements `0x2dc` and `0x2e0` shows the record's own MonLvl copy is only written at `0x508973` / `0x508987`, which is why the spawn-time level comes from the row.

### The region record (`Density`, `Uniques`, `Monsters`)

The record array is built once per game: `0x5098b0` (called from nowhere else) calls `0x508750` for every level `1..count-1` and stores the records in the array at `*(game + 0x1d0)`; `0x508740(array, level)` returns `array[level]` (also used at `0x508d5f` and `0x50ad3a`). A record is `0x9f0` bytes.

| Field | Record offset | Size | Evidence |
|---|---|---|---|
| copy of row `+0x0d` | `+0x00` | 1 | `0x508750` copies it; the plugin compares it with the row (the difficulty check) |
| group count | `+0x10` | 1 | `0x509c24`..`0x509c43` (`0x509940`) |
| group weight sum | `+0x11` | 1 | same; sum of the byte weights |
| group count 2 | `+0x12` | 1 | same |
| groups | `+0x14 + 0x34*i` | `0x34` each | word monstats row at +0, byte weight (monstats `+0x64`) at +2, byte at +3 (member count by `0x4195e0`, inferred); 13 groups end exactly at `+0x2b8` |
| `MonDen` | `+0x2b8` | 4 | write `0x508921` |
| `MonUMin` | `+0x2bc` | 1 | write `0x50892d`, read `0x419d47` |
| `MonUMax` | `+0x2bd` | 1 | write `0x508939`, read `0x419d3f` |

Readers: `0x419e80` picks the monster kind from `+0x10`, `+0x11` and `+0x14`; `0x419d20` reads `+0x2bc` / `+0x2bd`; `0x50acf0` reads `+0x2b8`. In Normal difficulty only, `0x419e80` also reads the row's umon list (`+0x3d`); maps open in Hell only, so that read is not reached.

### What is written

| Map column | Where | Value |
|---|---|---|
| `Level` | row, six `MonLvl*` fields | the level, 1 to 255 |
| `Density` | record `+0x2b8` | the record's value plus `Density` percent, rounded half up |
| `Uniques` | record `+0x2bc`, `+0x2bd` | the record's value plus `Uniques` percent, each at most 255; no averaging |
| `Monsters` | record `+0x10`, `+0x11`, `+0x12` and the first groups | counts = the number of ids, per entry the monstats row, its weight (monstats `+0x64`, from the SDK view) and 0 members; the sum is stored as a byte |

`Champions` has no field: the champion roll is fixed in `0x419d20` (`0x419e12`..`0x419e2c`), so it is read and checked but not applied. This replaces the earlier sentence that one pair served all special packs and was averaged; `Uniques` now scales the pair alone. Written at map open, on the game thread, just before `OpenRedPortal`.

### Guards (runtime, per part, fail open)

- Slot guard: nothing is written when the slot level is 137 or less, or is not the checked slot of the map's own tileset (Maze) (`SlotAllowed`). Local, independent of `ValidateSlots`.
- Row: readable, `Id` equals the slot level; `Level` off when a `MonLvl*` field is outside 0..255.
- Record: pointer from `game + 0x1d0`, the level below the levels count (64-bit at `tables + 0x1468`, the bound `0x508750` loops to; the array holds levels 1 up to count - 1) and `array[level]` readable (`Readable`) for `0x2c0` bytes; `record+0` equals row `+0x0d` for the game's difficulty (`game + 0x104`); `Monsters` off when the group counts are above 13 or inconsistent, `Density` off when `MonDen` is outside 0..100000, `Uniques` off when `MonUMin` / `MonUMax` is above 100.
- Monsters list: refused with a log line (the slot keeps its own list) when it has more than 10 entries, a weight outside 0..255, weights that are all 0 (the game's weighted pick would find none) or a sum above 255 (the record keeps the sum in a byte, `0x509940` adds the weights into `record + 0x11`). A monster whose monstats row has bit 0 clear at `+0x3c` is left out, as `0x509940` does (`test` against the byte at `0x1d996d0`, which is `1`, at `0x509940` line `(byte)DAT_141d996d0 & *(byte *)(row + 0x3c)`, the same test as `0x493280`); when none is kept the list is refused. Static only: the name of monstats `+0x3c` bit 0 is not known.
- A part whose check fails is switched off with a log line; the others still apply. A write that fails stops the open and the undo restores what was written.

### Restore

The row is restored on `GameLeft` and on unload, and on a failed open (`Undo`), writing a field back only while it still holds the plugin's value. The record is undone on a failed open but only forgotten (ledger cleared, no write) on `GameLeft` and unload: the array belongs to the game that is being destroyed or is gone, so a write there is not safe. A new game builds fresh records.

### Risks, and what needs the game

- Needs game: that `Density`, `Uniques` and `Monsters` written to the record change the spawns (static reading says so, nothing was observed), and that `Level` shows in monster levels.
- Inference: entry `+3` is a member count (`0x4195e0`) and 0 is a safe value; the plugin writes 0.
- Normal difficulty also reads the row's umon list; not reached, as maps open in Hell only (#159).
- The level is bounded by the levels count read at `tables + 0x1468` through the data tables base; when that cannot be read the record parts are off.
- One record per slot: a second map through the same slot in one game overwrites the first map's values for rooms that populate later.
- Client side: a TCP/IP client writes none of this; the host spawns the monsters.

### Game check (listed for later)

A map with `Level` 60 and `Monsters` `quillrat1,zombie1`; `cartographer open`; the log shows the row and record writes; read a monster's level and kinds. Then `Density` 100 and `Uniques` 100 on a slot with a nonzero base and count packs. Leave the game and check the log reports the row put back.

## 16. Outdoor S2 spike results (issue #89)

The Snow (Barricade) tileset pieces, sockets and plan grammar (issue #147, static evidence only) are in [cartographer-snow.md](cartographer-snow.md).

Spike: `plugins/cartographer-outdoor-spike/` on `spike/cartographer-outdoor-s2` (never merged). Build 93847, slot 5 test save, level 138 (Act 5, DrlgType 3, LevelType 21), layout `*E,EWS,SW/-,*N,NS/-,-,*N` (3 x 3 blocks of 32 x 32 tiles: an L plus a branch; `*` is a Pygmy clearing; level rect `6000,3000 96x96`). Status per item: **live** = seen in a running game, **static** = from code or a log of the build, not walked.

| Item | Result |
|---|---|
| Layout stamps and the level builds | **Live.** 6 pieces stamped, 96 rooms for 6 blocks (16 rooms of 8 x 8 per block); `0x3ee780` made no rooms for the 3 void blocks. |
| Void edge (`0x3ede00`, grid `+0x58` = `0x100`, no room) | **Live, acceptable.** A `cheat goto` onto a void block does not place the player there: the game falls back to the entry (tiles 19,18). A click-walk from the entry toward the void block below it stopped at tile y 24 inside the entry clearing (the block begins at y 32). The screenshots show dark rock, not a hole. Fill or a border piece is **not needed for play**; the automap edge was not checked. |
| Edges walk as predicted | **Live for 4 of 5.** Walked: (0,0) to (1,0) (tile x 19 to 34); (1,0) to (1,1) (tile y 17 to 43); (1,0) to (2,0) (x 59 to 68, past 64); (2,0) to (2,1) (y 17 to 42, x 78). **Static only: (2,1) to (2,2):** `cheat goto` reaches the block centre (80,80) with the player on open ground, but the click-walk from (2,1) stopped at (78,42), so no crossing was walked. The client loads collision only a few rooms around the player (a whole-level dump shows 95 to 100 % blocked), so a walker works from the position, not from a whole-map dump. |
| Portal lands on the entry cell | **Live.** `OpenRedPortal(game, player, key, 138, 1)` returned 1 and the portal opened in town; entering it logged `ARRIVAL ... subtile (30098,15082) tile (6019,3016) level-relative (19,16) = cell (0,0): IN THE ENTRY CELL`. The finder (`0x3da650`) took the room the spike marked with `0x30000` (tile 16,16 of the entry cell). Not tested without the mark (`arrival = "other"`). |
| Boss cell is a clearing | **Live.** The `*N` cell (1,1) shows the open Pygmy clearing with monsters in it (a Demon Rascal is named on screen); the player stood in it. The monster and object rows were those cloned from Act 5 row 111, so the layout alone does not decide what fills a clearing. |

### Findings

- `Vis0` to `Vis7` = 0 is recorded in section 8a (slot row requirements); the crash with `-1` was reproduced here.
- The portal builds the level on the server inside the `OpenRedPortal` call; the client builds it again on arrival. The stamper and the void marking therefore run twice per entry, once per side, and both runs must give the same grid.
- `OpenRedPortal`'s last argument is `1`, as the maps plugin passes it.
- `core-maps.dll` was out of the test slot's plugins, so the spike's row 138 and hooks were the only users of the level build.
- D2RCore needs `data\global\excel\d2rloader\core-ui\automap.txt` in the mod folder; without it the game asserts at start and faults.

### What the outdoor builder still needs before players get outdoor maps

1. A position-driven walk test over the whole layout (this spike crossed four of five edges by guided pathing), and a check that every `*` clearing joins its path piece in every orientation the generator can produce.
2. The slot row as engine contract: `Vis` `0`, one row per slot with the offset and size the layout needs (section 8 Q4), and one shared hook on `0x3271c0` (section 3).
3. A server-to-client consistency rule: both sides stamp from a seed or layout sent with the portal, never from independent random picks (section 12).
4. Monster and object population per clearing, boss placement in the clearing, and an exit or waypoint to leave the map (the spike had none; the way out was the town portal).
5. Automap and minimap edge behaviour, a border piece or fill if the automap shows the void, and the arrival choice made by the layout (the entry cell) instead of a spike-side room flag.
6. The real player flow: the map item and portal tooltip path (sections 3 and 10) driving this slot, multiplayer sync (section 12) and a sweep over layouts and LevelTypes; none of that was run in this spike.

Non-rectangular outlines for the open-field families (issue #148, static only): [cartographer-outline.md](cartographer-outline.md).

## 17. AutoCheck: a game check the plugin runs by itself (issue #173)

`AutoCheck=1` and `AutoCheckMap=<map Name>` in `cartographer.txt` (test slots only). In a Hell game the plugin opens the map through the console path's own gates and `OpenMapPortal` (no item), waits for the player to be in the portal level, logs the arrival, moves the player into the plan's boss room (the `goto` route), and writes `AUTOCHECK PASS|FAIL <item>` for `open`, `arrival`, `boss`, `monster-settings` and `stat-changes`, then `AUTOCHECK DONE pass=N fail=M` (an item that does not apply is written `AUTOCHECK N/A <item>` and counted as `n/a=K`, not in `pass`). Code: `src/engine/auto_check.cpp`; pure parts (checklist, lines, setting, stand point): `src/autocheck`. `AutoCheck=1` also turns on the arrival probe (the `[maze probe] arrival` line), nothing else of `MazeProbe`.

### New part

| Part | Role | RVA | Signature | Max patch / hook | Guard |
|---|---|---|---|---|---|
| UnitMove | Called. `(game, unit, room, x, y, arg6, arg7)` puts a unit at a subtile position in `room`; returns 0 when it could not. With a null room it only tries the unit's own room and its neighbours (`0x38dfb0`, `0x2efde0`), so a far cell fails: the check passes the room it found. arg6 (`[rbp+0x77]`): nonzero skips the free-spot search `0x362f90` (mask `0x1c09`). arg7 (`[rbp+0x7f]`): nonzero resets the path (`0x342210`) and sets unit flag `0x40000000` (`0x34e140`). Both are 0 here | `0x491ca0` | `40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F9 48 81 EC 88 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 EF 44 8B 6D 77 41 8B F1 49 8B D8 48 8B FA 4C 8B F1 48 85 D2 75 11` | 25 (security cookie load) / 0, only called | 60-byte signature unique across the executable sections |
| RoomAtTile | Called. `(dataFlag, act, tileX, tileY)` finds the room holding that tile of the act's levels. It is not a pure lookup: with no room and no level holding the tile it goes on through the find-or-create level function `0x3267c0` (id 0 when no level of the act holds the tile), which can allocate and link a new level into that act and build its rooms, so it must only be called on the act the tile belongs to. A 9-byte thunk (`mov rdx,[rdx+0x70]; jmp 0x328900`); callers: `goto` (`0x41031f`) and `0x49015c`. `act` is an element of the game's act list (`game+0x190`: pointer to the acts, count at +8; `0x4900c0..0x490110` index it with the byte `0x3e2b5c8` returns). The check does not call the loader import: it takes the act from the slot's levels.txt Act (`Slot::act`, the value `0x3e2b5c8` returns for the level), checks it against the count and that `acts[act]` and `acts[act]+0x70` are readable, calls the lookup once, and keeps the room only when its level (`RoomLevelId`) is the slot level; any failed step logs the reason and the player walks | `0x2f0810` | `48 8B 52 70 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC 48 8B 49 18 E9` (the padding and the next thunk make it unique; the 9-byte form matches 3 places) | 4 (the `jmp`) / 0, only called | 21-byte signature unique |
| RoomBringUp | Called. `(game, room)`: builds and populates the room if it is not up (`0x48f190`, `0x503790`, `0x510f00`, populate `0x50acf0`, then `0x2f04e0`). The populate reaches the plugin's hook, so the check calls it with its own lock released | `0x4042a0` | `48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 85 C0 75 ?? 48 8B D3 48 8B CF E8` | 16 (first `call`) / 0, only called | 40-byte signature unique |

The route is the one the console command `cheat goto` uses (handler `0x4100e0`, table record at `0x238bba0`; calls at `0x41031f`, `0x410332`, `0x410358`): find the room, bring it up, `UnitMove` into it. x and y are subtiles (the handler divides by 5 for the lookup). `goto` also calls `0x488180(game, player, levelId, 0)` to enter a level; the check does not use it: the portal is entered by hand so the arrival check stays real. Each part is resolved on its own; if any is missing, or the room is not found, or `UnitMove` returns 0, the boss step logs why and waits for the player to walk (120 s). Before the move, and while waiting for the Boss, the check re-reads that the player is in the slot level of a Hell game; if not, the Boss is a FAIL (`the player left the map`).

Evidence: disassembly over the dump (`dump_dis.py`, stamp `0x6ab3782c`), signature uniqueness by `check_portal_signatures.py`. The act-list layout and the argument meanings are read from code, not run.

### Searched, not found (static)

- **Entering a portal (moving the player to another level).** `cheat goto` does it through `0x488180(game, player, levelId, 0)` (handler `0x4100e0`, found through the console cheat table at `0x238b120`), but it does not walk through the portal, so using it would skip the arrival check. The check therefore waits (180 s) for the player to walk into the red portal and logs `AUTOCHECK waiting`.
- **Leaving the game.** The dump has the strings `ExitGame`, `QuitGameConfirmation` and `Leave Game`, but no handler was followed to a callable function, and the SDK has no leave call. After `AUTOCHECK DONE` the plugin logs `AUTOCHECK LEAVE not done` and the checker uses Save and Exit.

Both are desk results (static only); finding them needs a debugger session in a test slot.

### Needs a game

Everything above. The unit mover's meaning of `x`/`y` (subtiles, as the server room rect is) and that standing in the boss room wakes its population are read from code and from the earlier `cheat goto 138,25020,10130` evidence, not run. A passing run needs: Hell game, a character in Harrogath, the map's slot installed, then one click on the portal.
