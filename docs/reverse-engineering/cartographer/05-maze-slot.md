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
- **Recommendation:** Depend 0, OffsetX past the act's envelope plus a margin (6000 for any act, as the S1 spike used in Act 5; but a level whose layer is not the town's must also fit the 16-bit automap range or it loses its revealed map after a town trip: OffsetX minus OffsetY within -3736..3735 and OffsetX plus OffsetY at most 7471 for 360 tiles, so the plugin uses 3500,0, see note 22), any OffsetY, origin + 360 well under 13107. Outdoor rows (DrlgType 3) have Offset -1 or arbitrary values and their rects are set by act start-up (`0x3f0130` -> `0x3f1c60`), so their positions are not known statically.
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
