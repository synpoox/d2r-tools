# Act 2 Maggot Lair (#279)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 62 Lair 1 A, 63 Lair 1 B, 64 Lair 1 Treasure (the Maggot Lair levels) |
| DrlgType / LevelType | 1 (maze) / 18 Act 2 - Lair (3 DT1 files) |
| `lvlmaze` rows | Rooms 18, 14, 10; SizeX = SizeY = **10**, Merge 500 |
| Room pieces | Def 482-496: Lair W..NSEW, 15 masks, 1-6 DS1 each (`pieceBase` 481, which is the Duriel's Lair preset) |
| Variants | **none**: no Theme family. The "+15" swap of 482-496 lands on 497-511, which are the Prev, Next, Treasure and Tight Spot rows |
| Special rooms | 497-500 **Prev W/E/S/N** (dead ends), 501-504 Next, 505-508 Treasure, 509 Tight Spot S (the Maggot Queen's room) |
| Warps | 48 Up, 49 Down |

## Does the Cave/Crypt path work unchanged?

Almost: Prev is a one-exit family like Cave and Crypt (`prev = {497, 498, 499, 500}`, `pieceBase` 481, `roomTiles` 10). The Lair case lays a 2 x 2 ring first, then walks, so `callsRingGrower = true` (the hook exists). Open points:

1. **The "+15" swap** (`0x3ebd90`: `max(2, rooms/5+1)` rooms become the same-exit Theme piece). The Lair has no Theme family, so the engine must skip it for this type; if it does not, plan rooms turn into Prev, Next or Treasure pieces. Needs a static read, then a game check.
2. 10-tile cells are not a multiple of the 8-tile engine rooms; the built box may not equal plan width x 10 (arrival, Boss and event then refuse).
3. The installer template is the first plain Lair maze row, level 62.

## Risk, effort

Risk **medium** (swap behaviour, 10-tile box). Effort **M**: catalogue row, installer value, ready list, tests, docs, the ring and swap checks.

## Game checks

- Ring no-op leaves exactly the planned rooms; no Prev, Next, Treasure or Tight Spot room appears except the entrance.
- Built box equals the plan box at 10 tiles per cell; arrival, Boss, event.
