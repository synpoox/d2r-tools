# Monster Count: phase 0-1 RE spike (build 93847)

Source: the user's in-memory dump `D2RLoader_6ab3782c_dump.exe` (memory layout, file offset = RVA). No Ghidra
install or analysed project was available in the work environment, so the toolkit's Ghidra scripts could not run
and `rea` (which needs Ghidra 12.1) could not either. Analysis used `tools/re_static.py` (Capstone + `.pdata`,
byte-scan xrefs: the same queries as Refs / Disasm / StringXrefs / FindDisp). Addresses: `plugin/src/d2r.h` and
`knowledge/d2r-93847-addresses.md` (*Monster ledger and spawning*). Nothing has been hooked in game.

## Phase 0: environment
- PE TimeDateStamp `0x6ab3782c`, "Game Version 3.3.0", "Game Build 93847": matches.
- D2RLoader FileVersion **`1.3.1-beta`** (built from `remotes/origin/beta/master`). The toolkit targets "1.3.1";
  same stamp as the registry, so the registry's addresses apply, but confirm this is the loader you ship with.
- Only the dump was checked, not the live install (the install is not reachable from here).

## 1a. Spawn
`0x492cf0` is only a wrapper: it packs its arguments and calls the **spawn core `0x493cc0`**. 12 wrappers (15
call sites) reach the core. Every path found goes through it: level populator (`0x50acf0` -> `0x492e70`,
`0x50b1b0`, `0x50b4d0`, `0x50b7e0`, `0x509cc0`), champion/unique packs and minions (`0x499ba0`, `0x499880`,
`0x49ead0`), summons (`0x4350e0` -> `0x492cf0`), missile hit 6 (`0x50a050`), revived inactive units (`0x505d70`
-> `0x509cc0`), quests. Monster units allocated outside the core: hirelings only (`PlrSave2.cpp` `0x5331f0`,
`0x531790`, `0x53bd30`). All other allocator sites create objects.

## 1b. Death
`0x44db20` entry unchanged (no `FF 25`). D2RLoader's `LifecycleService` already offers a **MonsterDeath listener**
(server thread, level id included), so no death hook is needed. Pets: alignment 2 (below), never counted.

## Key finding: D2R already counts
`MonsterRegion.cpp` keeps a per-level ledger: `*(game+0x1d0)[levelId]` -> `+0x2cc` spawned, `+0x2d0` killed.
- +spawned at the spawn core unless monstats `neverCount` (inferred bit) or spawn flag 8.
- +killed when a counted, **hostile** (alignment 0, `0x2f4190`) monster enters death mode; -killed on resurrect.
- Alignment change hostile -> ally (Conversion, summons) removes it; back to hostile re-adds it.
- Removal through the inactive-unit code adjusts it.

D2R's own Den of Evil check (`0x5dde20`) completes on `spawned == killed`. This is exactly the requested
predicate: monsters, hostile, not pets/summons/mercs/NPCs (alignment), `neverCount` excluded, hirelings never
enter it. Not excluded: hostile monsters flagged not-killable in monstats (check the data, phase 2).

## 1c. Room -> level
`0x2efc10`: room +0x18 -> +0x90 -> +0x1f8. Call-only (not hookable). D2RCore also exports
`GetLevelIdFromRoomContext` (ordinal 2001); D2RLoader reimplements some level code (`.maho` thunks).

## 1d. Timing: room by room
Monsters are created when a room **first activates** (`0x408670` / `0x4042a0`: room `+0x54` bit 0 set once after
the populator `0x50acf0` runs), not at level generation. So "remaining" is only known for rooms the player has
come near. D2R itself waits for all rooms to be populated before closing the Den of Evil.

Options (decision needed):
1. **Lower bound with a "+"**: show `spawned - killed` and add "+" while the level still has unpopulated rooms
   (compare populated room flags against the level's DRLG room count). "7+ monsters remain" in a half-explored
   level, exact once every room has populated. Recommended.
2. Plain lower bound, no unexplored marker: simplest; the number can go up as you explore.
3. Force-populate the whole level on entry: exact from the start, but changes gameplay (all monsters exist at once,
   AI/performance, D2RLoader owns parts of this code). Not recommended.

## 1e. Top-right text
- The native automap details (game name, difficulty, area; D2RLoader appends its own lines there) are drawn by
  client code I could not pin down statically in a few focused attempts (`AutoMapPanel::Draw` is a profiler label
  with no direct refs). Hooking it would also sit next to D2RLoader's own patch.
- **Cheapest safe route: PluginSDK `OverlayService`** (display-only text after the game UI, no hooks), placed in
  the top-right from `Frame::screenWidth` and `measureText`. Risks: the loader's font, not D2R's; fixed Y offset
  below the native lines (their count depends on settings); visibility must follow the automap (try
  `WidgetService::findPanel("AutoMapPanel")`, to be tested in game). Fallback: the status console command.

## Data flow (proposal for phase 2)
No service gives the server `Game*`. Plan: one cold-path hook on `0x509520` (ledger spawn; hookable, 15 bytes)
records `regions = rcx` for the current session; a `runOnGameThread` task every ~250 ms reads
`regions[level]` for the level from the `LevelChanged` event and publishes an atomic count for the overlay.
`GameLeft` clears it (queued game work of the old session is discarded by the loader). Optional: hooks on the
other four ledger mutators for instant updates. No unit state, save or table is changed.

## Needs the user's machine
- `modkit show monstats` to confirm the `neverCount` / `enabled` / `killable` columns and list hostile
  non-killable monsters that would keep the count above 0.
- The automap layout (`modkit widgets` on the automap layout) for the overlay's anchor and visibility.
