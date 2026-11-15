## 36. A boss room reached by stairs, without a load screen (issue #475)

Build 93847 (stamp `0x6ab3782c`), D2RLoader 1.3.1-beta. Desk work only: dump disassembly (`plugins/cartographer/tools/dump_dis.py` on `D2RLoader_6ab3782c_dump.exe`), vanilla tables from `d2rAI-toolkit/d2r-data/93847`, the earlier notes. Nothing run in a game. Evidence labels: **static**, **inference**, **needs game**. Part of #19. Revisits #55 (closed: the owner then chose to keep the boss in the map; this note is for the new request of a boss room behind a level link).

### 1. Answer

- **No load screen is decided by the act, not by the kind of link.** `WarpToLevel` `0x488180` compares the target level's act with the player's: same act, it finds the arrival spot and teleports; another act, it runs the act change. Both slot levels are Act 5, so a stairs link between two of them never changes act. **Feasible** (static for the branch; the screen itself **needs game**).
- **A boss room is a second slot level joined to the map by a stairs tile**, the vanilla Cave Level 1 to Cave Level 2 pattern: the map's boss cell gets a Next piece (tile main index 1), the boss level is a preset level whose DS1 carries the matching tile back. Two levels.txt rows link through `Vis1`/`Warp1` and an existing `lvlwarp` row.
- **Far from the arrival is already in the plan.** The layout's boss cell is the end of the main path (Corridor and Open) or the ring cell opposite the entrance (Loop); the outdoor plan picks it with `pickFarthestCell`.
- **Not every tileset has stairs art.** Cave, Crypt, Ice, Jungle (two clearings), Snow have a link tile; Lava (28) and Siege have none. Those keep the boss in the map.

### 2. What decides a load screen

