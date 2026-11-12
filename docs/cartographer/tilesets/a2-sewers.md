# Act 2 Sewers (#243)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

| | |
|---|---|
| Vanilla levels | 47 Sewer 1 A, 48 Sewer 1 B (waypoint), 49 Sewer 1 C, 65 Sewer 2 A |
| DrlgType / LevelType | 1 (maze) / 13 Act 2 - Sewer (4 DT1 files). Not the Act 3 sewer (LevelType 25) |
| `lvlmaze` rows | Rooms 6, 12, 14 (47-49), 10 (65); SizeX = SizeY = 12, Merge 500 |
| Room pieces | Def 302-316: Sewer W..NSEW, 15 masks, 2 DS1 each (`pieceBase` 301, which is the Act 2 Town preset) |
| Variants | 317-331 Theme |
| Special rooms | 332-335 Prev W/E/S/N (dead ends) and 336 Prev NS (dock); 337-340 Next; 341-344 Radament's Lair; 345-348 Waypoint; 349-352 Chest |
| Warps | 21 (Dock to Town), 22 Up, 23 Down |

## Does the Cave/Crypt path work unchanged?

No. The Act 2 sewer case runs **its own grower (`0x3eab30`)** instead of the shared walk. The one-exit Prev family fits the model (`prev = {332, 333, 334, 335}`), so once the grower is hooked the rest is the Jail recipe. The research marks the Act 2 sewer "not yet steerable".

Naming: the known tileset name `Sewer` is Act 3 (LevelType 25 in `tilesets.tsv`); the Act 2 one is `Act 2 Sewer` in the catalogue. Pick the user-facing name before adding the `TilesetNames` entry. The Radament piece (quest, level-id test) is unreachable from a slot.

## Risk, effort

Risk **medium-high** (unstudied grower). Effort **L**: grower spike and hook, name, installer value, tests, docs.

## Game checks

- The grower hook builds exactly the planned rooms; no Radament or Waypoint room appears unplanned.
- Prev redirect, arrival, Boss, event at 12 tiles per cell.
