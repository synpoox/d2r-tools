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

### Lava filler rooms (issue #385): measured, no shortfall

Question (#385): the Lava edge fill (section 23) adds closed filler rooms to the level's room list; if they never activate, `+4` would never reach `+0xc` and `PaceBudget` would stop short of the total. Date 2026-11-13, same build and loader as above, slot 7, Hell, test save, `debug_mode` and `cheat god 1` first, `MazeProbe=1`. Map `LavaXL` (Lava, Sprawl, ExtraLarge, Level 90, Density 100, Uniques 100, total 14), opened with `cartographer open`, then every 24-tile cell of the 13 x 14 grid visited by `cheat goto`, nearest the entrance first (cells without ground leave the player where he was). Not walked by a character.

| Number | Value |
|---|---|
| plan rooms (cells) | 55 (cells are 24 x 24 tiles, 9 rooms of 8 x 8 each) |
| filler rooms made | 96 cells (log line `maze filler ring: 96 of 96 rooms made`) |
| rooms in the level list | 1359 = 495 + 96 x 9 |
| `+0xc` (rooms in level) | 495 from the first populated room to the last = 55 x 9: the plan rooms only |
| rooms populated (probe lines, all distinct rects) | 590 |
| `+4` at the end | 591 (the probe's room number plus 1 on every line) |
| `+8` at the end | 397 |
| unique count at the end | 15 (total 14) |

So the premise is wrong twice. `+0xc` does not hold the fillers (495 is the plan alone), and `+4` counts every room the game populates, 95 more than the plan has rooms: the rooms beside a visited plan room activate with it, the fillers among them. `+4` passed `+0xc` at the 494th populated room (84 percent of the 590), so the unpatched budget `total * +4 / +0xc` reaches the total before the last rooms and nothing falls short. The first version of this fix (`PaceBudget` dividing by `rooms - fillers`, 495 - 96 = 399 here) subtracted fillers that were never in the count and reached the total at the 375th probed room (about 64 percent), front-loading the packs; the run above was made on that build. It was reverted, and the section "Spread check (issue #317)" below supersedes that line for Lava maps: fillers are now left out of `+4` (the numerator), which is not the reverted change (it took them out of the denominator, `+0xc`, where they never were). Not measured: the same map on the unpatched build (the budget column of the log gives it by formula: 14 * `+4` / 495), a second layout or size, a character walking the map. `+4` also overruns `+0xc` here, so on a map of this kind the share is capped at the total near the last sixth of the rooms.

### Spread check (issue #317): Cave and Snow fine; Lava filler rooms left out of the pace

2026-11-13, slot 11, Hell, test save, god mode, `MazeProbe=1`, every room visited by `cheat goto` (not walked), rooms in activation order. Maps Level 90, Density 100, Uniques 100. Cave: uniques by thirds 28 / 44 / 28 percent; Snow 36 / 32 / 32 percent (build of origin/main). Crypt and Jungle not measured.

**Measurement trap (Lava).** The log's `planned grid` lists the plan cells only. The filler ring is one cell wide on every side, so a room at level cell (cx, cy) (`(x - 2500) / 24`, `y / 24`) is plan cell (cx - 1, cy - 1). Sweeping and classifying by the unshifted grid visited the wrong cells (only 230 to 298 of the 567 plan rooms were populated) and called plan rooms fillers. The first report of this issue ("7 of 13 uniques in filler rooms", plan rooms 5 / 1 / 0) came from that error and is withdrawn: reclassified with the shift, the origin/main Lava run had 13 uniques, 0 in fillers (partial sweep, 230 plan rooms: 4 / 1 / 8 by thirds), and the first build of this fix had 8 uniques, 0 in fillers. Fits: the plugin's per-room filler flag (logged as `record probe: pace begin: filler N`) agreed with the shifted grid on 698 of 698 rooms (run 2) and 827 of 827 (run 3), so the mask is right, and `+0xc` is the plan rooms only (504 to 585 here).

**Why the fix still stands.** Fillers are in `+4` but not in `+0xc` (above), so they advance the unfixed pace without being map progress. `RoomBudget` (levelrow) gives a filler room the count made so far (no unique there) and paces plan rooms on `+4` minus the fillers seen. That is consistent with the measurement of #402: the reverted change divided by a smaller room total, this one takes the fillers out of the numerator only; and #402's "nothing falls short" held because the fillers advanced the pace, a count of the total, not of where packs landed. Without the fillers the budget reaches `rooms` only at the last plan room, so a plan room that rolls no pack leaves a shortfall (run 2: 8 of 14). So once a filler has been seen, the total is due at 80 percent of `+0xc` (the slack the old pace had by accident, `+4` passing `+0xc` at about 84 percent). Detection needs the arrival mark (`MarkArrival`, a level built as planned); a fallback or partly game-built Lava level keeps the old pace.

**Run 3 (this build, full sweep).** Total 14, 827 rooms populated (503 plan, 324 filler by the grid, all flagged filler by the plugin), 15 uniques made (the game overshoots by whole packs: one room made 4), 0 in filler rooms. Plan rooms by thirds in activation order: 9 / 2 / 4 uniques; as pack events (a room's burst counted once) at plan-room numbers 54, 80, 247, 425, 463, 533 of 827 rooms, so packs appear from the first rooms to the last 35 percent. The budget at those rooms equalled the count made plus one, so the gate is MonUMax; no preset or superunique spill-over showed in these lines. The earlier "3 uniques in filler rooms" were the classification error above.

Not measured: origin/main on the same full sweep, a second seed, Crypt, Jungle, a walking character.
