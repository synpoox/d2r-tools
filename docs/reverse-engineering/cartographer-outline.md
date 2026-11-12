# Cartographer outdoor: non-rectangular outline spike (`cartographer`, issue #148)

Desk work only, no game. Part of #19 (outdoor builder phase 8, optional), after S2 (#89, [section 16](cartographer/16-outdoor-s2.md)) and the build hook ([section 8a](cartographer/08a-outdoor-build-hook.md)). Image: build 93847, stamp `0x6ab3782c`, the decrypted memory-layout dump (file offset == RVA); instructions read with capstone, vanilla tables read from a copy of `data\global\excel` (`lvlprest.txt`, `lvltypes.txt`, `levels.txt`). Evidence labels: **static** (read from code or data), **inference** (reasoned from static facts), **not shown** (needs the game). Nothing here ran in a game.

## 1. Answer

**Conditional yes, static only.** A non-rectangular level needs no new engine idea:

- **Shape of the walkable area** is already free. The plan marks every cell outside the shape void with `VoidCell` (`0x3ede00`, flag `0x100`, no room made); S2 saw that work in game for an L plus a branch, with three void blocks. A cross, a U, a ring or a blob is only a different set of void cells. Nothing in `0x3ede00` or in the room maker depends on where a void cell is. One thing S2 did not have: a void cell **enclosed** by open ground (a ring's hole). Mechanically the same call (inference), not walked.
- **The edge** is the open question, and only for the families with no socket edges (Act 1 Wild, Act 2 Desert, Act 4 Mesa). Jungle blocks close their own sides (the exit mask), so S2's void edge looked like dark rock. An open field next to a void cell has no art of its own; the engine's border pass `0x3efaf0` stamps the cliff and border pieces along an outline. Section 3 says what that pass accepts. It accepts a **rectilinear polygon of cell indices** (axis-aligned edges only) and nothing else, which covers L, cross, U and blob shapes, and a ring only with extra work (section 5).
- **The engine's own void pass is not enough** for any shape that is not a rectangle with corner notches (section 4): the plan marks all void cells itself, as it already does.

Recommendation in section 6. Not worth building before Jungle and Snow are checked in game.

## 2. Where the outline comes from (static)

| Function | RVA | What it does (static) |
|---|---|---|
| `OutdoorEntry` | `0x3ee320` | Makes the four grids, then calls `0x3f9b80(block+0xb8, rect, 0, block+0x338)` to build the outline list, shifts it to level-local coordinates, divides by 8 (to grid cells) and merges consecutive equal nodes (`0x3ee3d9..0x3ee46a`). Then dispatches on the act byte; Act 5 reaches our `OutdoorAct5Builder` hook. |
| outline builder | `0x3f9b80` | Decrements the rect's width and height (so nodes are the cell **indices** of the corners, 0,0 to w-1,h-1), allocates four nodes of `0x18` bytes through the game allocator (`0xa20bb0`, `0xa20c80`, vtable slot `+8`), links them in a circle, then calls `0x3f9d60` with the neighbour-level list (`block+0x338`) to carve notches where a neighbouring level joins. A NULL neighbour list is skipped (`test r15,r15; je 0x3f9fb8`). Our slot has no neighbours when `Vis0..7` are 0 ([section 8a](cartographer/08a-outdoor-build-hook.md)), so its outline stays the plain rectangle (inference). |
| outline node | | `x` at `+0`, `y` at `+4`, `kind` byte at `+8`, flags dword at `+0xc` (bit 0, bit 1 used below), `next` at `+0x10`. The list head is `block+0xb8`; the loop ends when `next == head`. |
| border from outline | `0x3efa20` | Walks the list; for each node with flag bit 0 set, asks `0x3ede60` and writes grid `+0x30` and `+0x58`. The Act 5 builder `0x3fe2e0` calls it (at `0x3fe3bf`); `0x3efaf0` is **not** called from the Act 5 builder. |
| edge neighbour test | `0x3ede60` | Only classifies a node that lies on the bounding rectangle (x or y is 0 or w-1 or h-1), looks for a neighbour level whose region holds the cell next door, and returns a bit for it (`1 << (4 + index)`) or 0. A node inside the rectangle returns 0 (`0x3edf35`). It is a level-to-level link test, not a polygon function. Nothing to feed it. |
| outline and border pass | `0x3efaf0` | Called by the vanilla Act 1 builder (`0x3fac10`, at `0x3fac4e`, after `0x3efa20`), a caller at `0x3fbd3b` (the Act 2 builder by its place in the dispatch, not read further) and the Act 4 builder (`0x3fdc00`, at `0x3fdc2c`, not for level `0x6c`). Section 3. |

The vanilla outdoor levels therefore always go through the same four-corner list. No vanilla caller builds anything but a rectangle with neighbour notches, so a general polygon through `0x3efaf0` has never been exercised by the game (inference).

## 3. What `0x3efaf0` accepts (static)

Per consecutive pair of nodes (`this`, `next`; the list is circular):

1. `0x3f9b20` returns `sign(dx)`, `sign(dy)` of the pair.
2. Direction index from the 3 x 3 table at `0x1d10b70`, indexed `(dy+1)*3 + (dx+1)`: `[-1,1,-1,0,-1,2,-1,3,-1]`. So north = 1, west = 0, east = 2, south = 3, and **all four diagonals and the zero vector are -1**. The index is then used unchecked: `-1` reads the piece table four dwords before its start (`0x3efc5a..0x3efc6b`).
3. Border piece: `LvlPrest id = table[0x1d10a50 + (dir*4 + family)*4]` where `family` comes from the level type at level `+0x1e0`: 2 (Wilderness) gives 1 for a node with `kind == 0` and 0 otherwise, 16 (Desert) gives 2, 27 (Mesa) gives 3, 31 calls `0x3fe740`, **anything else gives -1** (a garbage index). The level type must therefore be 2, 16 or 27 for a family built this way, which is what section 8a already requires (the slot's `LevelType` equals the tileset's).
4. Stamping loop (`0x3efc86`): unless node flag bit 1 is set, it steps `(x, y) += (sign dx, sign dy)` from the node to the next one and, at every step, calls `PieceStamp(dataFlag, level, x, y, piece, -1, 0)` (`0x3ecfc0`) and ORs `1` (kind 0) or `3` (kind not 0) into grid `+0x58` at the cell (`0x3faa50`, op 0). The exit test is `x == next.x && y == next.y` (`0x3efc86..0x3efc8e`). **A segment that is not horizontal, vertical or an exact 45 degree diagonal never meets that test and the loop does not end.** A 45 degree segment ends but takes the garbage piece of step 2.
5. Corners: after each segment a second table (`0x3ef980`, `0x1d10c40`) turns the incoming and outgoing direction into a corner row of the same piece table, and `0x3ecfc0` stamps it on the node (`0x3eff90..0x3effcc`). Straight continuations stamp nothing. Rows 0 to 3 of the piece table are the four straight edges, rows 4 to 11 the corner and end pieces.
6. A node whose flags are exactly 1 (bit 0 set, bit 1 clear) is handled per act (`0x3efcf1`): it is a transition marker, not a border, and for the act byte of an Act 5 level (4) it stamps entrance pieces at the segment midpoint. Use flags 0 for ordinary nodes.
7. Void pass (`0x3f0040`): four scans, from the grid's four corners (table at `0x1d111e0`: `flagX, flagY, stepX, stepY` = `0,0,+1,+1`, `1,0,-1,+1`, `0,1,+1,-1`, `1,1,-1,-1`). A scan takes the rows from its corner while the row's edge cell does **not** have bit 0 in grid `+0x58`, and in each such row ORs `0x100` into every cell from the edge inward until the first cell with bit 0. No bounds are checked: a row with no border cell runs off the grid.

Piece ids (`lvlprest.txt`, the table row index; every one is 8 x 8 tiles, one grid cell):

| Family | LevelType | Straight W, N, E, S | Corners and ends |
|---|---|---|---|
| Wild (kind 0) | 2 | 4, 5, 6, 7 (`Bord1..4`) | 8 to 14 (`Bord5..11`), 15 |
| Wild (kind not 0, cliff) | 2 | 0 (none), 16, 17, 0 | 18, 19, 22, 23 (`StnClf5`, `6a`, `7`, `10`) |
| Desert | 16 | 364 to 367 (`edge1..4`) | 368 to 375 (`edge5..12`) |
| Mesa | 27 | 799 to 802 (`Border1a..4a`, 3 files each) | 803 to 810 |

(Row to id: the pieces in rows 4 to 11 were read column by column from the table; which corner is which row was not decoded, only that the corner function picks among them.)

Consequences for a custom outline:

- Every edge must be axis-aligned and have length. An outline traced from a cell mask (`traceEngineOutlines` in `outline_shapes.*`) is always that.
- The polygon runs through the outermost **open** cells, not around them: a rectangle covering `w x h` cells is the list (0,0), (w-1,0), (w-1,h-1), (0,h-1). A reflex corner's node is the open cell diagonally opposite the void cell.
- A one-cell-wide neck makes the polygon double back (two edges on the same cells, a 180 degree turn the corner table has no row for). Shapes need every feature at least 2 cells wide (`thinCells == 0`).
- One list, one loop. `0x3efaf0` reads only `block+0xb8`. The other six lists (`+0xc0..+0xe8`, freed with the same `0x3f9a90`) are not read by it.

## 4. The void pass is not a general void fill (static)

Because a scan stops at the first row whose edge cell already carries bit 0, it only reaches void that **contains a corner of the grid** or that follows from one: the notches of an L (one corner) or a plus (four corners). The tests in `tests/outline_shapes_tests.cpp` model it (`engineVoidMarks`):

| Shape | Void cells the pass leaves unmarked |
|---|---|
| L, cross | none |
| U (notch in the middle of one edge) | the whole notch |
| ring | the hole (also enclosed, no scan can see it) |
| blob | usually some (over half of 100 seeds) |

An unmarked void cell has no piece and no `0x100`, so `0x3ee780` makes a tile-fill room for it (section 8, `Q3`; the plan section 1.4): floor tiles outside the border. The plan therefore marks **every** void cell itself with `VoidCell`, which is what `StampPlan::voidCells` already lists for the Jungle plan, and uses `0x3efaf0` (if at all) for the border only. Marking first or afterwards does not matter: `VoidCell` sets the flag grid cell to exactly `0x100`, bit 0 clear, and the pass only ORs `0x100`.

## 5. Shapes

| Shape | Socket families (Jungle) | Open-field families, via `0x3efaf0` | Open-field families, own border stamping |
|---|---|---|---|
| L, cross, U | Layout plus `VoidCell`. L plus branch walked in S2. | One rectilinear loop. Fine. | Fine. |
| Blob (no holes) | Layout plus `VoidCell`; shape comes from the layout generator. | One loop, after the shape generator keeps every feature at least 2 wide (`blobShape` does). | Fine. |
| Ring or any hole | Hole cells are void blocks; no new call. The enclosed void is untested live. | **Two loops**, and the list holds one. A second call with `block+0xb8` swapped to the hole's list is plausible (the pass reads the head fresh), with the hole walked the other way round so the cliff faces the right side; not shown. A single list that bridges outer and hole with a zero-width corridor doubles back, which the corner table does not handle. | Stamp the hole's boundary cells like any boundary cell. |
| Diagonal edges (a staircase, a "round" blob outline) | Not an outline question; a staircase of cells is a rectilinear loop. | Never as a diagonal segment (section 3, item 4). | Same as above. |

"Own border stamping" means the plan lists, for each border cell (an open cell with a void 8-neighbour), the piece from the tables in section 3 and replays it with `PieceStamp`, like the layout stamps. No new native call, no list rewrite. It costs a corner and end selection table in the plugin (to be copied from `0x1d10c40` and the piece rows once, in a game-checked form).

## 6. Routes and recommendation

**Route A: rewrite the outline list, let `0x3efaf0` stamp the border.** In the `0x3fe2e0` hook, after `0x3ee320` built the rectangle, free that list (`0x3f9a90(block+0xb8)`) and build the polygon's nodes through the game allocator (`0xa20bb0` then `0xa20c80`, slot `+8`, size `0x18`; the same sequence `0x3f9b80` uses), then call `0x3efaf0(dataFlag, level)`. Gets engine-chosen pieces and corners for free. Needs two new native calls (allocator, `0x3efaf0`) and a hand-built list the game later frees (`0x3ee220` walks it; a mistake faults at teardown, section 8 Q4). `0x3efaf0` has a signature in section 8 (`48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9 48 81 EC E0 00 00 00 ...`) and is hookable; it is not a part the plugin uses yet.

**Route B: stamp the border from the plan.** Only `PieceStamp` and `VoidCell`, both already called by the hook, both with game history (S2 for `VoidCell`). The plan holds the shape, the void cells and the border pieces; the host and a client compute the same list from the seed. Handles holes and diagonal staircases. Costs the corner table.

**Recommendation:**

1. **Do not build either before Jungle and Snow are checked in game.** Open-field families are phase 8, optional (plan section 6), and need the same builder, entrance and boss work that Jungle still waits for.
2. **If it is built, take route B.** It keeps one hook set, adds no allocator call and no list the game must free, and does not depend on the `0x3efaf0` loop's hang on a bad segment. Route A is the shorter path only for a single hole-free loop.
3. **Shapes first: rectilinear, every feature at least 2 cells wide, in one piece.** `outline_shapes.*` has L, cross, U, ring and blob generators with those guarantees (blob; the others by their arguments), and the loop tracer in corner and in engine cell coordinates.
4. **Keep the void marking in the plan** (all void cells), as in section 4.

No answer is given for how the border art looks, whether the corner rows join, or whether a cliff at a void edge reads right; those are art checks in game.

## 7. What was built

`plugins/cartographer/src/outdoor/outline_shapes.{h,cpp}`: pure shape masks (`rectangleShape`, `lShape`, `crossShape`, `uShape`, `ringShape`, `blobShape`), analysis (`openRegions`, `enclosedVoidRegions`, `borderCells`, `thinCells`, `engineVoidMarks`, `unmarkedVoidCells`), loop tracing (`traceLoops` in corner coordinates, `traceEngineOutlines` as engine cell-index polygons) and `checkLoopForEngine` (axis-aligned, non-zero edges). Not wired into the build hook, the plan or any header the plugin includes. Unit tests: `tests/outline_shapes_tests.cpp` (13 cases; 300 seeds of blobs checked for one region, no holes, engine-ready loops and no scan overrun). The glob in `tests/CMakeLists.txt` only builds a test whose name matches a core, so `outline_shapes_tests` has its own block linking `cartographer_outdoor`.

## 8. Needs game (none of this is shown)

1. A **ring**: one enclosed void block in the Jungle layout (S2 had only edge voids). Walk beside the hole; confirm the player cannot enter it and the automap shows it.
2. For an open-field family, build a rectangle with route B's border stamps (one family, say Wild), and look at it: do the straight pieces and corners join?
3. Then an L and a U with a notch in the middle of an edge, to confirm an unmarked notch really gets fill rooms without the plan's own `VoidCell`, and none with it.
4. If route A is ever wanted: build a list through the allocator, call `0x3efaf0`, leave and return (teardown), watch the crash guard.
5. Whether a client builds the same border from the same seed (the plan is data, so it should; section 12).
