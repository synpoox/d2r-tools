## 8a. Outdoor build hook (issue #141)

Code: `plugins/cartographer/src/engine/outdoor_hooks.{h,cpp}`, the pure plan in `src/outdoor/outdoor_plan.*`. Build 93847 (stamp `0x6ab3782c`). Unit tested (the plan, the cache, the seed arithmetic, the grid comparison) and built; the hooks ran in game on 2026-11-12 (single player, Hell): Jungle and Snow maps built as planned in every game, with arrival in the entrance cell and the Boss and events in their planned rooms (outdoor-check-2, outdoor-check-3, snow-check-1, map-item-type-check in `docs/cartographer/checks/`). The edges and void cells (#142) are still open.

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
- Preconditions, all read-only, otherwise the game builds its own level with the reason in the log: every part on, level type 3, drlg and the outdoor block (`level+0x18`) readable, no rooms yet, a map opened (`OutdoorMapOpened`), the slot row's OffsetX, OffsetY, SizeX, SizeY in `levels.txt`, `LevelType` equal to the tileset's (`OutdoorLevelType`), and a plan that passed `planStamps` and `checkStampPlan`.
- Then: write the rectangle (origin from the slot row, size from the plan; both are 0 for a slot when the build starts, section 4 S1), reseed (`[level+0x1e4] = int32(drlg[0x860]) + levelId`, `[level+0x1e8] = 0x29a`), call `OutdoorEntry`. Its Act 5 branch reaches our `OutdoorAct5Builder` hook, which, only when the thread's build context names this level, calls `PieceStamp(dataFlag, level, cx, cy, lvlPrest, file, 0)` per stamp and `VoidCell(level, x, y)` per void cell. Then the level type is set to 0 (no branch of the dispatch matches, so only the native tail runs, section 4 S1), the original `LevelBuild` runs, and type 3 is restored.
- Afterwards the built grid is read back (`+0x08` piece grid, `+0x58` flag grid through the row index) and compared with the plan (`compareGrid`); a difference is logged, not repaired. If the Act 5 branch was not taken (the slot level is not in Act 5 in `levels.txt`) the plan was not stamped, and the log says so.
- The plan is cached by (seed, tileset, shape, size, origin, limits, options), four entries, so the host's and a client's builds of one map share one computation. Failed plans are cached.
- `OutdoorProbe = 1` logs `[outdoor probe]` lines: the level before and after, the grid as characters, the differences, and the rooms with their pieces. It defaults to 0.

### After our build, and what is not hooked without an Outdoor slot

- **Second reseed.** The original's prologue (`0x3271e2..0x3271f0`) reseeds the level RNG again after our build returns control to it, so the level RNG ends at the fresh seed, not where a generator left it. The tail does not read it (section 8 Q5); the last fallback of `0x3da650` does. It is the same on the host and on a client.
- **No Outdoor slot installs nothing.** `InstallOutdoorHooks` finds the parts (for `cartographer check`) but hooks neither `0x3fe2e0` nor, when the maze steering did not take it, `0x3271c0`, when slots.txt has no Outdoor slot (one log line says so). The game then runs as without the outdoor code. Outdoor maps are built only when a map is opened.
- **Act 5 only.** `PrepareSlot` refuses, before any write or engine call, unless the Outdoor slot row in levels.txt has `Act` = 4 (Act 5) and an id above 137 (a vanilla level or a town is never rebuilt). Otherwise `0x3ee320` would dispatch to a vanilla Act 1-4 builder (`0x3ee484..0x3ee4db`) on our level and rectangle. A missing `Act` column also refuses. The game then builds the level itself.

### Slot row requirements (found by the outdoor spike, PR #168)

- The Outdoor slot's levels.txt row needs `Vis0` to `Vis7` = 0. With `-1` the game crashes at image `+0x3268fb`. The installer writes the row (issue #144) and is not changed here; this is recorded for that change.

### Entry, boss and event rooms (issues #143 and #145): seen in game

- After a build that matched the plan, `ArrangeRooms` reads the level's room list and reduces three plan cells to one room each with `chooseCellRoom` (the room wholly inside the cell nearest its centre; a cell is 32 x 32 tiles, a room 8 x 8): the entrance cell, the event cell and the boss cell. The S2 spike (PR #168) saw the arrival land in the entry cell only when the spike set `0x30000` on a room of it.
- Arrival: `planArrival` gives the entrance cell's room `0x30000` in room `+0x50` and clears the bits on every other room, as the maze `MarkArrival` does (same three guard parts `ArrivalFinder`, `ArrivalRoomFinder`, `ArrivalFlagTest`, checked only; a missing one leaves the arrival to the game). Refused, with the reason in the log, when the build is not the plan, a room lies outside the plan's level rectangle or the cell holds no room. `OutdoorProbe = 1` logs the player's first room in the slot level and whether it is in the entrance cell (`[outdoor probe] arrival`), driven from the same tick as the maze probe.
- Boss and event: the chosen rooms' exact DRLG rects are what `OutdoorBossRoom` / `OutdoorEventRoom` return; `event_apply.cpp` uses them for the Outdoor slot level exactly as it uses the maze's (the tracker compares the room rect), so the map Boss (the `bosses.txt` entry, one monster at the room centre) and the event go in that one room. No event cell: the event falls back to the distance rule.
- Not done: population per clearing. The S2 spike's clearing monsters came from rows cloned from Act 5 row 111 (the level's own monster data), so the layout does not decide them; the slot level's `Monsters`/`Density` (section 15) are the only control for now.
- Seen in game (outdoor-check-2, outdoor-check-3, snow-check-1): 9 of 9 arrivals in the entrance cell, the map Boss spawned as a superunique in the boss room, and Horde and Treasure events fired in the event room.

