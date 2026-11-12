# Cartographer outdoor Snow (Barricade) tileset: what a Snow layout is built from (issue #147)

Desk work for issue #147, "Cartographer outdoor Snow (Barricade) tileset". Companion to [cartographer.md](cartographer.md) sections [8](cartographer/08-outdoor-builder.md), [8a](cartographer/08a-outdoor-build-hook.md) and [16](cartographer/16-outdoor-s2.md) (the Jungle outdoor build, the slot level, the S2 spike). Everything here is read from tables and DS1 files of build 93847; nothing was run in a game, and the plan code that follows from it (`plugins/cartographer/src/outdoor/snow_plan.*`) is not wired into the portal, the tables, the installer or the outdoor hook.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer outdoor builder, Snow family (`src/outdoor/snow_plan.*`, `tests/snow_tests.cpp`); not called by the plugin yet |
| Module | none: table and DS1 data only. The engine side is cartographer/08-outdoor-builder.md (Q8 for the Barricade pieces) and 8a (the hook) |
| Build identity | D2R 3.3 build 93847, stamp `0x6ab3782c` (`D2RLoader_6ab3782c_dump.exe` for the section 8 references) |
| Data read | `data/global/excel/{levels,lvltypes,lvlprest,lvlsub}.txt` of the D2RLoader 3.3.0 compiler copy and of the d2rAI-toolkit `d2r-data/93847` (the same values); the DS1 and DT1 files under the toolkit's `d2r-data/93847/data/global/tiles/expansion/siege/`, read with `plugins/cartographer/tools/d2r_readers.py` |
| Guard | none needed: no address, signature or byte guard is recorded or changed here |
| Validation | Static only. Edge sockets are derived from the DS1 walk flags and from the piece names; no stamped Snow piece has been seen in a game |
| Failure mode | Fail open, as for the Jungle: the plan refuses a bad request and the game then builds its own level |

## 1. Answer in short

- **Level rows.** Act 5 outdoor rows with `DrlgType` 3 and `LevelType` 31 (the Barricade level type): 111 Frigid Highlands, 112 Arreat Plateau and 117 Frozen Tundra. Frozen Tundra (117, "Act 5 - Barricade Snow") is the snow one and the model for a Snow slot row: `SubType` 11, `SubTheme` 1, 128 x 80 tiles, `Pal` 4. Rows 111 and 112 use `SubType` 10 (the dirt decoration set) and their sizes are `-1` in the extract; the engine sizes them (cartographer/08-outdoor-builder.md Q10). Level 110 (Bloody Foothills) is `LevelType` 30 ("Act 5 - Siege") and uses other tiles.
- **LevelType.** 31, "Act 5 - Barricade". It is shared by the dirt pieces (915..930) and the snow pieces (987..1002): the snow look comes from the snow DS1 files, not from the level type. A Snow slot level therefore needs `LevelType` 31, where the Jungle slot has 21; one slot row cannot serve both (see section 7).
- **Pieces.** LvlPrest 987..1002, "Act 5 - Barricade 1..16 Snow", all 16 x 16 tiles. They are not a 4-bit exit-mask family like Jungle 530..544: they are a Wang-tile style family of fence-line tiles that join by the position of a fence where it crosses an edge (section 4). So the Jungle picker, which chooses one piece per exit mask, cannot use them; `snow_plan` is a separate plan that builds lines of pieces and produces the same `StampPlan` the Jungle plan does.
- **Cleaner than the dirt twins.** The dirt pieces 915..930 carry the same grammar but with loose objects on their edges and DS1 variants whose edges differ from each other. The snow pieces have clean edges and one DS1 file each (987 has two, with identical edges), so Snow is the better first family.
- **Not planned yet:** the borders (957..982), the cave stairs (983..986), the waypoint (954), the Hell portals (955, 956), the Blaze Ripper unique (952) and the 8 x 8 decoration pieces; section 5 lists them with what is known.

## 2. The level rows (static, `levels.txt`)

