# Act 1 Catacombs (#275)

Research only, nothing run in a game. Sources as in `a1-barracks.md`.

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

No. The Catacombs case runs **its own room grower (`0x3e8610`)** instead of the shared walk `0x3e96d0`. Cartographer replaces the walk, so for this tileset the plan never reaches the engine. The grower must be hooked (graph built inside it, as `Plan::ring.graphInRing` does for ring tilesets) after a spike on what it calls. The research marks Catacombs "not yet steerable".

Prev is not a one-exit family, so the entrance is an ordinary room (`prev.present = false`, supported).

Tileset name: `Catacombs` (not yet in `TilesetNames`).

## Risk, effort

Risk **high** (unstudied grower; it may place the waypoint and Next rooms itself). Effort **L**: a spike on `0x3e8610` (signature, callees, whether it ends in `0x3eb850`), a hook with a fallback, then the Jail-sized catalogue and docs work.

## Game checks

- The grower replacement builds exactly the planned rooms; no waypoint or Next room appears unplanned.
- Arrival, Boss and event; 12-tile cell box check.
- A second game after a restart builds the same level (cached plan).