| Fact | Evidence | Level |
|---|---|---|
| `WarpToLevel(game, player, level, 0)`: `0x4881c2` calls `0x3e2b5c8` (level -> act) for the target; `0x488212` calls `0x484bf0` (the player's current act); `cmp r15b, al` at `0x488217`. Not equal: `0x502d00` then `0x487d20` (the act change; `0x487d20` has this one caller). Equal: `ActSpawnSpot` `0x2ef8b0` (`0x4882b9`), then `0x491ca0` (teleport) | `dump_dis.py ... dis 0x488180 100`; `knowledge/d2r-93847-addresses.md` WarpToLevel row | static |
| The client builds an act (all its levels' DRLG, the act's DT1 preload) on packet `0x03` (LoadAct): handler `0x129930` -> `0x86bc0` -> `0x2ef1c0` | [12-map-sync.md](12-map-sync.md) Q2 | static |
| `LoadActPalette` `0x9041a0` is called only at the end of the load screen | `knowledge/d2r-93847-addresses.md`; [32-level-lighting.md](32-level-lighting.md) | static |
| So the load screen belongs to an act load, and a level change inside the act does not send `0x03` | the three rows above | inference (strong) |
| Portals and warps link levels within one act only (`0x3e2b5c8`) | `knowledge/engine-limits.md`; [33-woven-tilesets.md](33-woven-tilesets.md) section 6 | static |
| The map portal (Harrogath, Act 5 -> slot, Act 5) is already a same-act move | slot rows are Act 5 (`cartographer-outdoor-builder.md` "The slot row") | static |
| The server path of a stairs click (tile unit operate) was not traced. Callers of `WarpToLevel`: `0x4025c4`, `0x4028fe`, `0x410283`, `0x4b6620`, `0x4f2f7a`, `0x548c99`, `0x549a2d`, `0x58f081`, `0x58f1a2`, `0x5934ae`, `0x5e5eed` (E8/E9 scan); which one is the tile path is open. Vanilla links levels by stairs only inside an act | xref scan of the dump; levels.txt | static (list), inference (path) |

A seamless outdoor border (River of Flame 107 to Chaos Sanctuary 108: `Warp` -1 both sides) and a same-level teleport are the other no-load-screen moves. The owner asked for an entrance like stairs, so this note uses the stairs tile.

### 3. How a stairs tile links two levels

| Fact | Evidence | Level |
|---|---|---|
| A wall tile of orientation 10 (or 11) is a link: the tile switch `0x3f332c` sends it to `0x3f4670` with `k = (tile >> 20) & 0x3f`; `k` picks `Warp[k]` of the level's row (`0x3daad0`, row `+0x68 + k*4`, or a DRLG override list), then the `lvlwarp` row with that `Id` and direction `l`/`r` (`0x32c980`) | [20-warpdefs-assert.md](20-warpdefs-assert.md), [35-map-link-tiles.md](35-map-link-tiles.md) section 1 | static |
| `Warp[k]` = -1 gives no link (the lookup asserts once and returns NULL; the tile is dead). The installer writes `Warp0..7` = -1 and `Vis0..7` = 0 on every slot row (0 for Warp crashed, -1 for Vis crashed) | note 20; `plugins/cartographer/tools/install.ps1:9-16`, `:392`, `:489` | static |
| Cave pieces: Prev 83-86 tile 10/0/21 (k 0), Next 87-90 10/1/21 (k 1), Down 91-94 10/4/4 (k 4), Treasure 103-107 10/1/21 (k 1). All are 24 x 24 dead ends (`CaveWNext1.ds1` ...) | note 35 section 2; `lvlprest.txt` rows 83-107 | static |
| Vanilla Cave Level 2 (`Act 1 - Cave 2 Treasure`, Id 13) is a preset level: `DrlgType` 2, `Vis1` 9, `Warp1` 4 (`Act 1 Cave Up`, `Direction` b). Its DS1 is lvlprest 104 (`LevelId` 13, `CaveRoom2.ds1`) | `levels.txt` row 13; `lvlprest.txt` row 104; `lvlwarp.txt` row 4 | static |
| Cave Level 1 (Id 9) has `Vis1` 0 and `Warp1` -1, yet it links down to 13 in play: the down link comes from level 13's side or from the override list in `0x3daad0` | `levels.txt` row 9 | static (table), inference (mechanism) |
| Other tilesets' link tiles: Crypt Prev 139-142 / Next 143-146; Ice Prev 1018-1021 / Next 1022-1025 / Down 1026-1029; Jungle `spid*2.ds1` (10/1/17) and `pyg*2.ds1` (10/1/0); Snow 983-986 (Barricade From/To Cave). Lava (28) and Siege: none | note 35 section 2 | static |

### 4. What the plugin does today

| Part | Where | Level |
|---|---|---|
| A map has an entrance and no exit. `SpecialHook` (`plugins/cartographer/src/engine/maze_hooks.cpp:676`) suppresses every Prev, Next and Down call of a slot level, steered or not (`maze/steer.cpp:134`, #460) | plan `cartographer-plugin.md` "Special rooms"; note 35 section 4 | static + game check in #467 |
| The entrance is an ordinary mask room marked by `MarkArrival` (no Prev piece, #455); every outdoor arrival room has a free spot (#456, #472) | note 35; `outdoor/outdoor_plan.h:160-167` | static + game |
| The boss cell: `layout.cpp:110-126` `reserveRooms`, `bossIdx = n - 1` (path end) or `n / 2` on a ring (opposite the entrance). Outdoor: `pickFarthestCell` (`outdoor/outdoor_plan.h:129-132`) | code | static |
| The Boss ladder: plan boss room, its cell, every other room as it populates, then a forced spawn; #463 (open) makes every rung the real Boss | note 34; `engine/event_apply.cpp:654-774`; `eventplan/eventplan.cpp:316` `roomsFarthestFirst` | static + game |
| A room's piece can be changed after the picker by writing `piece+0` (filler ring; woven probe), and the room's DT1 mask follows the id | [23-maze-lava.md](23-maze-lava.md); note 33 section 10 (e) | live |
| Per-map writes of a levels row and region record (monster level, density, uniques) | `engine/level_row.cpp`, `levelrow/levelrow.h:1-9` | static + game |

### 5. Design

1. **Data (installer).** Per tileset with link art, one more slot level, Act 5: `Cartographer <Tileset> Boss`, `DrlgType` 2 (preset), the map tileset's `LevelType`, `Vis1` = the map slot's id, `Warp1` = the tileset's vanilla Up row (Cave 4, Crypt 8, Ice 73). One lvlprest copy of a vanilla treasure or small room with `LevelId` = the new level (Cave: a copy of 104). The map slot row gets `Vis1` = the boss level and `Warp1` = the vanilla Down/Next row (Cave 5, Crypt 9, Ice 74; which row a vanilla Next tile uses is **inference**, since level 9's own `Warp1` is -1). No new `lvlwarp` rows. `slots.txt` gets a `<Slot>Boss` row per tileset.
2. **Map level: one stairs room.** The plan's boss cell (already path-farthest) becomes the warp cell. The cell must be a dead end with one exit, as the Next pieces are. Corridor and Open paths end in one; a Loop's boss cell is on the ring, so the plan adds one dead-end cell off it. After the picker, write `NextBase + dir` to that room's `piece+0` (the proven filler-ring write). `SpecialHook` keeps suppressing the game's own Next/Down calls, so fallback builds still have no stairs (#460 holds).
3. **Boss level.** Preset, built by the game (no plan, no sync needed: the DS1 and seed are the same on host and client). The Boss ladder runs there instead of the map's boss room; the map's event room stays in the map. The level row and region record get the same per-map writes as the map slot.
4. **Fallback.** When the warp cannot be placed (Lava, Siege, an unsteered build, a fallen-back plan), the Boss stays in the map's boss room, as today. The Boss always spawns somewhere (#463).
5. **Leaving.** The boss level's stairs lead back to the map's stairs. Town portal works from both levels.

### 6. Open questions (needs game unless noted)

- Does the stairs tile in a room whose `piece+0` was written after the picker get its link? `0x3f4670` runs per tile at room load, but the room warp-bit loop `0x361750` (from `0x3608a0`) may need flags set by `0x3eb850` placing the piece. If not, steer one Next call into the warp cell instead (the #48 call-order blocker, note in #55).
- Does a same-act stairs move show no load screen in D2R? Check a vanilla Act 5 Ice cave stairs, then the prototype.
- Where does the arrival land in the boss level: the matching Up tile, by `lvlwarp` `ExitWalkX/Y`? And does the override list in `0x3daad0` (static, not read) touch slot ids?
- Map reset per game: the boss level must be rebuilt and cleared with the map (automap reset, region record, `Layer` bucket < 100).
- Jungle and Snow: pinning the link file of a clearing in the boss cell (the reverse of `PickedCell::linkSafe`), and which boss preset each uses.
- Which tile-operate function calls `WarpToLevel` (list in section 2).
