## 34. Pieces that carry a waypoint or an area link (issue #455)

Build 93847. Desk work on the game's own DS1 files (read with the repo's readers, nothing committed from the game). Evidence labels: **static** (file or table read), **inference**, **needs game** (not yet seen live).

### 1. How the list was found

`plugins/cartographer/tools/d2r_readers.py: ds1_links(ds1, waypoint_objects)` reads every DS1 a catalogue piece can draw and reports two things:

- **A waypoint object**: an object of type 2 whose (act, id) is an `objpreset.txt` row with ObjectClass `WaypointAct3`, `WaypointWilderness` or `WaypointIceCave`. **static**.
- **A link tile**: a wall tile of orientation 10 or 11 (the engine's special tiles). A level's `Warp0..7` and `Vis0..7` links come from these; the tile's main index picks the warp slot (Prev 0, Next 1, Down 4). **inference**: orientation 10/11 as "link tile" fits every vanilla stairs and exit piece below and nothing else, but the engine code that reads it was not traced.

`catalogue.py` writes the DS1 files that have either into the new `links` column of `catalogue.tsv` (outdoor rows). Maze pieces are whole rooms, so they were scanned by piece instead (scratch script, same reader).

### 2. The list per tileset

| Tileset | Carries a link | Clean |
|---|---|---|
| Cave (3) | Prev 83-86 (Up stairs, tile 10/0/21), Next 87-90 (10/1/21), Down 91-94 (10/4/4), Cave Entrance 51, DOE Entrance 52, Treasure 103-107 (10/1/21) | mask pieces 53-67 |
| Crypt (4) | Prev 139-142 (orientation 11, main 0), Next 143-146 (orientation 10, main 1) | mask pieces 109-123 |
| Lava (28) | none | 836-851 (the tileset has no Prev family) |
| Ice (33) | Prev 1018-1021, Next 1022-1025, Down 1026-1029, waypoint 1034 | mask pieces 1003-1017 |
| Jungle | Webby 575-584: `spid*.ds1` (File1: waypoint object and tile 10/0/17) and `spid*2.ds1` (tile 10/1/17); Boggy 585-594: `bog*.ds1` (File1: waypoint object); Pygmy 595-604: `pyg*.ds1` (File1: waypoint object and tile 10/0/0) and `pyg*2.ds1` (tile 10/1/0). File3 of Webby and Pygmy, Files 2 and 3 of Boggy are clean | paths 530-544, transitions 545-574, Head and Tail |
| Snow | 983-986 (Barricade From/To Cave; never used) | 987-1002 and the edge ring 957-982 |
| Siege | none in the strips 865-879 (unused waypoint presets 880, 953, 954) | 865-879 |

Act 5 Lava 1053-1056 has a link tile too (10/33/0) but is not one of the four Cartographer maze rows.

### 3. Why the plugin met them

- **Jungle**: a clearing is one LvlPrest with three DS1 files, and the engine draws the file from the level RNG when the stamp passes `file == -1`. Webby, Boggy and Pygmy therefore showed a waypoint or an exit tile in about two of three draws. **static** (files) plus **needs game** (the draw).
- **Cave, Crypt, Ice**: the plugin placed the tileset's Prev family as the entrance room. Those four pieces hold the Up stairs, a level exit tile. Next, Down, the waypoint and the treasure pieces were never placed by the plugin.

### 4. What changed

- Outdoor: `Piece::usableDs1()` leaves out the DS1 files listed in `links`; a piece with none left is not picked; a clearing with a link variant pins the safe file (`PickedCell::linkSafe`, honoured even with `pinFiles` off).
- Maze: no `MazeTilesets` row has a Prev family, so the entrance is an ordinary mask room (Lava's mechanism: arrival by `MarkArrival`).
- Not changed: the game's own unsteered maze can still place Prev, Next and Down pieces. The plugin steers every slot call, so a Cartographer map does not reach that path; a map that falls back to the game's own builder would.

### 5. Open

Whether arrival lands correctly in Cave, Crypt and Ice without the Prev piece is **needs game**; the in-game results are in the pull request.
