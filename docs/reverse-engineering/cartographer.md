# Cartographer native contracts (`cartographer`)

**Status:** sections 1 and 2 are a stub; nothing is validated in this repo. Section 3 records phase 1 (`plugins/cartographer`): its twelve parts have byte signatures, checked statically against a dump (no game evidence).
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are RVAs (image base `0x140000000` + RVA), except
the D2RCore thunk entry below.
**Evidence source:** the pd2-resurrected research and spikes, not re-checked here:
- `docs/research/drlg-steering.md` on pd2-resurrected branch `docs/drlg-steering-spike` (pull 929);
- `docs/research/level-regeneration.md` on pd2-resurrected branch `claude/level-regen-spike-117061` (pull 930);
- `spike/cartographer-steer-proto`: the maze steering prototype (`plugin/cartographer-steer-spike/plugin.cpp`);
- `spike/cartographer-level-regen`: the level regeneration spike;
- `spike/cartographer-outdoor-s1` at b78cfe2: the outdoor builder spike (uncompiled).

All repository links are under https://github.com/synpoox/pd2-resurrected.

Every row below has: Build 93847, Guard "not yet: needs a byte signature", Status "not validated in this repo", and
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

## 2. Not yet recorded

Entry bytes, signatures, contracts beyond the roles above, and any validation in this repo. The first Cartographer
probe step (outdoor plan, S0) is meant to produce the byte checks. Until then no hook may be written from this page
alone (AGENTS.md).

## 3. Phase 1 parts: the map item and the red portal (`plugins/cartographer`, issue #20)

**Date:** 2026-11-11. **Status:** code written and built; byte signatures for all twelve parts recorded and checked
against a dump of the running image (static). Not installed into a game and not run in game; nothing here is validated at
a game level.

Cartographer finds every part below by byte signature at load. A part needs exactly one match in the game's executable
sections; otherwise it stays off, the reason goes to the plugin log and `cartographer check`, and the portal hooks are
not installed (fail open: the item then does what D2R does with an unknown spell). The portal is live only when all
required parts are on. Hooks go in use-first: the use hook does nothing until the check hook is in, so no portal can open
ungated. A signature for any part can be supplied without a rebuild in `signatures.txt` (columns `Part`, `Signature`),
beside `maps.txt`; it replaces the built-in one.

