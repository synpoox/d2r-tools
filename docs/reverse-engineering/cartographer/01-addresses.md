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
| `0x3f3970` | Room loader `(dataFlag, room)`: adds the level row's files for the set bits of `[room+0x80]` (row at `[level+0x1e0]`, files `0x3c` bytes apart), plus three fixed DT1s. Signature recorded and unique in 33. Planned hook for mixed tilesets (plan phase 9) | §33; outdoor plan; cartographer-plugin.md |
| `0x32c300` / `0x32bdf0` | LvlTypes row getter `(levelType)` (row stride `0x781`) / automap tile lookup `(levelType, tileType, style, sequence)`. Unsigned here, static only | §33 |
| `0x86bc0` | Client act load: preloads only the current act's LvlTypes DT1s. Fallback hook for cross-act tiles | outdoor plan |
