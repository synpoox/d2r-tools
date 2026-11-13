## 33. One map from several tilesets woven together (research spike, issue #396)

Build 93847 (stamp `0x6ab3782c`), D2RLoader 1.3.1-beta. Part of #19, plan phase 9 ([cartographer-plugin.md](../../cartographer/plans/cartographer-plugin.md)). Desk work only: no plugin code, nothing run in a game. Evidence labels: **static** (code or table read), **inference**, **guess**, **live** (seen in game earlier), **needs game**. RVAs are in the memory-layout dump (`plugins/cartographer/tools/dump_dis.py`, capstone); table reads are from `d2rAI-toolkit/d2r-data/93847` and `plugins/cartographer/tools/catalogue.py`. Lighting is only touched on in section 3 (note 32, open PR #388, owns it).

### 1. Answer

**One level can hold pieces from several tilesets. Weave inside one slot level; do not join levels.**

- A level has one LevelType, which fixes **one DT1 file list** (up to 32 names). But each room loads only the files its own piece's `Dt1Mask` selects from that list, so a room is not tied to the level's tileset, only to the list. Widen the list (the other tileset's DT1s appended) and the rooms of both tilesets draw. Cave plus Lava needs 12 files, Jungle plus Snow 24 (used files only), far under 32.
- Everything else is per level and cannot be woven: palette (levels.txt `Pal`, act wide), HD lighting biome, the monster lists in the levels row, the LevelType-keyed automap lookup, the builder (`DrlgType`, and for a maze the LevelType case).
- Cheapest route that could work: **maze host, Cave plus Lava** (both 24-tile rooms, both W/E/S/N exit-mask families, same exit mask convention). The slot stays a Lava level (LevelType 28); the piece picker returns Cave ids in the Cave zone; one DT1 is appended to the level's file list. About three small changes, one new hook at most (section 5).
- Two slot levels joined by a warp is the fallback and loses what makes a "map" (one portal, one level, one automap, one seed); it is also fenced by the engine (section 6).

### 2. What the engine data says

| Fact | Evidence | Level |
|---|---|---|
| `lvltypes.txt` has `Id`, `File 1`..`File 32` (DT1 paths under `data/global/tiles`) and `Act`. A row is `0x781` bytes: 32 name slots of `0x3c` bytes, then the Act byte at `+0x780`. The getter `0x32c300(levelType)` returns `rows + id * 0x781` and asserts on an id past the vector (`0x32c31e`). The vector grows with rows, so a LevelType above 35 loads | `0x32c300..0x32c34b`; `knowledge/engine-limits.md` | static |
| **Room loader `0x3f3970`** `(dataFlag, room)`: takes the row at `[[room+0x90]+0x1e0]` and, for each set bit `i` (< 32) of `[room+0x80]`, appends the name at `row + i*0x3c` to the room's tile list (`room+0xb0`, via `0x1460250`); then adds three fixed files (`Blank.dt1`, `InvisWal.dt1`, `Warp.dt1`) and sets room flag `0x1000000`. Callers `0x328579`, `0x328949`, `0x3289b9`. Unique signature: see the line under the table | `dump_dis.py ... dis 0x3f3970 90`, `pat` | static |
| `[room+0x80]` is written at room creation (`0x3dde70`, `0x3dcd25`), a creator argument. That it comes from the piece's lvlprest `Dt1Mask` was not traced to the call chain; it is consistent with every vanilla row read: Lava pieces carry `2047` (all 11 of LvlType 28's files), Cave pieces `1` (its one file), Jungle `255` (of 14), Snow `0x1fbc33`/`0x1fbc37` (cartographer-snow.md section 3) | `lvlprest.txt` + `lvltypes.txt`, script over all of them | inference (strong) |
| The DT1 names in a DS1's own header are not used in game; a room's files come only from LevelType plus mask (the catalogue tool reads the header only to compute edges) | loader above | static |
| Cross-act DT1s already work in the slots: Jungle (LvlType 21, Act 3) draws in the Act 5 outdoor slot (section 4 S1, 8 Q11), Lava, Cave, Crypt and Ice draw in the Act 5 maze slots (checks 2026-11-12). The client act load `0x86bc0` preloads only the current act's rows, but the room loader above adds the files per room, so no preload hook was needed | sections 4, 8 Q11 | live |
| Two DT1 files with the same tile key `(orientation, main, sub)` in one list are not a problem across rooms: a room's list holds only the files of its mask. Within one piece it is the vanilla situation. Cave vs Lava pieces never share a mask bit after the remap in section 4 | loader above | inference |
| Counts (non-empty files): Cave 1, Crypt 2, Ice 1, Jungle 14, Lava 11, Snow/Barricade 31 (bit 31 free), Siege 11. Tiles a family really uses (union of its prest masks): Jungle 9, Lava 11, Snow 15, Ice 1 | script over `lvltypes.txt`, `lvlprest.txt` | static |
| Bit 31 of a mask is unused by every vanilla row (no type has a 32nd file), so it is free as a tag if a hook ever needs to mark guest rooms | same | static |