Build 93847, image stamp `0x6ab3782c`. The RVAs are evidence from pd2-resurrected `plugin/core-maps/map_open.cpp` and
`plugin/common/pd2_engine.h` (UnitStat's from d2rAI-toolkit `rvas_93847.h`); the plugin finds the parts by signature and
does not use them. The signatures are in `plugins/cartographer/src/core/part_signatures.h`.

| Field | Record (issue #39) |
|---|---|
| Date | 2026-11-11 |
| Consumer | Cartographer portal (`src/engine/portal.cpp`) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, under D2RLoader 1.3.1; memory-layout image of the running D2RLoader process (d2rAI-toolkit `tools/dump_image.py`), SHA-256 not recorded |
| Location | Function entry RVAs in the table; every signature starts at the entry (offset 0), so the match address is the function |
| Guard | The whole signature compared under its mask at the resolved address (D2RLoader also re-checks the copied entry bytes of the two hooked parts). Max patch length: bytes from the entry before the first relative branch, call or RIP-relative instruction; both hooks copy fewer (15 of 44, 14 of 38) |
| Contract | Roles and signatures in the table. Wildcards (`??`) cover relative branch and call targets, RIP-relative displacements, struct-table offsets (`ItemRecord`: the table count offset `+0x15a8`) and the thunk's jump slot (`UnitStat`). One exception: `GameCounter` keeps one call displacement unmasked (below) |
| Evidence | Disassembly of each entry (Capstone, over the dump). Each signature is the shortest whole-instruction prefix of at least 24 bytes that matches once, with masked bytes as wildcards, across all executable sections (`.text`, `.maho`, `.koishi`, `.btext`; each section's VirtualSize, as the plugin scans). Roles checked from the code: `OpenRedPortal` calls `UnitRoom`, a room check, `GameCounter` on `game + 0x1aac8` (read, and incremented after a successful opening), `UnitCoords` and `FindFreeSpot` (`3, 0x400, 5, 100` as recorded), a create call with object 60, and `ClientMessage(player, 0x15, player)` on a refusal; `UnitClassOf` returns `[unit + 4]`; `RoomLevelId` follows `[room + 0x18]` into a function reading `[[level + 0x90] + 0x1f8]`; `ItemRecord` is a bounds-checked `base + row * 0x1c0` over a vector at table `+0x15a0` |
| Validation | Static only: `plugins/cartographer/tools/check_portal_signatures.py` on the 93847 image, 12 of 12 unique and resolving to the recorded RVAs; unit tests parse every signature. Not installed into a game, not run in game, not checked on Steam build 94995 |
| Failure mode | Fail open: a signature with no match or more than one match, or a guard mismatch, logs which part and leaves the portal off |
| Coexistence | The dump is of a D2RLoader process, so detours already in place show in it. None of the hooked or called entries starts with `E9`; `UnitStat` starts with D2RLoader's own `FF 25` thunk (below). Another plugin detouring a part later fails that part's guard |

| Part | Role | RVA | Signature (offset 0 = function entry) | Guard bytes / masked | Max patch | Required |
|---|---|---|---|---|---|---|
| ItemSpellCheck | Hooked. `(game, player, item, target) -> u64`; item spell 16 is the map | `0x5817f0` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 30 / 0 | 44 (hook copies 15) | yes |
| ItemSpellUse | Hooked. `(game, player, item, target)`; runs the opening | `0x581680` | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 24 / 0 | 38 (hook copies 14) | yes |
| ItemRecord | Called. `(dataFlag, row) -> record`; record +0x80 code, +0x94 pSpell | `0x314110` | `40 57 48 83 EC 30 8B FA E8 ?? ?? ?? ?? 3B B8 ?? ?? ?? ?? 72 ?? 33 C0 48 83 C4 30` | 27 / 9 | 8 | yes |
| UnitClassOf | Called. `(unit, file, line) -> row` | `0x349860` | `48 83 EC 28 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC B8 FF FF FF FF 48 83 C4 28 C3 8B 41 04` | 41 / 6 | 7 | yes |
| UnitRoom | Called. `(unit) -> room` | `0x34b440` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9 74 ?? 48 83 C4 20` | 63 / 10 | 12 | yes |
| RoomLevelId | Called. `(room) -> level id` | `0x2efc10` | `48 85 C9 75 ?? 33 C0 C3 48 8B 49 18 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 85 C9` | 35 / 5 | 3 | yes |
| ClientMessage | Called. `(player, 0x15, player)`: "can't do that" | `0x491960` | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 49 8B F8 0F B7 F2 48 8B D9` | 24 / 0 | 27 | yes |
| OpenRedPortal | Called. `(game, player, key, level, max)`: red portal 60 by the player; counts openings per key and refuses at `max` (the one-map-per-game rule) | `0x5da800` | `48 89 5C 24 08 48 89 6C 24 10 44 89 44 24 18 56 57 41 56 48 83 EC 70 48 8B E9` | 26 / 0 | 35 | yes |
| GameCounter | Called. `(game + 0x1aac8, &key) -> int*`; key `'MAPS'` (`0x4d415053`) | `0x5d88b0` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 41 08 4C 8B F2 48 8B D9 4D 85 C0 75 ?? 48 8B 11 E8 A4 3F 00 00` | 44 / 1 | 34 | yes |
| UnitStat | Called. `(unit, stat, layer) -> int`; stat 12 is the level | `0x2f5020` (d2rAI-toolkit `rvas_93847.h`; pd2-resurrected: a jump into D2RCore) | `FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA` | 26 / 4 | 0 | yes |
| UnitCoords | Called. `(unit, int xy[2])` | `0x34a040` | `48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 85 FF 75 ?? 48 8D 4C 24 30 40 88 7C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9` | 89 / 15 | 19 | no (spot check skipped) |
| FindFreeSpot | Called. `(game, room, xy, 3, 0x400, &found, 5, 100)` | `0x517f90` | `48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 90 00 00 00 48 8B 05 ?? ?? ?? ??` | 30 / 4 | 23 | no (spot check skipped) |

Evidence level of every row: static, dump (not game). All twelve RVAs are function entries in this build and none looks
wrong. Findings per part:

- **ItemSpellCheck, ItemSpellUse:** the 15 and 14 entry bytes recorded earlier were not unique (3140 and 11 matches in the
  executable sections), so with them both parts would have stayed off. Extended to 30 and 24 bytes, each unique; the hook
  sizes are unchanged and inside the max patch length.
- **UnitStat:** `0x2f5020` is a D2RLoader thunk, `jmp [rip+x]` plus four NOPs, into D2RCore (pd2-resurrected found the same);
  the bytes after it are the dead original body, and calling the thunk is the way to read a stat. The signature covers the
  thunk and the start of the dead body, so it exists only while D2RLoader's thunk is in place; without it the part is off.
  The jump slot is masked. Do not patch it.
- **RoomLevelId:** a 17-byte wrapper (`[room + 0x18]`, then a jump). Three other wrappers have the same masked bytes, so the
  signature takes in the 15 bytes of padding and the next function's first three bytes. It breaks if the neighbouring
  function changes or the padding does.
- **GameCounter:** an `unordered_map<int,int>` lookup, and three copies of it have identical bytes apart from their call
  displacements (`0x5d88b0`, `0xd8590`, `0x8723b0`). No fully masked signature tells them apart, so the first call
  displacement (`E8 A4 3F 00 00`, to the map's rehash helper) is kept unmasked. This is the most build-sensitive signature
  here: a game update that moves the helper turns the part off (fail open), and the signature is then re-derived.
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
