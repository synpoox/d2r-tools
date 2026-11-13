## 29. Unique packs through the whole map (issue #317)

Map sizes here use the names from before #381: "Large" below is ExtraLarge now (Cave 12 x 12), "Medium" is Large, "Small" is Medium.

Date 2026-11-12. Consumer: Cartographer `Uniques` (`src/engine/level_row.cpp`, called from the room populator hook in `src/engine/event_apply.cpp`; pure part `src/levelrow/`, `tests/levelrow_tests.cpp`). Module `D2R.exe`, build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta. No new address and no new signature: it reuses the `PopulateRoom` hook (`0x50acf0`, section 9) and the slot level's region record (section 15).

### Why a table value cannot do it

Section 15 shows the picker `0x419d20`: below MonUMin a pack is unique with chance `+4 * 100 / +0xc` (rooms activated over rooms in the level), below MonUMax with a flat 6 percent, else not. With `Uniques` raising only MonUMax (#320) the count reaches MonUMax after about `MonUMax / 0.06` packs, so a total of 16 is spent in the first quarter of a Large map (about 2.1 packs per room, 576 rooms). Raising MonUMin front-loads instead, and no table value lengthens the flat phase.

### What the plugin does

At map open (`LevelRowBeforeOpen`) the map's total is `MonUMax` of the record scaled by `Uniques` (`UniqueTotal`), and the pace is armed (a map with `Uniques` 0, or a level MonUMax of 0, is not paced and keeps the game's own roll) for that game, slot level and record pointer. Each time the populator is called for a room of that level (the hook already exists), before the original runs, the plugin writes `MonUMin = 0` and `MonUMax = PaceBudget(total, +4, +0xc)` into the record, and after it returns it writes the two bytes back to the MonUMin / MonUMax saved once when the pace was armed (the planned values). `PaceBudget` is `total * (rooms activated) / (rooms in level)` rounded up, at least 1 and at most `total`; before the first room is populated `+0xc` is 0 and the budget is 1. The picker then makes a pack unique at the flat 6 percent only while the count at `+0x2c8` is below the budget, so the unique packs are given out as the rooms are activated. The record holds the planned values outside the call, so the open-time ledger and `CheckRecord` for a later open stay exact.

| Record field | Offset | Use |
|---|---|---|
| rooms activated | `+0x04` (int32) | read: the share of the map done (rises by one per activated room; section 15) |
| rooms in the level | `+0x0c` (int32) | read: 0 until the first room is populated |
| unique count | `+0x2c8` (byte) | read by the probe only |
| MonUMin, MonUMax | `+0x2bc`, `+0x2bd` | written around each populator call of the armed level, then put back |

Guard and failure mode: armed only when the record part `Uniques` is allowed (the record matches the row, `MonUMin` / `MonUMax` plausible) and the level's MonUMax is above 0. Each call checks the game pointer, the room's level id (`EvRoomLevelId`) and that the record is readable; any failed check or write leaves the room to the game's own roll. A failed put-back disarms the pace and logs an error. Disarmed at the top of every open, on a failed open (also `opened == 0` in `portal.cpp`), when the record write of the open fails, on leaving the game and on unload. When the populator hook is not installed (a part not found) the pace does nothing and the #320 behaviour (MonUMax raised once, MonUMin unchanged) stays. `RecSpan` grew to `0x2d0` so the unique count is inside the checked span.

### Evidence (live, slot 5, Hell, test save, god mode, `MazeProbe=1`)

Cave Sprawl Large, Level 90, Density 100, 576 rooms (`+0xc`). Each run opened the map once and visited every 24-tile cell of the plan by `cheat goto 138,x,y` nearest the entrance first, so rooms are probed in a breadth-first order; counts come from the `record probe:` log lines (unique count delta per populated room, with the room's rect), not from the automap.

| Run | Uniques | Total | Uniques made | By quarter of the rooms probed (entrance to far end) | First, last room |
|---|---|---|---|---|---|
| 1 | 100 | 16 | 17 (575 rooms probed) | 4, 4, 4, 5 | room 3, room 544 |
| 2 | 400 | 40 | 38 (512 rooms probed) | 11, 9, 8, 10 | room 27, room 487 |

The same quarters by distance from the entrance (x + y of the room rect) were 5, 3, 4, 5 (run 1) and 11, 9, 8, 10 (run 2). The total follows `Uniques` (16 against 40), and the count tracks the budget through the whole map. Single game assumption: the pace state is one global (game pointer, level, record) touched only from the game thread; two games in one process are not supported. Not run: the same map without the pace (the #320 build); the section 15 runs of that build and the formula above are the comparison. Small overshoot is expected: the game reads the count after a pack's units exist, so several packs of one room can be unique together (one room made 4 in run 1). The ~73 ceiling below was measured on the #320 build, without the pace. Not seen: a character walking the map (a click-walk attempt reached only 11 rooms; whether `+4` counts distinct rooms or repeated activations, and so whether uniques could bunch near the entrance when walking back and forth, is unverified), a second layout per setting, other densities, multiplayer, a total above the 6 percent ceiling (about 73 on this map).

### Lava filler rooms (issue #385)

The Lava edge fill (section 23) adds closed filler rooms that are in the level's room list, so `+0xc` counts them, but they have no ground and are not expected to activate, so `+4` never reaches `+0xc` and `PaceBudget` would stop short of the total (static reasoning, not measured; the issue estimated about two thirds of a Lava level's rooms are filler, the share depends on the layout). The maze build hook records how many fillers it made (`MazeFillerRooms`) and `PaceBudget(total, seen, rooms, fillers)` divides by `rooms - fillers` (ignored when that would leave nothing). Maps without a filler ring (Cave, Crypt, Ice, outdoor) pass 0 and are unchanged. Not seen in game: whether the fillers really never activate; if they do, `+4` reaches `+0xc` and the subtraction would front-load the budget slightly (still capped at the total).
