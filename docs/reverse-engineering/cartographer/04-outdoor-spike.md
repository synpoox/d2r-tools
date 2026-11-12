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
