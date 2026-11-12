# Cartographer native contracts (`cartographer`)

**Status:** the nine maze rows in section 1a and the twelve phase 1 parts in section 3 have byte signatures, checked statically against a build 93847 dump (no game
evidence; the portal hooks are written, not validated in game). Section 5 answers the maze slot's level size, origin and `0x3eb850` questions statically (issue #62), with the in-game checks still open. The outdoor rows (section 4) have byte signatures that matched on build 93847 in a test slot, and the spike exercised the parts live at the levels shown per row. Every other row of sections 1 and 2 is still a stub, not validated in this repo.
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
| `0x3e96d0` | Maze random walk `(level)`: adds rooms until the room count is reached. Replaced for the map level | drlg-steering.md; steer-proto |
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
| Consumer | Cartographer guided maze (plan section 5, phase 3); no hook exists yet |
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

## 2. Not yet recorded

Signatures for `0x3e96d0`, `0x3f3970` and `0x86bc0` (the other rows have signatures in section 1a or, for the outdoor rows, section 4, or are found from a signed function by following its calls, as `0x367110`, `0x3e2b5ce` and `0x3e2b5c8` are), contracts beyond the roles above, and any game validation of the maze rows. Until then no hook may be
written from this page alone for the rows without a signature (AGENTS.md); the 1a rows are static only.

## 3. Phase 1 parts: the map item and the red portal (`plugins/cartographer`, issue #20)

**Date:** 2026-11-11 (validated 2026-11-12). **Status:** validated in game on build 93847, image stamp `0x6ab3782c`
(D2R 3.3.0, D2RLoader 1.3.1-beta, test slot with the D2RToolsTest mod and a test save, Normal difficulty): all thirteen parts
found by signature at load, the three hooks installed, the item opened its own map's portal, the map loaded and returned,
a second open was refused, the Hell gate refused, the console `open` worked, and the tick's unit id matched
`LocalPlayerReady.playerId`. Not validated in game: a Hell-difficulty open, the level and free-spot gates, an ordinary
spell-16 item that no map names compared with vanilla (the pass-through of the two item hooks), the spell-16 use with no
matching check, leaving the game with the portal open, and the TCP/IP client refusal. Open, cause unknown: a town portal
scroll used on the map level itself did nothing; no comparison without Cartographer was made.

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
checks in the plugin README. Not validated in game.

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

What remains: the in-game check listed in the plugin README and the #20 pull request (install, the signatures all
resolving at load with `cartographer check`, the hooks going in, one opening and the refusals). Nothing in section 3 is
validated in game, and the hook entries and `UnitStat`'s thunk are only checked in the dump of a running process.

## 4. Outdoor S0 and S1 spike results (issue #22)

**Spike:** `spike/cartographer-outdoor-s1-finish` at 7f59e16 (`plugins/cartographer-outdoor-spike/`, never merged; S0 and the level build ran at 8d377ca, the position, collision and walk tooling is 9eeaedc, 7f59e16 adds the load-time stamp and loader log).
**Run:** build 93847, D2RLoader 1.3.1 (PluginSDK 0.4.0), vanilla RotW test slot 11 (mod `D2RToolsTest`), test character
(Barbarian, Normal), 2026-11-11 (first run) and 2026-11-12 (finishing run). The owner's install was not touched.
**Validation levels reached:** clean build; installed; live in the test slot (signatures, hooks, a level built in game, a
logged-position walk of the whole line in one run, own-room collision reads, automap screenshots). Live in game means the
position log, the collision reads and the screenshots below; unit tests do not apply to the spike. The DS1 walk flags
were only a supporting, offline read.
**Build identity (logged live at load by the spike, 7f59e16):** PE TimeDateStamp `0x6ab3782c`, loader build strings
`3.3.0` / `93847`, plugin ABI 4, PluginSDK 0.4.0, D2RLoader v1.3.1-beta (from the loader banner on screen; the loader
does not expose its version through the context).

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
- **Walking (live, one continuous run, route-guided):** the earlier two-segment walk (x 148 to 89 and x 11 to 70, all four seams crossed) is superseded. In the finishing run the character went from tile 7 (`cheat goto 138,30037,15066`) to the east end in a single click-walk, driven by the logged position and a route computed from the own-room collision scan (below). It crossed all four seams (tiles 32, 64, 96, 128) and the former stall zone at x 70..89 with no stall in the routed phase, reaching subtile x 714 (tile 142.8); the game's own pathing then took it blind east to a dead end at subtile (751,64), tile 150.2 (`track off`: reached x 30034..30751, y 15032..15066). Tiles 0 to 6 and 151 to 159 were not walked (the east tail is almost certainly the closed outer edge of the last block; not proven). The run is route-guided, not free exploration: the route was planned from the collision scan, then followed by feedback clicks. The monsters' columns (64 to 72 and 75) were zeroed in the slot's `levels.txt` for the walks (restored afterwards); the first stall of the first run was a locked chest object, and one stall near tile 23.8 in the first attempt was a dead end of an over-eroded route, not terrain.
- **Collision (live, own-room only):** `0x350a20` (mask `0x1c09`) is reliable only inside the player's own room (it walks that room's near list; elsewhere it reports blocked, about 98 percent of the level). The finishing run teleported the player into each room (`cheat goto`) and kept the own-room window: 62 of 80 rooms were sampled. Missing: rooms (0,0), (0,3), (3,0), (4,0), (4,3), (5,0), (6,0), (7,0), (8,3), (11,0), (12,3), (15,0), (16,0), (17,0) and the whole column 19 (non-walkable targets snap to the entry, so those rooms could not be entered by `goto`). The merged map is a lower bound on open ground only where sampled; unsampled rooms count as blocked. From it the walk route was found, and the route was then confirmed by the character actually standing along it (logged positions), which is the evidence, not the map alone.
- **Automap (live, compared by eye):** the automap draws a continuous blue dotted line along the whole line (both ends and
  across the seams seen) with tree rows flanking the corridor, matching `[E][EW][EW][EW][W]` running east-west. It was
  compared by screenshots (`automap_east`, `automap_west`) at two positions, not cell by cell and not against a mask dump.
  Verdict: consistent with the masks; not an exhaustive check.