### Open

- The void marker is the `VoidCell` path (piece 0, flag `0x100`). Whether that leaves a walkable-free hole, or a fill ring is needed, is issue #142; the S2 spike (section 16) saw the void edge as acceptable in game (dark rock, no hole).
- The entry room: whether the plan's entrance cell is where the player arrives is also #142 (section 16: a portal arrival landed in the entry cell, with a room flag the spike set).
- A Jungle map opens through `portal.cpp` (#144, see "Opening an outdoor map" below); that path ran in game from the item (map-item-type-check).
- `VoidCell` and `OutdoorEntry` were only ever reached by the spike (a hooked entry, direct stamp calls); calling `VoidCell` directly is untested.
- Not run in game: multiplayer, and the edges (#142).

### Opening an outdoor map (issue #144)

A Jungle map opens through the Outdoor slot from its item (`portal.cpp`: `OutdoorProblem`, then `OutdoorMapOpened` before the portal, `OutdoorMapCancelled` if none opens). `tools/install.ps1` writes the level "Cartographer Outdoor": Act 5, DrlgType 3, LevelType 21, 384 x 384 at (6000, 3000), copied from level 111, Vis0..7 = 0, no lvlprest or lvlmaze row (none was needed by the spike, section 16). The open path ran in game from the item (map-item-type-check). Multiplayer: the host sends the outdoor seed through map sync (see "Multiplayer layout sync" below). The installer sets `SubType` and `SubTheme` to -1 on the Outdoor row only (the Maze row is unchanged), so the Siege sub-level set of level 111 is not carried over. Before the portal opens, `OpenMapPortal` makes the plan (`OutdoorPlanProblem`, the same cached plan the build uses) and refuses with its reason if it fails, so the game's own Act 5 builder never runs on a Jungle-typed slot. After the slot level has been built with a map open (`OutdoorSlotBuilt`, a `SlotBuildState` like the maze's, cleared on leaving the game), or while it is being built, a second outdoor open is refused with a message. The installer's repair path for an existing "Cartographer Outdoor" row fixes Quest, LevelGroup, Entry, Warp, SubType, SubTheme and Vis (Act, DrlgType, LevelType, size and offsets are refused by the plugin at open instead).

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
