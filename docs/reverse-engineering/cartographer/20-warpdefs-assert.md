## 20. `warpDefs` assert at map entry (issue #254)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. Static reading only
(no game run); every claim below is from disassembly unless marked inference.

### The message

`BC_ASSERT: i != dataTable->warpDefs.end()` at `D2Common\src\DataTbls\LvlTbls.cpp:817`, "Error in DRLG related to stairs that connect levels", logged once at
map entry in two of three games of the names/drop check (`docs/cartographer/checks/2026-11-12-names-drop-check.md`), with "Continuing because assertion
dialogs are disabled".

### The lookup that fails

- `0x32c980(dataFlag, warpId, dir)` finds a row of the `lvlwarp.txt` table (vector at tables `+0x1890`, count `+0x1898`, row `0x60`) with `row+0x2c == warpId` and
  direction `row+0x5c` equal to `dir` (either side may be `'b'`, which matches any). The assert is at `0x32ca26`, in the not-found branch. After the assert
  the function returns **NULL** (`cmove rbx, 0` at `0x32ca55`), not an end pointer.
- `warpId` is `Warp[k]` of the level's `levels.txt` row (`0x3daad0(dataFlag, level, k)`, row `+0x68 + k*4`, or the drlg override list). A level with `Warp[k]` = -1 has
  no such row, so the lookup cannot succeed.
- Callers: `0x3da970` (wrapper, callers `0x361750` and `0x3f4670`), `0x3f4120` and `0x3f4527` (each re-look a room's warp row with direction `l`/`r`).
  `0x3f4670` is the one on the path of a room's stairs tile: the tile switch `0x3f332c` sends tile orientation 10 and 11 (special tiles, left and right) to it with
  `k = (tile flags >> 20) & 0x3f` and direction `l` (or `r` for 11).
- The other path, `0x361750` (room warp-bit loop, called from `0x3608a0`), is **not** the cause for the slot: `0x3609dc` skips every exit whose `Warp[k]` is -1 before it
  gets there, which is why the installer's -1 matters there. The stairs-tile path has no such skip.

### Reading

A DS1 piece with a stairs-type tile (orientation 10 or 11) loaded into the slot level asks for `Warp[k]`. The slot row has `Warp0..7` = -1 and `Vis0..7` = 0 (by design: no
exit), so the lookup misses and asserts. Which piece has the tile is **inference**: the entrance room is the tileset's "Prev" (up stairs) piece (`maze.h`), which is the one
a vanilla Cave level fills with stairs. The DS1 was not opened here. It also fits that the Crypt tileset was not run.

Outcome: `0x3f4670` sees NULL and returns 0 (`0x3f46ca`), so no warp link is made for that tile; the callers at `0x3f3434` and `0x3f4560` carry on. The follow-up
`0x3f4120` finds no node for id -1 and returns. So the effect is that the entrance's stairs tile is not a working exit, which is the intent (the slot has no exit), and
the assert line is the only symptom. Nothing in the assert path reads past the table.

### Why not a table change

A valid `Warp0` (and `Vis0`) would make that tile a real stairs warp to level `Vis0`, which the slot must not have. Keeping -1 is right; the assert is the engine
complaining about an exit that does not exist. A code fix (steering the entrance away from a stairs piece, or hooking `0x32c980`) would change the room layout or add a hook
to silence a log line, and is not justified by a harmless assert. No change was made.

### Not settled

- Game 2 of the check had no such line. Not explained here. Candidates: the line is logged once per process by the loader (games 1 and 3 may be the only two
  sessions that began fresh; not checked), or the entrance piece was not placed there.
- The tile itself (its DS1, orientation and `k`) was not read.

### Needs game

One game on a Cave map with the plugin log and the loader log: the assert line should appear at the map build (before the arrival line) and how often per
level build; a Crypt map likewise. To pin the tile, log `0x3f4670`'s `k` and `dir` (arguments `r8 >> 20 & 0x3f`, `[rsp+0x78]`) in a probe, or step the build with `MazeProbe 1`.
If the line is absent when the entrance is not the Prev piece, the reading holds. This note does not need a fix to be closed if that holds.
