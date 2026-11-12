# Cartographer native contracts (`cartographer`)

**Status:** sections 1 and 2 are a stub; nothing is validated in this repo. Section 3 records phase 1 (`plugins/cartographer`).
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

**Date:** 2026-11-11. **Status:** code written and built; not installed into a game and not run in game. Nothing here is
validated at a game level.

Cartographer finds every part below by byte signature at load. A part needs exactly one match in the game's executable
sections; otherwise it stays off, the reason goes to the plugin log and `cartographer check`, and the portal hooks are
not installed (fail open: the item then does what D2R does with an unknown spell). The portal is live only when all
required parts are on. Hooks go in use-first: the use hook does nothing until the check hook is in, so no portal can open
ungated. A signature for any part can be supplied without a rebuild in `signatures.txt` (columns `Part`, `Signature`),
beside `maps.txt`; it replaces the built-in one.

Build 93847, image stamp `0x6ab3782c`. The RVAs are evidence from pd2-resurrected `plugin/core-maps/map_open.cpp` and
`plugin/common/pd2_engine.h`; the plugin does not use them.

| Part | Role | RVA (evidence only) | Signature in the plugin | Required |
|---|---|---|---|---|
| ItemSpellCheck | Hooked. `(game, player, item, target) -> u64`; item spell 16 is the map | `0x5817f0` | entry bytes `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18` (15); uniqueness not verified, and if it matches more than once the part is off | yes |
| ItemSpellUse | Hooked. `(game, player, item, target)`; runs the opening | `0x581680` | entry bytes `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57` (14); uniqueness not verified | yes |
| ItemRecord | Called. `(dataFlag, row) -> record`; record +0x80 code, +0x94 pSpell | `0x314110` | none yet | yes |
| UnitClassOf | Called. `(unit, file, line) -> row` | `0x349860` | none yet | yes |
| UnitRoom | Called. `(unit) -> room` | `0x34b440` | none yet | yes |
| RoomLevelId | Called. `(room) -> level id` | `0x2efc10` | none yet | yes |
| ClientMessage | Called. `(player, 0x15, player)`: "can't do that" | `0x491960` | none yet | yes |
| OpenRedPortal | Called. `(game, player, key, level, max)`: red portal 60 by the player; counts openings per key and refuses at `max` (the one-map-per-game rule) | `0x5da800` | none yet | yes |
| GameCounter | Called. `(game + 0x1aac8, &key) -> int*`; key `'MAPS'` (`0x4d415053`) | `0x5d88b0` | none yet | yes |
| UnitStat | Called. `(unit, stat, 0) -> int`; stat 12 is the level | not in the registry; to be located from a dump | none yet | yes |
| UnitCoords | Called. `(unit, int xy[2])` | `0x34a040` | none yet | no (spot check skipped) |
| FindFreeSpot | Called. `(game, room, xy, 3, 0x400, &found, 5, 100)` | `0x517f90` | none yet | no (spot check skipped) |
| PlayerTick | Hooked (issue #40). The player mana step `(game, player) -> al`; the server runs it once per frame for every live player, so the console command gets a game and a player there. Optional: the map item does not need it | `0x42e350` | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA 4C 8B F9` (24 bytes, hook cuts the first 14: five whole instructions, no relative operand) | no (without it `cartographer open` arms the next item) |

PlayerTick evidence (static, read-only, against `D2RLoader_6ab3782c_dump.exe`): the 24-byte signature has 1 match (RVA
`0x42e350`; its first 14 bytes alone match 15 places, so the whole signature is used). `rdx` is the player and `rcx`
the game (saved to `rdi`/`r15` at entry). Its only caller is the player regen tick `0x42e600` (better-auras.md section 4),
which reaches it on every path for a live player (`0x42e74d`). `0x42e600` itself is hooked by Better Auras, and a second
hook on the same entry is refused, so the tick part uses `0x42e350` instead. Contract: the hook calls the original
first, then serves a posted console request when the unit is a player (`+0` == 0) whose unit id (`+8`) equals the
`playerId` of the SDK's `LocalPlayerReady` event, using the SDK event as the source of "which player is local". That
`playerId` is the unit id is an assumption (not documented by the SDK): if it is wrong, the request is dropped after 10 s
with a log line naming both ids. Not validated in game.

Game layout used: `game + 0x104` difficulty (u8), `game + 0x106` data flag (u8), `game + 0x1aac8` counters; item
record `+0x80` code (4 letters), `+0x94` pSpell. Item spell 16 is free in vanilla D2R. The town is Harrogath, level 109.

Guard: one signature match per part, and D2RLoader re-checks the copied entry bytes of the two hooks. Failure mode for
every part: fail open, log the reason, leave the portal off. Coexistence: another plugin hooking the same dispatchers is
not handled; D2RLoader refuses a second hook on the same bytes and the part then reports that.

What remains: dump the running game (`d2rAI-toolkit/tools/dump_image.py`, read-only), record a unique signature for each
"none yet" part, confirm the two entry signatures are unique, then run the game check listed in the plugin README.
