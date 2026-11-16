## 37. The Arcane Sanctuary room grower (issue #280)

Build 93847 (D2R 3.3, image stamp 0x6ab3782c, D2RLoader 1.3.1). Addresses are RVAs of that build and are found by signature at run time, never by address. Evidence: **static** (disassembly of the memory dump with `plugins/cartographer/tools/dis`), **needs game** where marked.

### 1. Where it is

The maze builder `0x3e69e0` dispatches on `LevelType - 3` through the jump table at `0x3e76d8` (33 dwords). LevelType 19 (Arcane Sanctuary, level 74 in vanilla) goes to `0x3e6e7e`. That case calls neither the walk `0x3e96d0` nor the ring grower `0x3e9820`: it calls its own grower `0x3e7a80(level)` (**static**).

The grower lays a fixed spiral of about 64 rooms through add-room `0x3e77a0` and the picker `0x3eb960`, drawing one random start rotation (`rng & 3`, at `0x3e7ab1..0x3e7aea`) and adding every room in a direction derived from it (`(rot + 1) & 3`, `rot & 3`, and so on; **static**). After the spiral, a loop at `0x3e7edb..0x3e8123` walks the stack room array `[rsp + 0x40]` (it does not hold the first room) and, for each non-null room `i`, writes the file variant `(i / 15 + rot) & 3` (`0x88888889`, `shr 3`; `rot` is the start `rng & 3`) into `[[room + 0x40] + 4]` (e.g. `0x3e7f1e`/`0x3e7f22`; **static**). The steered build writes the same value for plan room `k` with `i = k - 1`, and draws `rot` from the level seed the way the game does (`arcaneDrawRotation`), so the variant mix matches a vanilla Arcane map and the seed advances as in the game. **Not written to one-exit pieces (510, 511, 513, 517)**: the game only ever makes those the Summoner rooms (the special-room table above), and a piece with fewer DS1 files than the variant crashed the room load in game (read of bad memory at `0x3e1540..0x3e156d`, ArcM with piece 517 and variant 3, and an earlier Large build); they keep the engine's own pick (file -1). It returns to `0x3e860e`. After it, the case makes one special-room call to `0x3eb850` with the table at `0x1d0f730`: (517 to 528, 3), (511 to 526, 0), (513 to 527, 1), (510 to 525, 2), the four Summoner pieces. Then the common tail `0x3e75bc` runs.

The picker for LevelType 19 adds 509, so a room's piece is 509 + exit mask (W 1, E 2, S 4, N 8) = 510 to 524. Piece 509 is a Lair row, so there is no closed filler piece. The Summoner pieces 525 to 528 are placed only by the special-room call.

### 2. The hook

| Item | Value |
|---|---|
| Signature (`MazeArcaneGrower`) | `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 81 EC 30 02 00 00`, 31 bytes, unique, up to the first RIP-relative load |
| Hook cut | 15 bytes (whole instructions) |
| Max patch (`check_maze_signatures.py`) | 999: byte 31 is a RIP-relative load, not a branch, so the branch check does not apply |

`ArcaneGrowerHook` runs the same steered step as the walk hook (`SteeredStep`): on a steered build it replaces the grower with the plan's adds (`0x3e77a0`), links (`0x360bb0`) and the picker on every room; unsteered, or after a fallback with no plan room made, it calls the original grower, so a vanilla Arcane Sanctuary still builds. After a refusal that follows some plan rooms, the original is **not** called: the spiral passes the result of each add-room as the next parent without a null check (`0x3e7b2b` tests it, but `0x3e7b5e` passes it on; add-room reads `[rcx + 0x90]` at `0x3e77b2`), so a refused add would crash. The hook keeps the rooms made, runs the picker on them and logs a warning (`mayRunOriginalStep`). The special-room hook already suppresses the Summoner pieces: the row has no Prev family, so `answerSpecial` has nothing to place.

Failure mode: if the signature is not found or the hook does not install, the `Arcane` row is refused with a console and log message (`MazeTilesetProblem`, and in `LevelBuildHook` for a map already open); nothing is shown on screen. Other tilesets are unaffected.

### 3. Edges

No filler ring (`ringCells` 0): no closed piece exists, and the dark space around the platforms is the tileset's own look. Edges of a room are open on the sides that the piece's exit mask leaves open and the plan only puts exits where a neighbour exists. **needs game**: the check on each Size is recorded in the pull request.

### 4. Installer

Level 74 is the only level of LevelType 19 and a quest level, so `tools/install.ps1` copies it by id (`Template = 74`), clears `Quest`, `QuestFlag` and `QuestFlagEx`, and writes the `lvlmaze.txt` row with 12-tile rooms. Offset x 500, y 0 (the Durance row takes x 1000): in automap range (int16 cells), clear of Snow (1500 to 1692) and above the first Act 5 level (y 1000).
