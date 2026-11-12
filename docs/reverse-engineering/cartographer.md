# Cartographer native contracts (`cartographer`)

**Status:** the outdoor rows (section 3) have byte signatures that matched on build 93847 in a test slot, and the spike exercised the parts live at the levels shown per row; everything else is still a stub. Cartographer has no plugin code in `plugins/` yet.
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are D2R.exe RVAs (image base `0x140000000` + RVA); final addresses will be found by
byte signature, not fixed RVAs. The `0x3e2b5ce` / `[0x3e2ac38]` row is a D2RLoader stub in D2R.exe whose jump target was
logged inside `D2RCore.dll` (section 3, S0). `0x3e2b5c8` (6 bytes before it) is the same kind of stub, also into `D2RCore.dll`.
**Evidence source:** the pd2-resurrected research and spikes, not re-checked here:
- `docs/research/drlg-steering.md` on pd2-resurrected branch `docs/drlg-steering-spike` (pull 929);
- `docs/research/level-regeneration.md` on pd2-resurrected branch `claude/level-regen-spike-117061` (pull 930);
- `spike/cartographer-steer-proto`: the maze steering prototype (`plugin/cartographer-steer-spike/plugin.cpp`);
- `spike/cartographer-level-regen`: the level regeneration spike;
- `spike/cartographer-outdoor-s1` at b78cfe2: the pd2 outdoor builder spike (uncompiled; reference only);
- `spike/cartographer-outdoor-s1` in this repository (synpoox/d2r-tools): the compiled spike `plugins/cartographer-outdoor-spike/`
  at 8d377ca (S0 probe, S1 jungle line) and the follow-up commit named in section 3; it is the evidence for section 3.

The pd2-resurrected links above are under https://github.com/synpoox/pd2-resurrected.

Every row below has: Build 93847, and Failure mode "fail open: log and leave Cartographer's map off". Rows marked "signed"
carry a byte signature that matched exactly once in the running image (section 3, S0 table); their Guard and Validation
(signature, hook, or called, and which levels) are in section 3. Rows not marked have Guard "not yet: needs a byte signature"
and Status "not validated in this repo". The registry's image stamp is `0x6ab3782c`; the spike did not log the stamp
itself (it was logged only in the follow-up run, section 3). Coexistence: D2RCore patches or thunks some DRLG code
(`0x3e2b5ce`); see that row.

## 1. Addresses

