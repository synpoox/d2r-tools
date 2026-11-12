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
| `0x3e96d0` | Maze random walk `(level)`: adds rooms until the room count is reached. Replaced for the map level. Signature recorded in section 1a; hooked by Cartographer (not run in game) | drlg-steering.md; steer-proto |
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

Hook cuts (whole instructions, all exact bytes): `0x3271c0` 14, `0x3e96d0` 18, `0x3eb850` 17, `0x3e9820` 14. Cartographer hooks these four (written, not run in game).

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
| Validation | Static only. No game. The hooks built on it are written but not run in a game. Not checked on Steam build 94995 |
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

The maze steering hooks now depend on these answers; none is game-validated yet (the code logs the reason and falls back to
the game's own maze when a step fails): the size written at `+0x2c/+0x30` in the `0x3271c0` hook before the original (Q4), the
slot row's declared rect as the lookup rect (Q2), the suppressed second `0x3eb850` call and the rotation counter kept in 0..3
(Q3), a private LvlMaze row (never freed) behind `+0x18`, and 360 x 360 accepted (Q1). The declared size is recorded once per game, per level id, before the first write; every plan is limited to that record (the hook overwrites the level's `+0x2c/+0x30` with the plan's size, which survives a teardown). On a fallback, and on any later unsteered build, the declared size and the level's own row pointer are put back first, so the game's walk runs against the level it was written for (after a fallback mid-walk it finishes from the plan's rooms already built). Also unverified: whether `0x3271c0` runs
before the plan exists, whether the add-room checks (`0x360c80`) reject adjacent but unlinked plan cells, whether the piece
struct is per room (fixed-flag writes), and `clearFixed=1` in `0x3eb960` for every room. `MazeProbe=1` logs the data to check.

### Left for the game

See also section 8 (Q6 and Q10) for the portal arrival and the outdoor size.

1. The whole game accepting a 360 x 360 maze level (Q1, size and lopsided checks).
2. The origin being clear of every other level's rect, including outdoor levels (Q2 log).
3. A slot id built with its second `0x3eb850` call suppressed: clean build, arrival and walk (Q3).
4. Optional: sizing the level to the layout span instead of the centred capacity (Q1 lopsided check).

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
| `0x2f5b80` | Find a list `(unit, state, flags)` on the owner's chain (`+0x90`, or `+0x98` with flags bit 13; next `+0x68`; match `(list+0x1c & flags & 0xffffdfff) != 0`) | `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B F8 8B F2 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 8B CB E8 ?? ?? ?? ??` | call |
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

**Status:** written and unit tested; player half off by default (`PlayerStats`); signatures checked statically on the build 93847 dump (`check_portal_signatures.py`); **not run in a game**. Applies the section 6 recipe: monster stats from a hook on the monster setup `0x494a60`, player stats from the existing PlayerTick hook (section 3). Code: `plugins/cartographer/src/engine/stat_apply.cpp`; pure plan and tracking logic in `src/statplan/`.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer map stats (`src/engine/stat_apply.cpp`) |
| Module | `D2R.exe` (SetListStat and AttachStatList are D2RLoader thunks into `D2RCore.dll`: call, never hook) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | `NewStatList` `0x2f7300`, `SetListStat` `0x2f7c00` (thunk), `AttachStatList` `0x2f2cb0` (thunk), `DetachStatList` `0x2f7920`, `FreeStatList` `0x2f4180`, `MonsterSetup` `0x494a60` (hooked). `UnitRoom` `0x34b440`, `RoomLevelId` `0x2efc10`, `UnitStat` `0x2f5020` are the portal's parts, found again under the names `StatUnitRoom`, `StatRoomLevelId`, `StatUnitStat`. Unit fields read directly: type `+0x00` (0 player, 1 monster), id `+0x08`, data flag `+0x1bd` (a byte: the three short helpers `0x34b9d0`, `0x34a330`, `0x34a0e0` are exactly these loads behind a null check, read in this change), stat list chain head `+0x90` (second head `+0x98`), next `+0x68` (section 6). The hook's first argument is the spawn block: `+0` game, `+8` room, `+0x18` monstats id (read at `0x494a72` to `0x494a84` and the caller `0x4943ec`..`0x4943f2`) |
| Guard | Signatures in `src/core/part_signatures.h` (`NewStatList`, `SetListStat`, `AttachStatList`, `DetachStatList`, `FreeStatList`, `MonsterSetup`). Each matched exactly once. `MonsterSetup` is 21 bytes with no `??`; the hook cuts the first 14 (whole instructions: `mov [rsp+18],rbx; mov [rsp+20],rbp; push rsi; push rdi; push r15`), which D2RLoader re-checks at the entry. The first relative operand is the `call 0x976e0` at `+0x2b`. Where a part does not match once, the part is logged and left off |
| Contract | Monster: after the game's own setup returns, if a plan is active and the room's level is the slot level, `NewStatList(0, 0, 1, unitId)`, `SetListStat(dataFlag, list, stat, value, 0)` for each monster entry, `AttachStatList(monster, list, 1)`. The list is never tracked or freed (it goes with the unit; inference). Player: from the tick, when the player's room level is the slot level, the same with type 0, and the list address is kept per unit id; on leaving the level (or the map closing) it is removed only if it is still on the unit's chain: `DetachStatList(unit, list)`, `FreeStatList(dataFlag, list)`. No state is set and no state bit is changed (the engine's own lists use a state, but a state id that does not exist in the game's states table would write outside the bit set; the plugin keeps the list address instead). Game thread only; the tables are read under a lock. On `GameLeft` the plugin forgets its state and touches no game memory |
| Evidence | `dump_dis.py ... dis 0x494a60 16` (prologue, args), `dis 0x4943d0 14` (the call and that its result is unused), `dis 0x34a0e0 / 0x34a330 / 0x34b9d0` (field loads), `dis 0x2f7920`, `dis 0x2f4180` (detach computes its own data flag; free does nothing for a list with a negative flags word), `pat "<signature>"` for uniqueness; `check_portal_signatures.py` for all of them |
| Validation | Unit tests (`statplan_tests`: the draw to plan, stat id lookup, player apply and remove decisions, the tracking table), signature check on the dump. **Not run in a game** |
| Failure mode | Each half is off when any part it needs is missing or its guard fails; the other half still works. Nothing is replaced by a guess. A map opens either way |
| Coexistence | The monster hook is a new hook on `0x494a60`, which nothing else here hooks; it calls the original first. The player half rides the portal's PlayerTick hook (also Better Auras' neighbour `0x42e600`). The D2RCore spawn hook (flag `0x40`) runs after `0x494a60` and is untouched |

### Needs game (the section 6 stats checks, settled by the `statprobe:` log lines)

1. **Percent stat on a monster, and the order against `0x49dd70`.** For the first six monsters of a map the log has `statprobe: monster #n ... stat 12 (level) was L at the hook, maxhp A -> B, totals [..] -> [..]`. A level of 0 at the hook means `0x49dd70` runs later; a nonzero level means earlier. Totals that change by the listed value show the list counts; maxhp A -> B shows whether `item_maxhp_percent` reaches a monster's life (if not, pd2's direct `SetUnitStat` on maxhp is the fallback). Then check the monster's life in play, and whether a later level write resets it.
2. **List on the chain.** The same lines end `list on chain: yes/NO (n lists)`. `NO` means the chain offsets (`+0x90`, `+0x98`, `+0x68`) are wrong for an attached list; removal then never runs (it only frees a list it finds), and this is the one to fix first.
3. **Client replication.** The probe is server side. In play: the monster's life bar and damage as the client sees them, the player's resistances on the character sheet, and the game's own stat text (the client builder `0x2dc4b0`).
4. **Stats without a state.** If totals do not change at all, the list needs a state; add one (a state id read from `states.txt`) and re-test.
5. **Player list and saves.** `statprobe: player ... entered the map` and `left the map` show the stat totals and chain length before and after. Save and exit inside the map, reload, and read `statprobe: first player unit ... holds N stat lists` against a game that never opened a map; a higher count means a list survived the save.
6. **Removal and leaving.** Leave the map level (town portal): the log shows `left the map: list removed (chain n -> n-1)`, and the stats are gone from the character sheet. Leave the game: `left the game: N monster(s) and M player(s)`, and a new game shows no leftover stats.
7. **Every monster.** Count of `monster #n` against the monsters seen in the level; whether minions and bosses get the list (they all pass the same setup call).
8. **Monster list free or leak.** Monster lists are never tracked or freed; that the game frees a unit's lists with the unit is inference (section 6). Heap count (or process private bytes) before and after several map runs with many monsters; steady growth per monster means the lists leak and each needs a detach and free on the monster's death or removal.
9. **Level stat on monsters.** `level` is refused on `Monsters` rows (the game's own level write at `0x49dd70` runs in or after the same setup and would interact with it); nothing here tests that write, so a `level` row is not offered until it has been checked.

### Player stats are off by default (review of issue #80, fix cycle 1)

- **`PlayerStats` in `cartographer.txt` is 0 by default.** While it is 0 nothing is attached to the player: the plan carries no player entries and the tick does not apply. Reason: the list is on the unit's chain until the next tick outside the map level, so a Save and Exit inside the map, or an autosave there, may write the character while it is attached. Whether the save writer reads base stats only is inference (equipped items' lists sit on the same chain and are not saved) and needs check 5 above. Use `1` on a test slot only until check 5 has passed. Removal when the map closes or the player leaves the level is unchanged.
- **Refused stats.** `buildPlan` refuses these on `Players` rows, because the game reads them as a total (`UnitStat`: base plus lists) and writes the result back as the base value: `statpts` (4), `newskills` (5), `hitpoints` (6), `mana` (8), `stamina` (10), `level` (12), `experience` (13), `gold` (14), `goldbank` (15). Matched by name or id. The other stats of the vanilla itemstatcost with no item bit (`lastexp` 29 and `nextexp` 30) are level-table look-ups and are not written back; `maxhp`, `maxmana`, `maxstamina` and the attributes are item stats and allowed. No further stat with this behaviour was found by reading vanilla `itemstatcost.txt`; this is a static reading of the table's save columns, not of the game's writers. `level` is also refused on `Monsters`, and so are `hitpoints`, `mana` and `stamina` (6, 8, 10): the mechanism above also runs for a monster on every hit and on regeneration, so a `+X` list would be given back each time base is rewritten (inferred; the damage writer was not traced in the dump). Param stats are refused on both targets: the 17 rows of the vanilla 3.3.0 `itemstatcost.txt` with a Save Param Bits value (static reading of that column; ids 83 `item_addclassskills`, 97 `item_nonclassskill`, 98 `state`, 107 `item_singleskill`, 126 `item_elemskill`, 151 `item_aura`, 155 `item_reanimate`, 179 `attack_vs_montype`, 180 `damage_vs_montype`, 188 `item_addskill_tab`, 195-199 and 201 `item_skillonattack`/`onkill`/`ondeath`/`onhit`/`onlevelup`/`ongethit`, 204 `item_charged_skill`), matched by name or id, because the list entry is set with layer 0, which would name skill or state 0. Stat ids are the itemstatcost row numbers, not the `*ID` column (vanilla row 212 says 213). `check` and the report name each refused row.
- **Generation counter.** A counter bumped under the lock on `GameLeft` is read with the plan on each tick and checked under the lock before a made list is recorded; if the game was left meanwhile the list is not recorded or touched (it belongs to the old game's unit). So a list pointer from an earlier game is never used, whichever thread delivers `GameLeft`.

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
| `0x3ede00` | Void cell setter `(level, x, y)`: SETs piece grid `+0x08` to 0 and flag grid `+0x58` to `0x100` | Q3 | hookable | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 41 B9 00 01 00 00` |
| `0x3edda0` | Clear cell `(level, x, y)`: SETs both grids to 0 (same shape as `0x3ede00`; only the last constant differs) | Q3 | hookable | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 45 33 C9` |
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
| Contract | At map open the event is decided with `events::decide` (the map's `EventChance` or the global one, the map seed of issue #92, which is per opening: `mixSeed(tick, player id, opening count)`, salted for the event stream). It is kept only if it can be spawned (Boss or Horde, `Spawn` found in `monstats.txt` `Id` or `superuniques.txt` `Superunique`, a Class that is in monstats, the spawn parts on). Each populated room of the slot level goes to `EventRoomTracker`: a maze map's event room is the plan's event cell (the DRLG rect recorded after the slot build by `maze_hooks.cpp`, `MazeEventRoom()`); a preset map (or a maze map whose event room is not known) takes the first populated room as the entry room and the first later room whose centre is at least `kMinEventDistance` (24 tiles, Chebyshev) from it. At that room, after the original returned, a Boss spawns its monster (a superunique through `0x499ba0`, unless its once-per-game bit is already set, then its Class as a plain monster) at the room centre and `Count` copies of its Class around it; a Horde spawns `Count` copies (a superunique as its Class). Spots come from the **server room rect** at `+0x80` (the rect the spawn search and the point-in-room test `0x2f0680` use, so its units are the spawn code's): the centre first, then a grid two tiles apart and two tiles in from the edge, where a tile is `server.w / drlg.w` server units (5 when it is subtiles, as `spotScale` computes; 1 if the rects are equal). No tile-to-subtile conversion is assumed. Every monster is `SpawnMonster(game, room, x, y, monstatsRow, 1, 0xa, 0)`, the arguments the superunique code itself uses, flags 0 so the game's normal path (and with it the map stats hook, section 7) runs; a superunique is `SpawnSuperUnique(game, room, x, y, row, 0)`. Once per map open; state is cleared on `GameLeft` with a generation counter. The `Drop` column is not applied in 6a. **Spawn names:** `Spawn` is resolved against the active mod's `monstats.txt` (`Id`) and `superuniques.txt` (`Superunique`, `Class`) by row; the row index is the parsed row count, which is what the engine is asked for. The game's compiled monstats and superuniques tables are reachable through the SDK `DataTableService` (`TableId::MonStats`, `SuperUniques`), but no row layout with a name field is recorded, so they are not used for names yet. A mod that does not ship these two files gets no Boss or Horde events: `cartographer check` and the report say so, and nothing is spawned or announced (follow-up issue linked under #19). **Threads:** the populator hook (spawn), the map-open decision (called from the portal's map open) and the player tick (entry message arming) run on the game thread; the banner draws on the overlay thread and reads a copy of the lines under `g_bannerLock`. **Superunique edge:** the game skips its once-per-game bitmap when the row's `+0x29` is set or the global `DAT_142aa6a74` is non-zero (`0x499c5e`); the plugin's own `superSeen` pre-check does not, so such a repeatable superunique spawns as its Class after the first time, though the game would have allowed it. Harmless, and not fixed |
| Entry message | The SDK route: the Overlay service (`registerFrameCallback`, phase `AfterGameUi`, `measureText` and `drawText`), fading in, holding and fading out over about six seconds; the lines are the map's `Name`, then the event's `Name` and `Message` when an event fired and is applied. The overlay draws on the local client only. Without the service (or if drawing fails) the lines go to the console (`WriteConsoleMessage`). It is shown from the player tick, once each time the local player's room level becomes the slot level. The chat packet fallback `0x477a30` is not used: the local routes need no network, and the packet route would only matter for multiplayer sync, which is out of scope |
| Evidence | `dump_dis.py ... dis 0x50acf0 / 0x492cf0 / 0x499ba0` (prologues), `pat` for the uniqueness of each signature, `check_portal_signatures.py` (all three rows ok on the dump); the callers of `0x50acf0` and the argument registers from section 6 |
| Validation | Unit tests (`eventplan_tests`: name resolution, placement, event room tracker, Boss and Horde spawn plans, the superunique bitmap bit, entry lines, entry tracker, banner alpha), signature check on the dump, a clean Release build. **Not run in a game** |
| Failure mode | The populator hook goes in at load only when `PopulateRoom`, `SpawnMonster` and `EventRoomLevelId` were all found; otherwise no event is spawned or announced, and the map still opens with its stats and its entry message. A missing `SpawnSuperUnique` spawns a boss as its Class. A map whose Spawn cannot be resolved logs why and has no event. A hook that D2RLoader installs without an original is logged as an error and leaves room population skipped until the game is restarted (the same hazard as the other hooks here) |
| Coexistence | A new hook on `0x50acf0`, which nothing else here hooks; the original always runs first. Spawned monsters pass `0x494a60`, so the map's monster stats reach them |