| Id | Name (string) | DrlgType | LevelType | SubType / SubTheme | Size (tiles) | Origin | Vis / Warp | Waypoint | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 110 | Act 5 - Siege 1 (Bloody Foothills) | 3 | 30 | -1 / -1 | 240 x 48 | 760, 1000 | none | 255 | other level type; not a Barricade level |
| 111 | Act 5 - Barricade 1 (Frigid Highlands) | 3 | 31 | 10 / 1 | -1 x -1 | -1, -1 | Vis2 0 | 31 | the level the section 8 Q8 restamp (`0x3feb80`) runs on (id `0x6f`) |
| 112 | Act 5 - Barricade 2 (Arreat Plateau) | 3 | 31 | 10 / 1 | -1 x -1 | -1, -1 | Vis2 113, Warp2 71 | 32 | cave entrance (`913`/`914`) |
| 117 | Act 5 - Barricade Snow (Frozen Tundra) | 3 | 31 | 11 / 1 | 128 x 80 (all difficulties) | 2000, 1896 | Vis0 115, Vis2 118, Warp0 72, Warp2 71 | 36 | the snow level; cave records (983..986) |

A Snow slot row is a clone of 117 with the Jungle slot's requirements from cartographer/08a-outdoor-build-hook.md (`Vis0..Vis7` all 0, its own offset and size, no waypoint, no warps, the town and quest columns of the other slot rows). `SubType` 11 is what adds the snow patches and snowy objects (`lvlsub.txt` rows "Dirt patches" and "snowy objects": `snowpatchlg_inv.ds1`, `treesandrocksSnow.ds1`, `Dt1Mask` 524305 and 3671091); with `SubType` -1 the level has no decoration, and whether the engine's decoration pass writes into a level the hook built is not known.

Sizes: Frozen Tundra is 128 x 80 tiles, 8 x 5 cells of 16 tiles. `snow_plan` calls that Medium, Small 6 x 4 and Large 10 x 7 cells, at most 12 x 12 (192 tiles); the real limit is the slot row's declared size (cartographer/08-outdoor-builder.md Q10).

## 3. The level type: `lvltypes.txt` row 31, "Act 5 - Barricade"

DT1 files, in column order (bit `n` of a lvlprest `Dt1Mask` is File `n + 1`):

| Bit | File | Bit | File | Bit | File | Bit | File |
|---|---|---|---|---|---|---|---|
| 0 | Siege/Ground | 8 | Siege/building | 16 | Siege/ascendors | 24 | MountainTop/Columns |
| 1 | Siege/fence | 9 | town/waypt | 17 | Siege/spike | 25 | IceCave/Exterior |
| 2 | Siege/cliff | 10 | Siege/pinetree | 18 | Town/collision | 26 | IceCave/Warps |
| 3 | Siege/fortified | 11 | Siege/battle | 19 | Siege/shrub | 27 | Ruins/Walls |
| 4 | Siege/snow | 12 | Siege/barricade | 20 | Siege/rocks | 28 | Ruins/Furnish |
| 5 | Siege/trees | 13 | Siege/rockclifffloor | 21 | Wildtemple/Entrance | 29 | Siege/pens |
| 6 | Siege/special | 14 | Siege/camp | 22 | MountainTop/floor | 30 | Siege/hellgate |
| 7 | Siege/trench | 15 | Siege/rockcliff | 23 | MountainTop/Entrance | 31 | (empty) |

The 16 fence pieces carry `Dt1Mask` `0x1fbc33` (987..994, 915..922) or `0x1fbc37` (the caps 995..1002, 923..930, which add cliff): ground, fence, snow, trees, pinetree, battle, barricade, rockclifffloor, rockcliff, ascendors, spike, collision, shrub and rocks. The dirt and the snow twin of a piece have the same mask. All of it is inside the level type's list, so no tile from another act is needed (the Jungle slot had to load Act 3 tiles into Act 5; Snow does not).

## 4. The Snow fence pieces (987..1002)