- **River or path (decided: both exist, the line is traversable):** the dark channel at x 70..90 is water (a river, or a lake where the line widens), impassable by design: a straight click walk into it stalled, as the earlier runs did. It does not cut the line: the banks and a north shelf are walkable ground. The live route kept to the north bank (rows about 9 to 12), detoured up to rows about 6 to 8 around the x 70..90 water body, then ran along rows about 8 to 11 to x 142 and crossed every seam on foot. So the stamped line is a navigable path with a river beside and through it, not a river-only line. Evidence: live in game (position log, own-room collision, the click walk). Offline DS1 walk flags (catalogue tool, `lay.unwalkable` = cell flag `0x20000`, subtile flag = DT1 bit 0) were ambiguous: honouring the flag cut the line near x 22, ignoring it left everything open, so they are not used as evidence. Not covered: the S2 questions (an L-shape plus a branch, a closed edge next to an occupied cell).
- **Earlier S1 conditions not redone in the finishing run:** render, automap, monsters, host and client logs, leave and return, and new game are from the first run (above). The finishing run did not repeat them, so issue #22 is `Refs`, not `Closes`.
- **Slot used:** 11 (`D2RToolsTest`). Game stopped, slot unlocked, the spike DLL removed, and the slot's plugins, config, logs,
  `levels.txt` and save folder restored from a backup taken before the runs.

## 5. Maze slot engine checks (issue #62)

Answers the four static questions of plan section 3.6 [V] for the guided maze slot. Everything here is read from the build 93847 dump (Capstone
disassembly; Ghidra was only a reading aid); nothing was run in a game. Where a conclusion needs the game, the check is given as "needs game: set X, observe Y".

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer guided maze hook (plan section 5, phase 3b); no hook exists yet |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the same memory-layout dump as section 1a (`D2RLoader_6ab3782c_dump.exe`) |
| Location | RVAs in the table below. Offsets are the DRLG level record (one per level per act; list head at `drlg+0x868`, next at level `+0x1b8`): `+0x00` DRLG type (1 maze, 2 preset, 3 outdoor), `+0x08` room count, `+0x10` first room, `+0x18` LvlMaze row, `+0x24/+0x28` origin x/y (tiles), `+0x2c/+0x30` width/height (tiles), `+0x1f8` level id. Room: `+0x48` next, `+0x50` flags, `+0x60/+0x64` x/y, `+0x68/+0x6c` w/h |
| Guard | The section 1a rows keep their signatures. The functions in the table below are unsigned, read-only evidence, except `0x3271c0`, which is already signed in sections 1 and 4 (its row is listed for the masked prefix only; do not record a second signature). The others were followed from signed functions or from the level build `0x3271c0`. The masked prefix shown for each matched exactly once across `.text`, `.maho`, `.koishi` and `.btext` of the dump (relative displacements and RIP-relative operands masked), so it can become a signature later; none has been checked in a running game. A hook that needs one of them records its signature and max patch length first |
| Contract | See the answers below |
| Evidence | Disassembly (Capstone) of the functions named, over the dump (the Ghidra decompile was only a reading aid and is not needed). Reproduce a listing with `python -I plugins/cartographer/tools/dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]`, and a prefix match with `... pat "<hex with ??>"` (read-only; refuses a different stamp; needs `capstone`). Example: `dis 0x32751e 3` shows `mov rdx, rbx / movzx ecx, r12b / call 0x3271c0` |
| Validation | Static only. No game, no hook. Not checked on Steam build 94995 |
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

### Left for the game

1. The whole game accepting a 360 x 360 maze level (Q1, size and lopsided checks).
2. The origin being clear of every other level's rect, including outdoor levels (Q2 log).
3. A slot id built with its second `0x3eb850` call suppressed: clean build, arrival and walk (Q3).
4. Optional: sizing the level to the layout span instead of the centred capacity (Q1 lopsided check).