Signature of `0x3f3970` (one match in `.text`): `48 89 5C 24 18 55 56 57 48 81 EC 40 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 30 01 00 00 48 8B 82 90 00 00 00`.

Empty slots are tolerated (inference, strong): vanilla Den of Evil entrance (lvlprest 52) has mask `0x100103` (bits 0, 1, 8, 20) in a LevelType with one file, and the game loads it. Guest masks only set named bits anyway.

### 3. Per-level things a woven map cannot split

| Thing | Where it is chosen | Consequence for a woven map | Level |
|---|---|---|---|
| Palette | levels.txt `Pal`; `SetActPalette` `0xcd6d0` is set by the client unit placement `0x9bf00` from the level record's `Pal`; legacy `pal.pl2` per act (`LoadActPalette` `0x9041a0`). DT1s carry no palette | One palette for the whole map: the slot level's (Act 5, `Pal` 4). Tiles are drawn with it, as the Jungle (Act 3 art) already is in the Act 5 slot. Mixing Act 3 and Act 4 art means both use one act's palette, so one side's colours shift (how much is an art question) | static + live for Jungle in Act 5 |
| HD lighting and biome | a level's HD preset names a biome (`data/hd/env/biome`), shared by every map on a tileset (note 32 (level lighting, open PR #388), #380) | One lighting for the map. #380's per-map tint applies to the whole woven map | static (from #388) |
| Automap | `0x32bdf0 (levelType, tileType, style, sequence) -> cel or -1`, no bounds check on `levelType`; AutoMap.txt rows keyed by the 36 built-in level type names (`0x32cae0`, table `0x1cfe110`) | Tiles of the added tileset look up in the host's LevelType, so they get no automap line or the wrong one (inference). Fix: hook `0x32bdf0` (the toolkit records its entry bytes `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20`, but they match 632 places in `.text` and are not a usable guard; a longer unique signature is needed before any automap hook) and, on -1, try the partner LevelType. | static + inference |
| Monsters, density, level, uniques | levels row and region record of the slot level ([section 15](15-map-monster-columns.md)) | One list for the map. Fits the "two biomes, one fight" idea, not "Cave monsters here, Lava monsters there". Per-zone spawning would be a per-room hook | static |
| Builder | `DrlgType` plus (maze) `[level+0x1e0]` switch at `0x3e69e0`, jump table `0x3e76d8`, index `type-3`, 33 entries (section 23, 24) | One builder per level. A LevelType above 35 would fall outside the table, so a **maze host keeps a real LevelType 3..35**. The outdoor stamper hook (`0x3fe2e0`, section 8a) has no such limit: it stamps whatever LvlPrest id the plan lists | static |

### 4. Cheapest route (design)

**Mechanics.** A woven map = host tileset A plus guest tileset B in one slot level.

1. **File list.** Append B's DT1s to the end of A's row: A keeps its indices, B's file `k` goes to `n_A + k`. Cave (1 file) as the guest of Lava (11 files) uses index 11.
2. **Masks.** B's pieces must carry masks shifted by `n_A`. Either a **copy of B's lvlprest rows** with `Dt1Mask << n_A` (installer data, ids appended after 1091; the pieces' `File1..6` and sizes unchanged; same pattern as the slot rows the installer already writes), or a hook on `0x3f3970` that shifts the mask for rooms of guest pieces (needs the room to know its piece id; **guess**, `piece+0` is read in section 23).
3. **Who writes the row.** Appending a file to Lava's row 28 changes a row vanilla levels use. It is harmless to them (no vanilla Lava mask sets bit 11, and Lava keeps its mask `2047`), but it is a shared-table edit. Options: (a) the installer writes the row (fast, prototype only); (b) the plugin patches the in-memory row at map open (like `level_row.cpp` for the levels row, section 15; **needs the row layout above and `0x32c300` called, not signed yet**); (c) a loader hook that adds the guest's file for guest rooms and leaves every row alone (one hook, `0x3f3970` signature above). Recommendation: (a) for the prototype, (b) or (c) for a release.
4. **A custom LevelType (row 36+) is not the cheap route for a maze**: the maze switch only covers 3..35, the automap lookup has no bounds check past 35. It would suit the outdoor host (stamper, no LevelType case) only.
5. **Piece picking.** The maze picker `0x3eb960` returns `base_A + mask` from the slot LevelType. A woven plan needs `base_B + mask` in guest cells: the same steering hook family as option C of the plan (`First + mask - 1`), here per cell. The plan decides zones; the picker is the one place that writes the piece id.
6. **Which pair first.** Cave + Lava on the maze host: same cell size (24), same mask rows (Cave `52 + mask`, ids 53..67; Lava `836 + mask`, 837..851), both walked by the same walk (Lava case `0x3e6c31` calls no special rooms and no ring grower, section 23). Cave has no closed cell (X) piece; Lava does (836), so the host's X serves both. Ice (16-tile) and Crypt are other sizes and cases; Jungle + Snow (outdoor stamper, same sizes need a seam rule) is the outdoor analogue.

**Maze and outdoor in one map.** The builder is per level, but the outdoor slot stamps by LvlPrest id, so a maze piece (24 x 24 = 3 x 3 cells of 8, the Jungle blocks are 32 and the Head/Tail 64 x 32) can be stamped by `PieceStamp` (inference, never tried). Cost: the maze's own room links and fill (`0x3e77a0`) are absent in the outdoor builder, so the plan must own them (the outdoor build already leaves rooms unlinked and walkable, sections 8a and 16: **needs game** for a maze piece's walls). So: maze plus outdoor is possible only on the **outdoor host**, never by mixing the two builders; same-builder pairs on the maze host are the safe first step.

### 5. Seams and the edge-void rule

- **No engine seam logic exists.** Pieces butt at the 8-tile grid. What the player sees at a join is the two pieces' own edge art plus walls. The engine's border pass (`0x3efaf0`, outline note) is for the level rectangle only.
- **Measured edges (static, DS1 walk flags, `catalogue.py` readers): Cave (24 x 24, 120 subtiles) vs Lava.** Lava exits are narrow and fixed: east side runs at subtiles 50..69 (+ 74); west 50..53, 55..69 (+74). Cave exits are wide: east 0..19, 55..57, 60..89, 93..119; west 5..49, 55..57, 60..89, 93..119 (both DS1 variants of a piece scan alike on these runs). A Cave-east to Lava-west join overlaps on subtiles 60..69 (**two tiles** of floor) plus the small runs 55..57 and 74. So the walkable seam exists, narrow, and Cave's wide opening shows as wall or floor that ends in Lava's wall: a visible hard step in the art, not a hole (inference).
- **Bridge pieces.** A true transition needs new DS1 art. That is allowed (the installer already writes lvlprest rows, and the plugin may ship its own resources) but it is new art and out of scope for this spike. So: accept the narrow overlap, or join zones only through pieces whose edges match (the catalogue's `edge*` columns plus a per-pair overlap check from the DS1 flags, as `cartographer-snow.md` section 4 does for sockets). Choosing joins from the walk-flag scan, not the piece names, is already the rule for Lava (839, 845, `a4-lava.md`).
- **Edge-void rule.** Each tileset must close its own map edge (owner rule 2026-11-13; Lava's filler ring, section 23, `ringCells`). In a woven map the filler rooms take the closed piece of the host (Lava X 836, which loads with the host's own list), so the ring is host-only and needs no guest filler; a guest-only edge (a Cave room on the rim) is closed by the same X ring one cell out. Check in game that the guest's wide openings do not leak into a filler cell as black (the Lava ring already fixes black for narrow openings).
- **Transition rule for the plan.** Zones are contiguous groups of cells; a border between zones is a set of cell pairs; a pair is allowed only if both sides have an open socket (`p`) and the DS1 overlap is at least one tile; else the border is closed with the `x` side. The layout step keeps every zone border connected through at least one allowed pair.

### 6. Why not several levels with warps

Owner decision D9 is one level, no zone warping. Engine fences: portals link levels within one act only (`0x3e2b5c8`), and the red portal opens one slot level per game; two levels mean two seeds, two populates and a second arrival, and the automap would stop reading as one map. Warps would give per-tileset palette, lighting, monsters and automap for free, so they stay the fallback if a game check shows the single-level palette or automap mix is unacceptable.

### 7. Prototype and how to tell it worked

**Prototype P1 (smallest).** Cave guest, Lava host, maze slot (slot 7, test save, `debug_mode` and `cheat god 1`; restore the slot tables after). A debug plan of a 3 x 1 strip: Lava, Cave, Lava (three rooms, two seams). P1 needs:

- **Data (installer, section 4 (a)):** the Lava row gets `Cave.dt1` at index 11; **15 copies of the Cave mask rows 53..67 with `Dt1Mask` = `1 << 11`** and new ids after 1091 (`File1..6` and size unchanged). Without the copies a vanilla Cave piece (mask 1) loads bit 0 of the Lava row, `Lava/Floor.dt1`, not Cave's tiles. P1 places the **copy ids**, never 53..67.
- **Mechanism (new plugin code, no new hook):** the one proven way to change a room's piece after the plan is the filler ring's write: set `piece+0` after the picker `0x3eb960` has run (section 23: filler rooms set to 836, read at room load). P1 does the same for the Cave cell, writing the copy id for its exit mask.
- **Risk:** `[room+0x80]` is stored at room creation (`0x3dcd25`, a stack value; its origin was not traced). If the mask is taken before the id write, changing the id afterwards will not move the mask and the Cave cell loads the wrong files. Check 1's tile-list log settles it; the fallback is route (c), a hook on `0x3f3970` that adds the guest file for guest rooms.

Expected log: the build matches the plan (as `maze probe`), all three rooms made, no crash guard line.

Checks and the pass signals:

1. **Draws.** Screenshots of both seams: Cave floor and wall tiles visible in the Cave cell (not black, not Lava tiles, no "missing tile" holes). Also log the room's tile list (`room+0xb0`) for one Cave room and one Lava room: the Cave room lists the appended file, the Lava room does not.
2. **Walks.** Click-walk or `cheat goto` across both seams: the player crosses at the two-tile overlap and is stopped by the wall elsewhere. A step is allowed; a gap or a block in the throat fails.
3. **Automap.** Walk the strip and open the map: record which cell shows lines. Expected (inference): Lava lines yes, Cave lines no or wrong. Then hook `0x32bdf0` with the fallback and recheck.
4. **Edge.** Walk the rim (ring of fillers around the strip): no black field past a Cave cell.
5. **Multiplayer.** Host builds and a client joins: both draw the Cave room (the loader runs on both sides; client act load `0x86bc0` is not on the path). Needs the section 12 sync.
6. **Palette and lighting.** The Cave cell next to Lava in the Act 5 palette: look, note.
7. **Monsters.** Populate once: record the monsters in the Cave cell (expected: the level's one list).

P2 only after P1: a real zone plan (bands), the border rule above, then the outdoor host with Jungle + Snow.

### 8. Open questions

- Is `[room+0x80]` always the piece's `Dt1Mask`, so a lvlprest copy with a shifted mask is enough? (Section 7 check 1 settles it.)
- Does the in-memory row patch (route (b)) survive the engine's own use of the row, and is the vector's stride exactly `0x781` on every build the plugin loads on? Not signed.
- Automap: does a guest tile with no AutoMap row leave a hole or draw the host's cel? Hook design depends on the answer.
- Can the outdoor stamper hold maze pieces (walls, links)? Never tried; P1 does not need it.
- Per-zone monster lists (a per-room spawn hook) are unexplored.
- How much of a Cave versus Lava seam is visible at 24-tile pieces (art only).

### 9. Verdict

**Feasible, and cheap for same-builder pairs.** Suggested follow-up: a "Cartographer: woven tilesets, prototype P1 (Cave in Lava)" issue; a second one for the outdoor host and the automap fallback once P1 passes.