`lvlprest.txt`: `Def` 987..1002, `Outdoors` 1, `SizeX` = `SizeY` = 16, `Dt1Mask` as above. Names are "Act 5 - Barricade N Snow"; the file letters name the pieces' joins. The DS1 walk flags (subtile flag bit 0, 5 x 5 subtiles per tile, 80 x 80 per piece) of the outermost subtile row or column on each side give the table below. "Blocked" lists `start+length` runs of blocked subtiles along that side (north and south sides left to right, west and east sides top to bottom); "empty" is the run with no tile at all.

| Id | File | N | E | S | W |
|---|---|---|---|---|---|
| 987 | `snowbarrAA1` (and `AA2`, same edges) | open | 23+2 | open | 23+2 |
| 988 | `snowbarrBB1` | open | 58+2 | open | 22+3, 58+2 |
| 989 | `snowbarrCC1` | 25+2 | open | 25+2 | open |
| 990 | `snowbarrDD1` | 55+2 | open | 55+3 | open |
| 991 | `snowbarrAB1` | open | 58+2 | open | 23+2 |
| 992 | `snowbarrBA1` | open | 23+2 | open | 58+2 |
| 993 | `snowbarrCD1` | 25+2 | open | 55+3 | open |
| 994 | `snowbarrDC1` | 55+2 | open | 25+2 | open |
| 995 | `snowbarrBR1` | 23+2, empty 25+55 | empty (all) | 23+2, empty 25+55 | 58+2 |
| 996 | `snowbarrAR1` | 23+2, 30+5, empty from 25 | empty (all) | 23+2, empty 25+55 | 23+2 |
| 997 | `snowbarrFB1` | empty 0+55, 55+1 | 58+2 | empty 0+55, 55+1, 60+3 | empty (all) |
| 998 | `snowbarrFA1` | empty 5+50, 55+1 | 23+2 | empty 0+55, 55+1 | empty 5+75 |
| 999 | `snowbarrCR1` | 25+2 | 25+2, empty 30+50 | empty (all) | 25+2, empty 30+50 |
| 1000 | `snowbarrDR1` | 55+2 | 25+2, empty 30+50 | empty (all) | 25+2, empty 30+50 |
| 1001 | `snowbarrFC1` | empty 5+75 | 64+1, empty 0+60 | 25+17, 65+2 | 64+1, empty 5+55 |
| 1002 | `snowbarrFD1` | empty 5+75 | 64+1, empty 0+60 | 25+2, 55+2 | 64+1, empty 5+55 |

Reading it:

