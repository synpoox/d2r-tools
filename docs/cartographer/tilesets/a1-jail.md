# Act 1 Jail (#273)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 29 Jail 1 (waypoint), 30 Jail 2, 31 Jail 3 |
| DrlgType / LevelType | 1 (maze) / 8 Act 1 - Jail (7 DT1 files) |
| `lvlmaze` rows | Rooms 12, 14, 16 (all difficulties), SizeX = SizeY = 12, Merge 500 |
| Room pieces | Def 206-220: Jail W..NSEW, all 15 masks, 4 DS1 each (5 for NSEW) |
| Variants | 221-235 Theme (+15 swap) |
| Special rooms | 236-239 **Prev W/E/S/N** (dead ends, stairs up), 240-243 Next, 244-247 Cath (Cathedral link), 248-251 Waypoint, 252-255 Pitspawn |
| Warps | 13 (Jail Up), 14 (Jail Down) |

## Does the Cave/Crypt path work unchanged?

Nearly: the closest match to Crypt. The Prev family has the Cave and Crypt shape (four one-exit pieces: `prev = {236, 237, 238, 239}`, `pieceBase` 205, `roomTiles` 12). Beyond the catalogue row:

1. **Ring grower.** The Jail case calls `0x3e9820(level, 2)` before its walk. Set `callsRingGrower` (the `RingHook` exists); `walkFollowsRing` stays true.
2. The Prev table group in the Jail case must hold entries from Jail W/E/S/N to Prev W/E/S/N for `answerSpecial`. The Cave and Crypt tables were dumped; the Jail one was not.
3. The installer template is the first plain Jail maze row (level 29, which has a waypoint); the installer already writes Waypoint 255, Quest 0, and Vis and Warp off.

## Risk, effort

Risk **low-medium**: 12 is not a multiple of the 8-tile engine rooms, so a cell may build as an 8 + 4 split, and `planCellRoom` needs the built box to equal plan width x 12. Effort **S-M**: catalogue row, installer value, ready list, tests, docs, plus the ring check. The best first tileset after Cave and Crypt.

## Game checks

- Ring no-op leaves exactly the planned rooms; the Prev call is redirected and `rot` stays 0..3.
- Built-room box equals the plan box at 12 tiles per cell; arrival lands in the entrance cell.
- Boss and event room choice works when a cell is 8 + 4 sub-rooms.
- No Pitspawn, Cath, Waypoint or Next piece appears in a built level.