### Needs game (settled by the `eventprobe:` log lines)

1. **Units of the server room rect.** Each populated room of the map level logs `eventprobe: room #n of level L populated: DRLG (x,y) w x h tiles, server (..) ..., k server unit(s) per DRLG tile`. Spawn spots are placed directly in the server rect (by static reading the spawn code's own units, from `0x2efb40`), scaled by that k; check that the spawned monsters stand in the event room, and that k is 5 (subtiles) or 1.
2. **Event room.** Maze: `eventprobe: maze event room (plan room n) is DRLG room ..., rect (..)` after the build, then a populated room with the same rect logs `event room from the plan` and `THIS IS THE EVENT ROOM`. Preset: the room chosen by distance, and whether 24 tiles suits the shipped preset layouts (it may fire in a room the player passes at once, or never in a small map). Whether a rebuilt level (leaving and re-entering) populates the same rooms, and that the event does not spawn twice.
3. **Spawns.** `event '..' (Boss): spawned N of M ... (k as a superunique, f refused)`. Every monster shown in play with the right class, level and the map's stats; the boss at the room centre with its minions around it. `superunique row R at (x,y): returned P, once-per-game bit now set` settles the return value and the bitmap mask (inferred as `1 << (row & 7)`). A second map with the same superunique in one game should log `already spawned this game` and spawn its class. `SpawnMonster`'s seventh argument is a free-spot search radius, not a mode (item 4), so `0xa` stays; check that ordinary monsters spawned with it look right.
4. **Position validity.** `SpawnMonster`'s p7 (`0xa`) is the engine's own free-spot search radius, not a mode (static: p7 goes to request `+0x2c`; `0x493cc0` at `0x493f13..0x493f34` searches only the exact point when p7 is negative, and with p7 >= 0 rings out from 3 to `p7*3` subtiles for a free position inside the room rect from `0x2efb40(room)`, server room `+0x80`). So `0xa` turns the search on (about 30 subtiles) and `-1` would turn it off and make walls and stacking worse; `FindFreeSpot` is not used. Check that monsters do not stack on walls or each other. The ring search can put a unit in a neighbouring room that is not yet active; the game's own packs share that risk, and the check is that nothing crashes or sits unreachable.
5. **Entry message.** The overlay banner appears on arrival through the portal (map name, then the event name and message), fades out, and appears again on re-entering the level; the console fallback when the overlay service is absent. It must not show in other levels.
6. **Rate.** Over many map opens the share with an event is near `EventChance`, and the row drawn follows `Weight`.
7. **Leaving the game.** After `GameLeft` and a new game there is no banner and no event from the old map, and no spawn in a level that was not opened.

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

## 11. Event Spawn names from the game's compiled tables (issue #95)

**Status:** static research only, from the build 93847 dump; **nothing was run in a game and no code was changed**. Context: section 9 resolves an event's `Spawn` against the mod's `monstats.txt` `Id` and `superuniques.txt` `Superunique` columns (`ReadSpawnNames`, `plugins/cartographer/src/plugin.cpp`); issue #95 asks whether the game's own loaded tables can do it, so a mod that ships those tables in another form (or none) still resolves. Labels: **static** is read from the dump; **inference** is reasoned from it; **needs game** is a check to run.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer event `Spawn` resolution (a future change to `ReadSpawnNames`) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` |
| Location | Data tables base `0x300a90(dataFlag)` (cl, below 4). MonStats row getter `0x976e0 (dataFlag, index)`, SuperUniques row getter `0x397220 (dataFlag, index)`. Name to row lookup `0xa121f0 (map, name, logOnMiss) -> row, negative on a miss`. Engine resolvers `0x3a9050 (dataFlag, name, out)` and `0x3a9150`. Table fields (with `dt` the base): `dt+0xf58` monstats row vector (count at `+0xf60`, row size `0x1fc`), `dt+0xf70` monstats name map pointer, `dt+0x1078` superuniques row vector (count at `+0x1080`, row size `0x3c`), `dt+0x1090` superuniques name map pointer; also `dt+0xf90` monsounds and `dt+0xfd0` monplace maps |
| Which hook | None. The route is reading rows (SDK) and, for superuniques, one call-only lookup |
| Guard | Masked prefixes below, each unique by `pat`. `0x976e0` is hookable (15-byte cut, no relative operand) and `0x300a90` hookable (16 bytes); `0x397220` (relative `call` inside the first 14 bytes), `0xa121f0` (`mov rax,[rip+..]` at +13) and `0x3a9050` / `0x3a9150` (`je rel32` at +3) are call-only |
| Contract | See Q1 to Q4. In short: re-read `*(dt+0x1090)` on every call, never cache it, skip the call when it is null, reject an empty name, treat exactly -1 as a miss, game thread only |
| Evidence | `dump_dis.py ... dis` of the getters, of the monstats and superuniques table loaders (found by the unique store of the monstats row size, `0x1fc`, to `[rsp+0x28]` at `0x3a0af9`; the superuniques store of `0x3c` at `0x3a2173` is **not** unique, the same `C7 44 24 28 3C 00 00 00` bytes match 5 sites: `0x3a2173`, `0x5bec88`, `0x5befb1`, `0x5cfa94`, `0x1b6ffd3`; that loader was found through its descriptor strings instead) and of the resolvers; `pat` for every other prefix |
| Validation | Static only. **Not run in a game** |
| Failure mode | If a guard does not match, or the self-check of Q4 fails, keep the `.txt` route of section 9 as it is today |
| Where it runs | Not in `LoadAll()` (`plugin.cpp` `ReadSpawnNames`, which runs at load and `Reload()`, possibly off the game thread and before the tables exist). The lookup belongs at map open, in or just before `resolveSpawn` (`event_apply.cpp`, reached from the `EventsMapOpened` path), or at the first map open after each `DataTablesLoaded` revision. See Q4 |
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

- **Row and getter (static).** `0x976e0` calls `0x300a90`, compares the index with the count at `dt+0xf60`, and returns `[dt+0xf58] + index * 0x1fc`.
- **The `Id` text is stored inline in the row (static plus inference).** The loader (chunk `0x39db7f`) describes its columns in 0x20-byte entries `{name, type, size, row offset, extra}`. The `Id` entry has type `0x13` and size `0x2f`, row offset 0, and `extra` is the address of the map slot `dt+0xf70` (not the map itself). Reading: a 47-character key plus the terminator, in a 48-byte field at row `+0`. The type, the size and the offset are static; that type `0x13` means "inline key text" is **inference** (it fits the toolkit's `knowledge/tables.md` note of a 47-character `Id` key, and the flag bytes at `+0x3c/+0x3e/+0x3f` in `monster-count-addresses.md`, which sit just after it). No hash or string-table index of the `Id` is stored in the row.
- **Who fills the map (inference, not in the dump).** Both table loaders (monstats and superuniques) hand their descriptor arrays to `0x3e2b544`, a D2RLoader `FF 25` thunk (`FF 25 4E F6 FF FF`, in `.maho`; the same kind as `0x3e2b5c8` in section 1) into `D2RCore.dll`. The rows, and the inserts into the maps behind `*(dt+0xf70)` and `*(dt+0x1090)`, happen in D2RCore, so the fill depends on D2RLoader's D2RCore, not on `D2R.exe`. That the map holds `Id` to row index, and that the stored keys are lower-cased, are therefore **inference** from the descriptors and from how the lookup is used. **Static in `D2R.exe`:** map creation (`0xa11fa0`, stored at `0x3a0a83` for monstats and `0x3a2103` for superuniques; it initialises the default `[map+0x3c]` to -1), the registration (the descriptor `extra` is the slot address) and the lookup.
- **Lookup by name (static).** `0xa121f0(rcx = map, rdx = name, r8d = logOnMiss)` lower-cases only the query (`0xa25650`), hashes it, compares exactly (`0x12d3ee0`) and returns `[node+0x30]` in `eax`. So case-insensitivity holds only if D2RCore inserted lower-cased keys (inference above). A miss returns exactly -1 (`0xa12385`; every caller does `test eax,eax; js`); `logOnMiss = 0`, or a name starting with `*`, skips the log. A null name **or an empty string** returns the map's default `[map+0x3c]` (-1 as initialised by `0xa11fa0`; whether D2RCore changes it is unknown), so the plugin must reject an empty `Spawn` before calling. A name longer than 15 characters makes the lookup allocate a temporary string from the game allocator (`0x76210`, freed through `0x12063c0` at `0xa123cf`): fine on the game thread, one more reason not to call it elsewhere. The same function serves monsounds (`dt+0xf90`), superuniques (`dt+0x1090`) and monplace (`dt+0xfd0`).
- **The engine's own by-name users (static).** `0x3a9050` (MonPreset `Place`) and `0x3a9150` (MonPet `monster`) call `0xa121f0` for superuniques, then monstats, then (`0x3a9050` only) monplace, and store a kind (2, 1, 0) and the row in a small out record. They are load-time callbacks and log on a miss, so they are not proposed. They store the kind differently: `0x3a9150` as a `u16` at `+0` with the row at `+2`, `0x3a9050` as a byte at `+1`. I did not find the cheat or console `mspawn` resolver in `D2R.exe` (the toolkit's `cheat-commands.md` says the same); it is probably in D2RCore (inference).
- **A runtime caller (inference on what it is).** `0x409bbc`, inside the chunk of `0x409ad0`, calls `0xa121f0(*(dt+0xf70), name, 0)` with `dt = 0x300a90(byte [r14+0x106])`, at runtime and not at load. Its AI-mode name table at `0x1d14e20` holds "regular", "idle", "wandering" and others, so it looks like a runtime spawn text parser; what it parses is inference. It is the evidence that the map outlives the load. The engine's other runtime caller guards the map first: `0x36005c` does `mov rcx,[rbx+0xf70]; test rcx,rcx; je` before the call.
- **Null map (static).** `0xa121f0` with a null map is a crash, not an assert: it calls `0xa0ffc0`, may `int3`, then falls through to `cmp [rsi+0x20]` on null, an access violation. `0x3a9050` does no null check either (it loads `[dt+0x1090]` and calls). Each table load stores a new map from `0xa11fa0`, so a cached pointer goes stale after a reload. **Contract:** re-read `*(dt+0xf70)` / `*(dt+0x1090)` on every call, never cache the pointer, and skip the call when it is null, as the game's own caller does at `0x36005c`.
- **Needs game:** that the map is still alive and filled after the load (the lookup is called outside the loader); `0x409bbc` above is the inference for "yes".

### Q2. The same for superuniques (`Name` key, `Class` field)

- **Row and getter (static).** `0x397220` returns `[dt+0x1078] + index * 0x3c`, with a null return for an index past the count at `+0x1080`.
- **The `Superunique` key is not in the row (static).** The loader marks the key column type `0x11` (size 0, lookup only) and registers it into the map at `*(dt+0x1090)`; the row has no key text. So the name cannot be found by reading rows: only through the map with `0xa121f0`, or from the txt file.
- **Fields in the row (static, from the loader and two consumers).** `Name` is a string-table index, a `u16` at `+2` (descriptor type `0x18`, through the string-table callback `0x308be0`). `Class` is at `+8`, resolved at load through the monstats map into a monstats row index (inference for the exact width; the consumers of `+8` treat it as a monstats row). The `u16` at `+0` is unproven (inference: the row's own id).
- **Lookup by name (static).** `0xa121f0(*(dt+0x1090), name, 0)`, as in Q1.

### Q3. Does the SDK `DataTableService` give enough, the row count plus a raw row pointer, to read those fields without engine calls?

- **MonStats: yes.** `getTable(ctx, bank, TableId::MonStats, &view)` (22; `bank` is the `Bank` the data flag maps to, see Q4) gives `{rows, rowCount, rowSize}`. Check `rowSize == 0x1fc` (the header says to check it before casting), then compare the 48-byte inline `Id` at row `+0` with the name, case-insensitively, over `rowCount` rows. No engine call. **Inference:** the SDK's `rows` is the same vector as `dt+0xf58`; the rowSize check is the guard. A linear scan once per map open is cheap.
- **SuperUniques: not for the name.** `getTable(ctx, bank, TableId::SuperUniques, &view)` (28) gives `0x3c`-byte rows, but the key is not in them (Q2). Once the row is known they give `Class` (`+8`) and `Name` (`+2`). `findRowById` supports neither table (`data_tables.h`).
- Rows are read-only, game-thread-only and expire on the next table load (the header's contract), which suits a once-per-map-open read. Off the captured game thread the SDK returns `Busy`.
- `Class` is at most 4 bytes: the next descriptor sits at `+0xc` (type 2). MonSound (type `0x15`, `+0x1c`, `dt+0xf90`) follows the same index-through-a-map pattern.

### Q4. The safest route, and its guard

- **Recommendation (inference).** Keep the section 9 `.txt` route as the fallback and put the game's tables first:
  1. **Monstats `Id`:** the SDK table (no engine call, no signature), guarded by `rowSize == 0x1fc` and non-null `rows` for a non-zero count.
  2. **Superuniques `Name`:** one call-only lookup, `0xa121f0(*(dt+0x1090), name, 0)` with `dt = 0x300a90(dataFlag)`, found by the unique prefixes above, skipped when `*(dt+0x1090)` is null and with an empty name rejected first. It needs struct offsets, so it is guarded by the self-check below. If the SDK later offers a name lookup for these tables, use that instead.
  3. **Class and display name:** after the row is known, read `Class` (`+8`) and `Name` (`+2`) from the SDK rows (`rowSize == 0x3c`).
- **Where it runs.** Not in `LoadAll()`: that runs at plugin load and `Reload()` (possibly from the console thread), before the tables exist, and `ReadSpawnNames` is only the txt route. The lookup runs at map open, in `resolveSpawn` (`event_apply.cpp`, reached from `EventsMapOpened`). The self-check runs once per `DataTablesLoaded` revision (or at the first map open after it) and only its result is cached against that `revision`; the map pointer never is.
- **Which table, and how the two keys pair.** `dt = 0x300a90(dataFlag)` takes a `dataFlag` below 4. The plugin already reads it from the unit as a byte at `UnitDataFlagOffset` `0x1bd` (`stat_apply.cpp:111`; the game's own helper is `0x34a0e0`). The SDK `getTable` takes a `Bank` (`Classic` 1, `Lod` 2, `Rotw` 3, `data_tables.h`). The header does not say how a data flag maps to a `Bank`, so the pairing is **not shown statically**; the self-check below proves it: use the `Bank` whose view matches the `dt` of the unit's data flag, and if none matches, the route stays off.
- **Self-check (the guard that matters), per revision.** With `dt = 0x300a90(dataFlag)` and `view` from `getTable(ctx, bank, TableId::MonStats, &view)`, require `view.rows == *(dt+0xf58)` and `view.rowCount == *(dt+0xf60)`; with the SuperUniques view, require `view.rows == *(dt+0x1078)` and `view.rowCount == *(dt+0x1080)`. This proves the bank / data flag pairing and the `dt` offsets directly, with no engine call. Then, before trusting `0xa121f0`, look up every monstats row's own `Id` through `*(dt+0xf70)` and require the returned index to equal the row's index (duplicates in different case aside). A mismatch or a null map turns the superunique route off and logs why. Do not call `0x3a9050` / `0x3a9150` (they log on a miss and are load-time callbacks) or the row getters (the SDK already gives the rows).
- **No hook is needed.** All calls are on the game thread at map open. Whether that is the thread the SDK treats as the captured game thread (the one running `EventsMapOpened`) is not shown; see Needs game.

### Needs game

1. Does `*(dt+0xf70)` resolve a stock `Id` to its row index at map open (the self-check), and `*(dt+0x1090)` a stock superunique name? Log the index and compare with the SDK row.
2. Is the miss value exactly -1 at runtime, with `logOnMiss = 0` printing nothing, and does an empty name return -1?
3. Does the 48-byte `Id` at monstats row `+0` match `monstats.txt` for a mod that renames monsters, and is `Class` at superuniques `+8` a monstats row index (compare with the txt's `Class`)?
4. What is the `u16` at superuniques row `+0`?
5. Does `getTable` return `Busy` on the thread that runs `EventsMapOpened` (the server player tick)? If so, the rows must be read from a `DataTablesLoaded` listener instead.
6. Does the self-check (`view.rows == *(dt+0xf58)` and the same for `+0x1078`) pass for the `Bank` the unit's data flag implies, and which pairing is it?
7. Does a stock monster `Id` resolve with a lower-case and an upper-case query alike (proves D2RCore inserts lower-cased keys)?
