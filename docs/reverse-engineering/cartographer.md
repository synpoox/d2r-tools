# Cartographer native contracts (`cartographer`)

**Status:** stub. Nothing here is validated in this repo; Cartographer has no plugin code in `plugins/` yet.
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are D2R.exe RVAs (image base `0x140000000` + RVA); final addresses will be found by byte signature, not fixed RVAs. The
`0x3e2b5ce` / `[0x3e2ac38]` row is a D2RLoader stub in D2R.exe whose jump target is in D2RCore (module not yet logged).
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