- **Two kinds of run.** AA, BB, AB and BA have open north and south sides and one short blocked run (two subtiles, a fence post) at row 23 or row 58 on their west and east sides: a fence running west to east. CC, DD, CD and DC mirror that: open west and east, a blocked run at column 25 or column 55 on north and south: a fence running north to south. The two row positions are the sockets **A** (23) and **B** (58); the two column positions are **C** (25) and **D** (55).
- **The name gives the sockets.** For the west to east group the first letter is the west socket and the second the east one (`AB`: A on the west side, B on the east side; `BA` the reverse). For the north to south group the first letter is the north socket and the second the south one. The table confirms it for all eight: 987 A/A, 988 B/B (its west side also holds an extra run at 22+3 beside the B run), 991 A to B, 992 B to A, 989 C/C, 990 D/D, 993 C to D and 994 D to C. One mismatch of a subtile: the D run is two wide on a north side and three wide on a south side (990, 993, 994), so two stacked D pieces overlap by one subtile instead of meeting exactly.
- **Caps.** The catalogue's R pieces (995 BR, 996 AR, 999 CR, 1000 DR) have tiles only in the half of the piece on the run side and nothing (empty) on the far side: their one socket is on the side facing the run (B or A on the west for 995/996, C or D on the north for 999/1000). The F pieces mirror them (997 FB and 998 FA: socket on the east, west side empty; 1001 FC and 1002 FD: socket on the south, north side empty). The letters name the socket, F or R the end ("first", "rear"). 1002 FD carries both a C run (25) and a D run (55) on its south side; the plan joins it only to D.
- **Grammar.** A west to east line is an F cap (997 or 998), run pieces of the AA/BB/AB/BA group whose west socket equals the previous piece's east socket, and an R cap (995 or 996) whose west socket equals the last east socket. A north to south line is the same with 1001/1002, the CC/DD/CD/DC group and 999/1000. Lines of the same orientation sit side by side or one above the other freely, because a run's other axis is fully open ground. A west to east run next to a north to south run does not fit (A/B sockets against an open side), so one map is one orientation.
- **Dirt twin.** The dirt piece of each snow piece is `id - 72` (915..930) with the same name letters, the same `Dt1Mask` and the same sockets, but its edges carry loose objects (for instance 916's west side `58+2, 67+3`, 922's north side `3+20, 27+11`) and its second and third DS1 files differ from the first at the edges. `catalogue.tsv` already lists both sets with all-`p` sockets (the tool's own note calls the Barricade and Snow sockets weak); the table above replaces that for Snow.

How to reproduce the table: with `plugins/cartographer/tools/d2r_readers.py` read each `snowbarr<xx>1.ds1` from the toolkit's `d2r-data/93847/data/global/tiles/expansion/siege/`, call `layout()` with the DT1s the DS1 header lists (`header_dt1s`), take bit 0 of each tile's 25 subtile flags (`Layout.flags`, row-major 5 x 5 per tile), build the 80 x 80 grid and read the first and last row and column.

## 5. Other Act 5 outdoor pieces (not used by the plan yet)

From `lvlprest.txt`, build 93847. "Files" is the `Files` column, "listed" the DS1 files named.

| Ids | What | Size | Notes |
|---|---|---|---|
| 895..906 | Barricade ravine and cliff borders (dirt), 16 x 16 | 16 x 16 | dirt set; the engine's border pass, see Q3 and Q8 |
| 957..968 | Cliff Border 1..12 Snow | 16 x 16 | 967 ("Border 11") has no DS1 file: never stamp it |
| 969..980 | Ravine Border 1..12 Snow | 16 x 16 | 979 ("Border 11") has no DS1 file: never stamp it |
| 981, 982 | Ravine-Cliff, Cliff-Ravine border 5 and 7 Snow | 16 x 16 | |
| 907..910 | Entrance and exit, 16 x 32 and 32 x 16 (no snow twin) | | stamped by `0x3feee0` at cells with flag bit `0x400` (section 8 Q8) |
| 911..914, 983..986 | Cave exits and entrances, dirt and snow (32 x 16 and 16 x 32) | | 983..986 are the records for level 117 at `0x1d12bf8`; their `Dt1Mask` adds the fortified, IceCave Exterior and Warps DT1s |
| 931..938 | Prison variants of 915..922 (dirt only) | 16 x 16 | the restamp of section 8 Q8 turns 915..922 into these in level 111 only |
| 939..941, 948..951 | Buildings, filler and snow lake pieces | 8 x 8 | decoration, 4 files each (951 and 950 list one) |
| 942..947 | Ruins | 8 x 8 | decoration |
| 952 | Snow Unique (`BlazeRipperABR.ds1`, `BlazeRipperCDR.ds1`) | 16 x 16 | `Files` 0 with two files listed: not drawn by the ordinary file index; the names (A/B and R, C/D and R) look like cap pieces for the two orientations. Unverified |
| 953, 954 | Waypoint dirt and snow | 8 x 8 | 954 `SnowWaypoint1/2.ds1`, `Dt1Mask` `0xc0a13` (ground, fence, snow, waypt, pinetree... bits 0, 1, 4, 9, 11, 18, 19) |
| 955, 956 | Hell portal N and W | 16 x 16 | `hellwrpN/W` plus a `snow` variant; `Files` 0 |

File counts: a pinned file index must be below both `Files` and the number of listed files. For Snow that is 2 for 987 and 1 for every other piece. The dirt table disagrees with itself (915 says 3 and lists 2, 916 says 2 and lists 3), another reason to plan Snow first.

## 6. What the plan builds

`planSnowMap(SnowRequest)` returns a `SnowPlan` with the same `StampPlan` as the Jungle plan, so the outdoor hook could replay it unchanged:

- A rectangle of `cellsW x cellsH` cells of 16 tiles (2 x 2 engine grid cells each), at most 12 x 12, at least 3 along the line, inside the slot row's declared size (`maxTilesX/Y`). The origin is the slot row's. No void cells (the rectangle is full).
- One orientation per map, taken from the seed unless the request fixes it. Each line is an independent random stream of the seed: a start cap, runs whose sockets chain, an end cap.
- With `pinFiles` off every stamp's file is `-1` (the engine draws it); on, a file below the piece's `files`.
- The entrance rectangle is the first run cell of the middle line (never a cap: a cap's far side is empty). No boss or event rectangle yet.
- `checkSnowPlan` re-checks a finished plan without using the planner: size, one stamp per cell on the 16-tile grid in row-major order, kinds by position, socket chain, file range, entrance.

What it does not do: any border (the outside of the rectangle is whatever the engine has there), monsters, waypoint, cave exits or a boss. The level is a field of fence lines, not yet a playable map.

## 7. Open, and what wiring is left

Needs game (none of this has been seen):

1. The ids 987..1002 stamped through `0x3ecfc0` into a type 3 level with `LevelType` 31: the pieces draw with snow tiles and nothing is black (section 8 Q11 for the Jungle; Snow has no cross-act tiles).
2. That a line's fences meet on screen at sockets A/B/C/D, and that a D to D join (two and three subtiles) looks right.
3. What the empty side of a cap looks like with nothing beside it: void (`0x100`) cells, a border piece, or the rock the S2 spike showed (section 16).
4. The arrival cell is walkable and the finder lands in the entrance rectangle (section 16).
5. Whether the level can be walked from end to end: the fence posts are short runs, but the walk flags of a whole map were not checked against crossings.
6. Whether the engine's `SubType` decoration pass (rows 10/11) writes into the hook-built level and what it does to the fence cells.
7. The origin of 915..930 in the vanilla build (section 8 Q8, still open).

Wiring (issue #147, second PR): the tables, the installer, the plan cache and the outdoor hook.

- Done: `tools\install.ps1` adds the level "Cartographer Snow" (Act 5, `DrlgType` 3, `LevelType` 31, 192 x 192 tiles at 6000, 3600, copied from level 117 with `Quest` 0, `LevelGroup` Null, no entry or warp text, `SubType` and `SubTheme` -1, `Vis0..Vis7` 0, `Warp0..Warp7` -1, `Waypoint` 255) and the `slots.txt` row `OutdoorSnow`. The offset is clear of every Act 5 level (the farthest is x 5055), of the Maze level (x 6000..6360, y 0..360) and of the Outdoor level (y 3000..3384). `ValidateSlots` requires the name "Cartographer Snow" for `OutdoorSnow`; `MapSlotName` sends a Snow map to that slot; `OutdoorSlotProblem` checks `LevelType` 31 on it and `OutdoorSlotRefusal` names the slot. `Snow` is a ready tileset in `maps.txt`.
- Done in the plan and hook: `planOutdoorMap` dispatches `Snow` to `planSnowMap` (Small, Medium or Large cells, the shape is ignored, only `stamps` are filled; the plan cache keys it like Jungle). The outdoor hook keeps one slot record per outdoor slot (level, geometry, refusal), builds either slot level, picks the record by the open map's tileset and refuses (the game then builds its own level) when the level being built is the other slot's. `OutdoorSlotLevelId()` returns the open map's slot level, so the event code and the arrival probe follow it.
- Left: the AutoCheck game check for the Snow level, an example Snow row in `examples/first-map` (it needs its own item code), and every in-game check in the list above. A Snow plan has no event or boss rectangle, so a `Boss` or event on a Snow map finds no room until those are planned.