| RVA | Role (per the plans) | Evidence pointer |
|---|---|---|
| `0x3271c0` | Level build `(dataFlag, level)`: reseeds `+0x1e4`, dispatches on level `+0x00` (type 3 goes to the D2RCore stub), runs the tail. Signed; hooked and replaced for the slot level in the spike (§3) | outdoor plan §2; §3 |
| `0x3e96d0` | Maze random walk `(level)`: adds rooms until the room count is reached. Replaced for the map level | drlg-steering.md; steer-proto |
| `0x3eb850` | Maze entrance and exit placer `(level, int[4], int*)`. Steered | drlg-steering.md; steer-proto |
| `0x3eb960` | Maze piece picker `(room, clearFixed)`: piece = tileset base + exit mask (base hardcoded). Planned hook for the piece set | drlg-steering.md section 2.3; steer-proto |
| `0x3e9820` | Maze ring grower. No-op in the planned steering for more tilesets | drlg-steering.md |
| `0x3e77a0` | Maze add-room: adds one room to the maze. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x32c6a0` | LvlMaze lookup: finds the LvlMaze row for a level. Role per the steering prototype | `spike/cartographer-steer-proto` |
| `0x360bb0` | Links two rooms (called after the overlap checks `0x360520`, `0x360470`, `0x360c80`); also used to link loop edges | drlg-steering.md lines 148, 188 |
| `0x3ee320` | Outdoor grid builder; dispatches on act, calls `0x3fe2e0` for Act 5, then `0x3ee780`. Signed; hooked and counted in the spike, called for the slot level (§3) | outdoor plan 1.2; §3 |
| `0x3fe2e0` | Act 5 grid allocation, reached from `0x3ee320`. Signed; hooked in the spike, which runs the stamper for the slot level instead. Finding: `0x3ee320` reaches it only when D2RCore's act lookup `0x3e2b5c8` returns 4 for the slot id (it did, §3) | outdoor plan 1.2; §3 |
| `0x3ecfc0` | Stamps a piece `(dataFlag, level, cx, cy, lvlprest, file or -1, 0)`; sizes it from LvlPrest, marks cells, draws the file from the level RNG at `+0x1e4`. Signed; called from the spike's `0x3fe2e0` hook, not hooked (§3) | outdoor plan; §3 |
| `0x3ee780` | Makes the preset rooms from the stamped piece-origin cells. Signed; hooked and counted in the spike (§3) | outdoor plan; §3 |
| `0x3fa7f0` | Allocates one of the four grids (`+0x08`, `+0x30`, `+0x58`, `+0x80`). Signed only: matched, never hooked or called by the spike; the native `0x3ee320` / `0x3fe2e0` call it | outdoor plan 1.2; §3 |
| `0x3fa790` | Grid read `(grid + field, x, y) -> u64`, used for the grid dumps. Signed; called by the spike, not hooked | §3 |
| `0x367110` | Level reseed `(level + 0x1e4, seedBase + level id)`: the first `E8` call in `0x3271c0` (`+0x30`). Found by that call, not signed; called by the spike | §3 |
| `0x3fac10` | Act 1 per-act builder `(dataFlag, level)`. Signed; hooked and counted: still called for levels 2 and 3 when a game starts (§3). Callers not logged | outdoor plan, probe step S0; §3 |
| `0x3e2b5ce` / `[0x3e2ac38]` | D2RLoader stub `JMP [0x3e2ac38]` into D2RCore, taken by `0x3271c0` for type 3. Found by following the first `E8` in `0x3271c0` whose target starts `FF 25`; the pointer held an address inside `D2RCore.dll` (§3) | outdoor plan 2; §3 |
| `0x3e2b5c8` | D2RCore's act lookup `int8 (u8 dataFlag, int32 level id)`, a `FF 25` stub into `D2RCore.dll`. Found as the first `E8` in `0x3ee320` whose target starts `FF 25`, not a fixed RVA. Returned 4 for the slot id 138 (§3) | outdoor plan 1.1; §3 |
| `0x3f3970` | Room loader: adds a room's DT1s from `level+0x1e0`. Planned hook for mixed tilesets (plan phase 9) | outdoor plan; cartographer-plugin.md |
| `0x86bc0` | Client act load: preloads only the current act's LvlTypes DT1s. Fallback hook for cross-act tiles | outdoor plan |

## 2. Not yet recorded

The maze rows (`0x3e96d0` to `0x32c6a0`, `0x360bb0`), `0x3f3970` and `0x86bc0` have no signatures or validation in this file.
No hook may be written from this page alone for those (AGENTS.md).

## 3. Outdoor S0 and S1 spike results (issue #22)

**Spike:** `spike/cartographer-outdoor-s1` at 9eeaedc (`plugins/cartographer-outdoor-spike/`, never merged; S0 and the level build ran at 8d377ca, the position, collision and walk tooling is 9eeaedc).
**Run:** build 93847, D2RLoader 1.3.1 (PluginSDK 0.4.0), vanilla RotW test slot 11 (mod `D2RToolsTest`), test character
(Barbarian, Normal), 2026-11-11. The owner's install was not touched.
**Validation levels reached:** clean build; installed; live in the test slot (signatures, hooks, a level built in game, the
character walked both ends of the line with a logged position, automap screenshots). The line was NOT walked in one
continuous run (see S1). The image stamp (`0x6ab3782c`) and the D2RLoader version were taken from the registry and the
loader banner, not logged by the spike.

**Probe commands** (console, test harness): `cartographer pos`, `track on|off|reset` (logs `pos subtile ...` lines and the
reached extents), `coll <mask> [dump]` (collision at the player; `dump` writes an 800x160 subtile file), `peek <addr> <len>`,
`rooms <level ptr>`, plus the S0 commands. Config: the spike's own file sets the block line, offsets and size.

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

The signatures were built from a decrypted dump of the same build, because the `D2R.exe` on disk is encrypted. The
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
- **Rooms:** 80 rooms (8x8 tiles each, a 160x32 tile rect; plan §1.5 expected a piece/fill split: the room count and size are from the spike's LogRooms, which does not tell piece rooms from fill rooms) on both the server and the client drlg. The room lists and grid dumps of every build
  (server, client, client again after leaving and returning, and server and client in a second game) are identical.
- **Monsters:** spawn on the level (Frigid Highlands monsters, from the cloned levels.txt row; they attack).
- **Leave and return:** `cheat goto 1` then back to 138: the client rebuilt the level and the room list and grid dump
  matched the first build exactly. The server kept its level.
- **New game:** leave the game, start another, `cheat goto 138`: the level built again with the same rooms and grid,
  no crash. No crash dump was written and the spike's log shows no fault, in either game.
- **Position readout (live):** `cartographer pos` and `track` read the player's dynamic path (unit `+0x38`, x `+0x2`, y `+0x6`,
  subtiles; control unit `0x484c90`, byte-checked at load). Level origin is subtile (30000,15000). Validated live.
- **Walking (live, in two segments, not one run):** a click walk driven by the logged position went from tile x 148 west to
  x 89 (subtiles 30446..30741), crossing the seams at 128 and 96. A second run, after teleporting to the west end, went from
  x 11 east to x 70 (subtiles 30054..30354), crossing the seams at 32 and 64. Both stopped at a dark blocked channel
  (screenshot at x 89; x 70 stalled the same way). The stretch x 70..89 was not walked, so the end-to-end
  condition is NOT met as one walk; all four seams were crossed on foot. The monsters' columns were zeroed in the slot's `levels.txt` for
  these runs (restored afterwards), the first stall was a locked chest object.
- **Collision (partial, own-room only):** `0x350a20` is reliable only inside the player's own room (it walks that room's
  near list; elsewhere it reports blocked, about 98 percent of the level). The spike sampled the player in 65 of 80 rooms
  (the north-west and east edges were not reached) and kept each own-room window. That OR-merged map is a lower
  bound that over-reports open ground (it showed open tiles in the dark channel the character could not enter), so it is
  not used as proof. It shows a walkable band through rows 9..13 at all four seams and a 1897-tile connected component from x 5 to 150.
- **Automap (live, compared by eye):** the automap draws a continuous blue dotted line along the whole line (both ends and
  across the seams seen) with tree rows flanking the corridor, matching `[E][EW][EW][EW][W]` running east-west. It was
  compared by screenshots (`automap_east`, `automap_west`) at two positions, not cell by cell and not against a mask dump.
  Verdict: consistent with the masks; not an exhaustive check.
- **River or path (still open, leaning river):** the line is one row of blocks, so every seam is shared by two blocks
  and the mask alone cannot separate "river" from "path". What was seen: the dotted automap line and a dark flat channel run
  continuously through the corridor, and walkable ground exists on the same rows at all four seams. Which feature runs through
  each 32-tile seam was not separated with trustworthy collision data, so this stays open, leaning river.
- **Slot used:** 11 (`D2RToolsTest`). Game stopped, slot unlocked, the spike DLL removed and the slot's own plugins
  restored.
