# Act 1 Catacombs (#275)

Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 34 Catacombs 1, 35 Catacombs 2 (waypoint), 36 Catacombs 3. Level 37 Catacombs 4 is a preset (Andariel) |
| DrlgType / LevelType | 1 (maze) / 10 Act 1 - Catacombs (6 DT1 files) |
| `lvlmaze` rows | Rooms 12, 14, 16 (34-36), SizeX = SizeY = 12, Merge 500 (the row for 37, Rooms 18, is unused: that level is a preset) |
| Room pieces | Def 258-272: Catacombs W..NSEW, 15 masks, 4 DS1 each. `pieceBase` 257 (257 itself is the Cathedral preset) |
| Variants | 273-287 Theme |
| Special rooms | Prev **EW 288, NS 289, NSEW 290**: passage pieces, not dead ends; 291-294 Next W/E/S/N; 295-298 Waypoint |
| Warps | 16 (to Cathedral), 17 Up, 18 Down |

## Does the Cave/Crypt path work unchanged?

Almost. The Catacombs case runs its own room grower (`0x3e8610`) before the shared walk. It only adds rooms next to the first room and places no special room, but the extra rooms break the steered walk (it needs exactly one first room). A small hook on the grower skips it on steered builds and runs it as before on a fallback (RE note 46).

Prev is not a one-exit family, so the entrance is an ordinary room (`prev.present = false`, supported). Prev, Next and Waypoint pieces are never placed.

Tileset name: `Catacombs`. Row: LevelType 10, 12-tile rooms, `pieceBase` 257, slot `MazeCatacombs`, level "Cartographer Catacombs" at (3500, 500).

## Game checks (slot 11, 2026-11-17)

Small, Medium, Large and ExtraLarge each built as planned with arrival in the entrance cell. Boss spawned in the plan's boss room (Small, Large, ExtraLarge by log; minions seen on Small) and the event in its room. Medium: all 13 rooms visited, outer rooms show N and W walls with black past S and E, no walkable void. A Cave map still builds. Not checked: a restart giving the same level, an Arcane map.
