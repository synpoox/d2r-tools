# Act 2 Sewers (#243)

Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 47 Sewer 1 A, 48 Sewer 1 B (waypoint), 49 Sewer 1 C, 65 Sewer 2 A |
| DrlgType / LevelType | 1 (maze) / 13 Act 2 - Sewer (4 DT1 files). Not the Act 3 sewer (LevelType 25) |
| `lvlmaze` rows | Rooms 6, 12, 14 (47-49), 10 (65); SizeX = SizeY = 12, Merge 500 |
| Room pieces | Def 302-316: Sewer W..NSEW, 15 masks, 2 DS1 each (`pieceBase` 301, which is the Act 2 Town preset) |
| Variants | 317-331 Theme |
| Special rooms | 332-335 Prev W/E/S/N (dead ends) and 336 Prev NS (dock); 337-340 Next; 341-344 Radament's Lair; 345-348 Waypoint; 349-352 Chest |
| Warps | 21 (Dock to Town), 22 Up, 23 Down |

## Built (#243)

Built as the maze tileset `Sewer` (slot `MazeSewer`, level "Cartographer Sewer", offset x 3000, y 500). The research note's "own grower" was wrong: the case runs the ring grower and the shared walk, and `0x3eab30` after them only places special rooms for the game's own levels 47 to 49 and 65, so a slot level needs no grower hook (`docs/reverse-engineering/cartographer/45-act2-sewer-case.md`). The Act 3 tileset is `KurastSewer`. No filler ring: piece 301 is the Act 2 Town preset, not a closed piece, and no Prev, Next, Radament, Waypoint or Chest piece is ever placed (#455).

## Game checks

Arrival, Boss, event and the four outer edges per Size are recorded in the pull request of #243.
