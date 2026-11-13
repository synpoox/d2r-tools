## 15. Map monster columns in the slot level's row and region record (issue #156)

Build 93847, stamp `0x6ab3782c`, static (dump); nothing here was run in a game. Code: `plugins/cartographer/src/engine/level_row.cpp` (game glue), `src/levelrow/` (pure: layout, guards, plan, ledger, undo; `tests/levelrow_tests.cpp`). The first route wrote every column into the levels row only; the review showed the spawn code reads a per-game copy, so the route is now two spaces: the row for `Level`, the slot level's region record for `Density`, `Uniques` and `Monsters`.

### The row (`Level` only)

The levels.txt loader (`0x32eac0`..`0x33061a`) builds one descriptor per column, `0x20` bytes each (`+0` name, `+8` type, `+0x10` row offset). `Id` is at `+0x04` (2 bytes); `MonLvl`, `MonLvl(N)`, `MonLvl(H)` at `0x18`, `0x1a`, `0x1c` and `MonLvlEx`, `MonLvlEx(N)`, `MonLvlEx(H)` at `0x1e`, `0x20`, `0x22` (2 bytes each). `Level` writes these six. It works because `0x32c700` reads the row's `MonLvl` live on each call (static). The scan of `.text` for displacements `0x2dc` and `0x2e0` shows the record's own MonLvl copy is only written at `0x508973` / `0x508987`, which is why the spawn-time level comes from the row.

### The region record (`Density`, `Uniques`, `Monsters`)

The record array is built once per game: `0x5098b0` (called from nowhere else) calls `0x508750` for every level `1..count-1` and stores the records in the array at `*(game + 0x1d0)`; `0x508740(array, level)` returns `array[level]` (also used at `0x508d5f` and `0x50ad3a`). A record is `0x9f0` bytes.

| Field | Record offset | Size | Evidence |
|---|---|---|---|
| copy of row `+0x0d` | `+0x00` | 1 | `0x508750` copies it; the plugin compares it with the row (the difficulty check) |
| group count | `+0x10` | 1 | `0x509c24`..`0x509c43` (`0x509940`) |
| group weight sum | `+0x11` | 1 | same; sum of the byte weights |
| group count 2 | `+0x12` | 1 | same |
| groups | `+0x14 + 0x34*i` | `0x34` each | word monstats row at +0, byte weight (monstats `+0x64`) at +2, byte at +3 (member count by `0x4195e0`, inferred); 13 groups end exactly at `+0x2b8` |
| `MonDen` | `+0x2b8` | 4 | write `0x508921` |
| `MonUMin` | `+0x2bc` | 1 | write `0x50892d`, read `0x419d47` |
| `MonUMax` | `+0x2bd` | 1 | write `0x508939`, read `0x419d3f` |

Readers: `0x419e80` picks the monster kind from `+0x10`, `+0x11` and `+0x14`; `0x419d20` reads `+0x2bc` / `+0x2bd`; `0x50acf0` reads `+0x2b8`. In Normal difficulty only, `0x419e80` also reads the row's umon list (`+0x3d`); maps open in Hell only, so that read is not reached.

### What is written

