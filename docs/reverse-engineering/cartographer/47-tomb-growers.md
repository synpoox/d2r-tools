## 47. The Act 2 Tombs growers (issue #242)

Build 93847 (D2R 3.3, image stamp 0x6ab3782c, D2RLoader 1.3.1). Addresses are RVAs of that build and are found by signature at run time, never by address. Evidence: **static** (disassembly of the memory dump); the in-game result is in the PR.

### 1. The case

The maze builder `0x3e69e0` dispatches on `LevelType - 3` through the jump table at `0x3e76d8`. LevelType 17 (Act 2 Tomb) is index 14, entry `0x3e6da7`. In order:

1. If the level id (`level+0x1f8`) is 61 (Tomb 3 Treasure) it takes a special path of its own (a slot id never matches).
2. `0x3ea0e0(level)`: the **pre-walk grower**.
3. The walk `0x3e96d0`.
4. `0x3eb610(level)`: the **special-rooms pass**.
5. The tail `0x3e75bc`.

The ring grower `0x3e9820` is never called, so the row has `callsRingGrower` false and no filler ring.

### 2. The pre-walk grower `0x3ea0e0`

It has no level-id test. It adds three rooms round the first room (three add-room calls `0x3e77a0`, the piece picked by `0x3eb960`) and sets the first room's piece to a Prev piece (444 to 447, the three-exit pieces that hold the Up stairs). That leaves four rooms before the walk. The steered walk (`SteeredWalk`) needs exactly one, so the grower must not run on a slot build. It also writes the Prev piece into the first room directly (`0x3ea18c` to `0x3ea1c4`, not through `0x3eb850`), so on an unsteered or fallen-back slot build it would put the Up stairs into the map: it is skipped on every slot-level build.

### 3. The special-rooms pass `0x3eb610`

It looks for rooms by level id (the Tal Rasha tombs, the treasure tombs, the waypoint tomb); none match a slot level. When no room has a piece above 428 it takes an assert path (`0x3e67c0`, then `int3` when that returns non-zero). The assert fires when the first room above 428 (the last room, when none is above 428) is not one of 444 to 447 (`0x3eb661`, `int3` at `0x3eb66f`). A slot build never has the Prev piece (the pre-walk grower is skipped), so this pass would assert: it is skipped too, on every slot-level build, steered, unsteered or after a fallback.

### 4. The room-count multiplier

`0x3e96d0` multiplies the room count by 3 for the level id equal to `drlg+0x120` (the act's tomb chosen at start-up) and by 2 for `drlg+0x874`. A slot id matches neither, and a steered build replaces the walk, so the multiplier never applies.

### 5. The hooks

| Item | Value |
|---|---|
| Signature `MazeTombPreGrower` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 8B 81 E4 01 00 00 4C 8B F1 48 8B 69 10 BE 03 00 00 00`, unique, hook cut 15 bytes, max patch 107 (`check_maze_signatures.py`) |
| Signature `MazeTombSpecialRooms` | `48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B 41 10 33 D2 89 54 24 38 48 8B F9 8B DA 48 85 C0 74`, unique, hook cut 15 bytes, max patch 33 |

`src/engine/tomb_growers.cpp` hooks both. Each calls the original unless a slot build is running on this thread (`MazeBuildSkipsTombGrower`: both are skipped on every slot-level build, as for Catacombs). Any other level runs the game's own growers. The pass after the walk only works when the first was skipped, so if either signature is missing both stay off and a Tombs map is refused with a console and log message (`GrowerMissing`), as for Arcane.

### 6. Pieces

Def 414 to 428 are the 15 exit masks (W 1, E 2, S 4, N 8), `pieceBase` 413. Piece 413 is no closed room, so there is no filler ring (`ringCells` 0). The Prev pieces (444 to 447), Next, Treasure, Cube, Tal Rasha, Leatherarm, Kaa, Chest, Waypoint and Tainted Sun (to 480) are never chosen by a plan.

### 7. Offset

x 4000, y 500: the last spot of the y-500 band (the range allows x up to 4235 at y 500), clear of the Catacombs (3500), of the other maze rows, and of every vanilla Act 5 rectangle (all start at y 1000 or higher).
