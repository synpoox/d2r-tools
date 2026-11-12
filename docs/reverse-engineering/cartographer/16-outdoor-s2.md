## 16. Outdoor S2 spike results (issue #89)

The Snow (Barricade) tileset pieces, sockets and plan grammar (issue #147, static evidence only) are in [cartographer-snow.md](../cartographer-snow.md).

Spike: `plugins/cartographer-outdoor-spike/` on `spike/cartographer-outdoor-s2` (never merged). Build 93847, slot 5 test save, level 138 (Act 5, DrlgType 3, LevelType 21), layout `*E,EWS,SW/-,*N,NS/-,-,*N` (3 x 3 blocks of 32 x 32 tiles: an L plus a branch; `*` is a Pygmy clearing; level rect `6000,3000 96x96`). Status per item: **live** = seen in a running game, **static** = from code or a log of the build, not walked.

| Item | Result |
|---|---|
| Layout stamps and the level builds | **Live.** 6 pieces stamped, 96 rooms for 6 blocks (16 rooms of 8 x 8 per block); `0x3ee780` made no rooms for the 3 void blocks. |
| Void edge (`0x3ede00`, grid `+0x58` = `0x100`, no room) | **Live, acceptable.** A `cheat goto` onto a void block does not place the player there: the game falls back to the entry (tiles 19,18). A click-walk from the entry toward the void block below it stopped at tile y 24 inside the entry clearing (the block begins at y 32). The screenshots show dark rock, not a hole. Fill or a border piece is **not needed for play**; the automap edge was not checked. |
| Edges walk as predicted | **Live for 4 of 5.** Walked: (0,0) to (1,0) (tile x 19 to 34); (1,0) to (1,1) (tile y 17 to 43); (1,0) to (2,0) (x 59 to 68, past 64); (2,0) to (2,1) (y 17 to 42, x 78). **Static only: (2,1) to (2,2):** `cheat goto` reaches the block centre (80,80) with the player on open ground, but the click-walk from (2,1) stopped at (78,42), so no crossing was walked. The client loads collision only a few rooms around the player (a whole-level dump shows 95 to 100 % blocked), so a walker works from the position, not from a whole-map dump. |
| Portal lands on the entry cell | **Live.** `OpenRedPortal(game, player, key, 138, 1)` returned 1 and the portal opened in town; entering it logged `ARRIVAL ... subtile (30098,15082) tile (6019,3016) level-relative (19,16) = cell (0,0): IN THE ENTRY CELL`. The finder (`0x3da650`) took the room the spike marked with `0x30000` (tile 16,16 of the entry cell). Not tested without the mark (`arrival = "other"`). |
| Boss cell is a clearing | **Live.** The `*N` cell (1,1) shows the open Pygmy clearing with monsters in it (a Demon Rascal is named on screen); the player stood in it. The monster and object rows were those cloned from Act 5 row 111, so the layout alone does not decide what fills a clearing. |

### Findings

- `Vis0` to `Vis7` = 0 is recorded in section 8a (slot row requirements); the crash with `-1` was reproduced here.
- The portal builds the level on the server inside the `OpenRedPortal` call; the client builds it again on arrival. The stamper and the void marking therefore run twice per entry, once per side, and both runs must give the same grid.
- `OpenRedPortal`'s last argument is `1`, as the maps plugin passes it.
- `core-maps.dll` was out of the test slot's plugins, so the spike's row 138 and hooks were the only users of the level build.
- D2RCore needs `data\global\excel\d2rloader\core-ui\automap.txt` in the mod folder; without it the game asserts at start and faults.

### What the outdoor builder still needs before players get outdoor maps

1. A position-driven walk test over the whole layout (this spike crossed four of five edges by guided pathing), and a check that every `*` clearing joins its path piece in every orientation the generator can produce.
2. The slot row as engine contract: `Vis` `0`, one row per slot with the offset and size the layout needs (section 8 Q4), and one shared hook on `0x3271c0` (section 3).
3. A server-to-client consistency rule: both sides stamp from a seed or layout sent with the portal, never from independent random picks (section 12).
4. Monster and object population per clearing, boss placement in the clearing, and an exit or waypoint to leave the map (the spike had none; the way out was the town portal).
5. Automap and minimap edge behaviour, a border piece or fill if the automap shows the void, and the arrival choice made by the layout (the entry cell) instead of a spike-side room flag.
6. The real player flow: the map item and portal tooltip path (sections 3 and 10) driving this slot, multiplayer sync (section 12) and a sweep over layouts and LevelTypes; none of that was run in this spike.

Non-rectangular outlines for the open-field families (issue #148, static only): [cartographer-outline.md](../cartographer-outline.md).