| Map column | Where | Value |
|---|---|---|
| `Level` | row, six `MonLvl*` fields | the level, 1 to 255 |
| `Density` | record `+0x2b8` | the record's value plus `Density` percent, rounded half up |
| `Uniques` | record `+0x2bd` only | MonUMax plus `Uniques` percent, at most 255; MonUMin (`+0x2bc`) is left alone (issue #317, below) |
| `Monsters` | record `+0x10`, `+0x11`, `+0x12` and the first groups | counts = the number of ids, per entry the monstats row, its weight (monstats `+0x64`, from the SDK view) and 0 members; the sum is stored as a byte |

`Champions` has no field: the champion roll is fixed in `0x419d20` (`0x419e12`..`0x419e2c`), so it is read and checked but not applied. This replaces the earlier sentence that one pair served all special packs and was averaged; `Uniques` now scales the pair alone. Written at map open, on the game thread, just before `OpenRedPortal`.

### Why only MonUMax (issue #317, static; build 93847)

`0x419d20(region, room)` picks the pack kind for a hit cell. Unique count `+0x2c8` (added once per non-minion unique unit in `0x49edb0` / `0x49eed0`): below MonUMin and with `+0xc` (room count of the level, set at the first populated room by `0x2efa20` -> `0x3269d0`) nonzero, a pack is unique when `rand % 100 < (+4 * 100) / +0xc`; else below MonUMax, `rand % 100 < 6` makes it unique; else champion or normal. Nothing in the dump writes region `+4` (no inc/add on it in `0x400000`..`0x600000`; `+8` is incremented at `0x50b161`), so it is either 0, or kept by D2RCore (the `0x3e2b44e` / `0x3e2b5c2` thunks go to the external DLL, not read). If it counts rooms done, the chance is the share of the level already done and a raised MonUMin is met early only if `+0xc` is small or `+4` is ahead of it; if it is stale-high the whole minimum is spent at the first rooms. So the plugin raises MonUMax only (flat 6 percent per pack, spread by pack count) and leaves MonUMin. **Inference, not seen in game:** the entrance bunching of the owner's report comes from the raised minimum. Needs game: log region `+4`, `+8`, `+0xc`, `+0x2c8` per populated room on a Uniques map, and the room of each unique pack.

### Guards (runtime, per part, fail open)

- Slot guard: nothing is written when the slot level is 137 or less, or is not the checked slot of the map's own tileset (Maze) (`SlotAllowed`). Local, independent of `ValidateSlots`.
- Row: readable, `Id` equals the slot level; `Level` off when a `MonLvl*` field is outside 0..255.
- Record: pointer from `game + 0x1d0`, the level below the levels count (64-bit at `tables + 0x1468`, the bound `0x508750` loops to; the array holds levels 1 up to count - 1) and `array[level]` readable (`Readable`) for `0x2c0` bytes; `record+0` equals row `+0x0d` for the game's difficulty (`game + 0x104`); `Monsters` off when the group counts are above 13 or inconsistent, `Density` off when `MonDen` is outside 0..100000, `Uniques` off when `MonUMin` / `MonUMax` is above 100.
- Monsters list: refused with a log line (the slot keeps its own list) when it has more than 10 entries, a weight outside 0..255, weights that are all 0 (the game's weighted pick would find none) or a sum above 255 (the record keeps the sum in a byte, `0x509940` adds the weights into `record + 0x11`). A monster whose monstats row has bit 0 clear at `+0x3c` is left out, as `0x509940` does (`test` against the byte at `0x1d996d0`, which is `1`, at `0x509940` line `(byte)DAT_141d996d0 & *(byte *)(row + 0x3c)`, the same test as `0x493280`); when none is kept the list is refused. Static only: the name of monstats `+0x3c` bit 0 is not known.
- A part whose check fails is switched off with a log line; the others still apply. A write that fails stops the open and the undo restores what was written.

### Restore

The row is restored on `GameLeft` and on unload, and on a failed open (`Undo`), writing a field back only while it still holds the plugin's value. The record is undone on a failed open but only forgotten (ledger cleared, no write) on `GameLeft` and unload: the array belongs to the game that is being destroyed or is gone, so a write there is not safe. A new game builds fresh records.

### Risks, and what needs the game

- Needs game: that `Density`, `Uniques` and `Monsters` written to the record change the spawns (static reading says so, nothing was observed), and that `Level` shows in monster levels.
- Inference: entry `+3` is a member count (`0x4195e0`) and 0 is a safe value; the plugin writes 0.
- Normal difficulty also reads the row's umon list; not reached, as maps open in Hell only (#159).
- The level is bounded by the levels count read at `tables + 0x1468` through the data tables base; when that cannot be read the record parts are off.
- One record per slot: a second map through the same slot in one game overwrites the first map's values for rooms that populate later.
- Client side: a TCP/IP client writes none of this; the host spawns the monsters.

### Game check (listed for later)

A map with `Level` 60 and `Monsters` `quillrat1,zombie1`; `cartographer open`; the log shows the row and record writes; read a monster's level and kinds. Then `Density` 100 and `Uniques` 100 on a slot with a nonzero base and count packs. Leave the game and check the log reports the row put back.
