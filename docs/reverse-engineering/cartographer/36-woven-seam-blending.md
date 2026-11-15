## 36. Blending two woven tilesets at their seam (research, issue #458)

Game files: build 93847 (`d2rAI-toolkit/d2r-data/93847`, `dataversionbuild.txt` 93847), read with `plugins/cartographer/tools/d2r_readers.py` and scratch scripts (nothing from the game is committed). Engine: only string reads of the installed `D2R.exe` (file version 3.4.95032); no new addresses, hooks or signatures. Desk work only: nothing was run in a game. Evidence labels: **static** (file or table read), **inference**, **needs game**. Builds on note 33 (woven tilesets) and note 32 (HD lighting, #380).

### 1. Answer

**Yes, in HD, with data only: give the guest piece's seam tiles the host's HD biome.** What the player sees at a woven seam is not DT1 art but the two rooms' HD presets: each DS1 has its own preset JSON with its own terrain and its own biome (`act1_caves` for Cave pieces, `act4_lava` for Lava pieces), so the HD terrain changes biome exactly at the room edge. Vanilla softens its own tileset borders the same way it can be done here: a preset lists `perTileBiomeOverrides` (tile x, tile y, biome), and the border tiles take the neighbour's biome; stamps and decals can paint another biome's terrain layers; a `VisualDataBox` switches the lighting inside a volume. The DT1 routes (transition tiles, seam rooms, tile picking) only change legacy (G key) graphics and have no Cave-to-Lava art to use.

| Route | Verdict | Cost |
|---|---|---|
| 1. DT1 transition tiles | **No.** No vanilla DT1 bridges Cave and Lava; vanilla tileset changes are authored DS1 art or a warp, and DT1 art is not drawn in HD anyway | New DT1 art, legacy mode only |
| 2. Seam room / ring of seam tiles (lvlprest + DS1) | **Possible, not worth it alone.** A room may load both tilesets' files (mask over the widened row), but the room still needs new DS1 art and its own HD preset, and it adds a cell to the layout | High: new DS1 + preset per seam piece |
| 3. HD rendering (biomes, stamps, lights) | **Yes (recommended).** HD terrain is a layered, height and noise blended terrain; per-tile biome overrides, cross-biome stamps and vis boxes are vanilla data used at vanilla borders | Medium, data only: seam copies of guest DS1s with edited presets; no hook |
| 4. Orientation and tile picking | **No engine rule** inside a maze room; seam direction only decides which preset edge (x or y) gets the override. The neighbour rule belongs in Cartographer's own plan (the piece-id write it already does) | Low, part of route 3 |

### 2. Route 1: transition tiles in vanilla DT1s

Tile keys `(orientation, main index)`, counted over each file (**static**, scratch script over the DT1 tile headers: orientation at +20, main at +24, sub at +28 of each 96-byte tile):

| DT1 | Tiles | Floor (orientation 0) mains | Wall mains (orientations) |
|---|---|---|---|
| `act1/caves/cave.dt1` | 212 | 0, 1, 2, 3, 4, 5, 21 (196 tiles) | 0 (o1, o2, o12) |
| `act4/lava/floor.dt1` | 494 (80 animated) | 20 (the lava itself, 80, animated), 22..29 | none |
| `act4/lava/floornew.dt1` | 198 | 37, 38, 39 | none |
| `act4/diab/floor.dt1` | 295 | 30..34, 40 | none |
| `act4/lava/walls.dt1`, `extwalls`, `intwalls`, `rocks` | 44, 29, 30, 22 | none | 20, 21, 22, 23 (o1, o2, o3, o4, o7, o12) |
| `expansion/icecave/interior.dt1` | 288 | 1..7 | 0..3 (o1..o7, o10..o12) |

- **No shared or transition keys between Cave and Lava**: the main index ranges do not overlap, and no DT1 holds art of both. The two tilesets never meet in vanilla (Act 1 caves, Act 4 River of Flame). In a woven room the keys resolve only against the room's own files anyway (note 33 section 2), so no tile of one set can be borrowed by the other room.
- **How vanilla changes tileset inside a level: it lists the other set's DT1s in its LevelType row and draws the border in one authored DS1.** LvlTypes rows: Courtyard (6) lists `Act1/Barracks/floor.dt1`, `basewall.dt1`, `barset.dt1`, `exitdn.dt1`, `objects.dt1`; Barracks (7) lists `Act1/Court/Outwall.dt1` and `Floor.dt1`; Wilderness (2) lists `Act1/Caves/Cavedr.dt1` (cave doors) and `Act1/Graveyard/graveyrd.dt1`; Barricade (31) lists `Wildtemple/Entrance`, `MountainTop/floor`, `Entrance`, `Columns`, `IceCave/Exterior`, `Warps`, `Ruins/Walls`, `Furnish`; Lava (28) lists `Act4/Diab/Floor`, `Walls`, `Bridge`; Act 5 Lava (35) is row 28 plus `Act4/Expansion/Specials.dt1`. **static**.
- **The vanilla border is a hard line in the DT1 art too.** `act1/barracks/courtwb.ds1` (11 x 15, the courtyard-to-barracks gate): 98 barracks floor cells (main 0) and one straight column of 12 court floor cells (mains 1, 10) at the east edge, no transition tile between them. **static**.
- **Floor layering is the one classic softening tool.** Every Act 4 Lava DS1 has two floor layers: layer 0 is lava (floor.dt1 main 20) under the whole room, layer 1 the ground on top, mixing `Lava/Floor` 22..29, `Floornew` 37..39 and `Diab/Floor` 30/40 in one room (e.g. `lavaew.ds1`: 4 wall layers, 2 floor layers; `lavase.ds1` layer 1: Lava 22/23/25, Floornew 37/39, Diab 40). Cave pieces have one floor layer (`caveew.ds1`: 1 wall layer, 1 floor layer). A blended seam in classic graphics would need new Cave-over-lava edge art in that second layer. **static** (layers), **inference** (art need).
- **Act 5 lava and ice edges**: Act 5's lava levels (Hell 1-3, Pandemonium 3, LevelType 35) are Act 4 Lava art; the Ice Caves (33) are separate levels reached through Barricade entrance pieces drawn with `IceCave/Exterior.dt1`. No vanilla level shows lava next to ice; the change is at a warp. **static**.
- **HD does not draw DT1 art.** A DS1 without an HD preset plays on flat ground with invisible walls: DS1 and DT1 give layout and collision only (`d2rAI-toolkit/knowledge/hd-graphics.md`, "Presets for new DS1 levels"). So DT1 transition art would only change legacy mode. **static** (toolkit, live there).

Verdict: no. Cost if wanted for legacy mode: new DT1 transition art plus seam DS1s.

### 3. Route 2: seam pieces (lvlprest, DS1)

- **The data side works.** A room loads every file its mask selects from the level's LvlTypes row (room loader, note 33 section 2, signature there). With Cave appended at index 11 of the Lava row (P1, note 33 section 10), a seam piece with mask `0xfff` loads all 11 Lava files and `Cave.dt1`: one room can draw both tilesets' tiles. **static** + P1 **live** for the guest mask `0x800`; mask `0xfff` itself **needs game**.
- **Limits (facts only).** A LvlTypes row has 32 file slots (row `0x781` bytes = 32 names of `0x3c` bytes plus the Act byte); a room's mask is 32 bits; the room loader then adds 3 fixed files (Blank, InvisWal, Warp). Cave plus Lava uses 12 of the 32 slots. (Note 33 section 2.)
- **What it costs.** A seam piece needs new DS1 art (no vanilla DS1 mixes Cave and Lava), and that DS1 still needs its own HD preset, or it draws flat ground in HD (section 2, last point). A separate seam room also adds a cell between zones, so the plan's cell grid changes. Every blend it gives in HD comes from its preset, which is route 3.

Verdict: possible, high cost; only worth it combined with route 3, and route 3 does not need it.

### 4. Route 3: HD rendering

What the HD side is (**static**, the presets and biomes under `data/hd/env`, 2100 presets scanned):

- **Terrain is a layered splat terrain.** A biome's `terrainDataUltra` / `terrainDataHigh` list `TerrainLayer`s (Albedo, Normal, ORM, Noise texture, `HeightBlendThreshold`, `NoiseScale`, `HeightBias`, grass and clutter); `terrainModelBlendSettings` (`heightRange`, `noiseFrequency`, `noiseScale` per quality) blend models into the ground. Each preset's terrain entity has its own `terrain.model` (e.g. `preset/act1/caves/caveEW/terrain.model`) and `terrainBlendMode` 1.
- **Stamps and decals paint layers.** `TerrainStampDefinitionComponent` (a mask texture whose R, G, B channels set `layerIndexR/G/B` of a named `biomeName`, strengths `stomp`, `stompG`, `stompB`) and `TerrainDecalDefinitionComponent` (albedo, normal, orm, `biomeName`, `layerIndex`). `caveew.json` has 7 stamps and 4 decals, all `act1_caves`.
- **A preset can use another biome's layers.** Cross-biome components in vanilla (own biome, used biome, kind, count): court_to_barracks, act1_barracks, decal 51, stamp 13; expansion_ruins, expansion_wildtemple, stamp 16, decal 4; act3_kurast, act3_travincal_outdoors, decal 14, stamp 3; act3_temple, act3_jungle, stamp 5, decal 5; act4_mesa, act4_fort, stamp 3; expansion_ruins, expansion_siege, stamp 1.
- **Per-tile biome overrides at vanilla borders.** The preset key `perTileBiomeOverrides` is a list of `TileBiomeOverride {tileX, tileY, biomeFilename}`; 41 of 2100 presets use it, every one with a named biome at a tileset border:

| Preset (own biome) | Override biome | Tiles |
|---|---|---|
| `expansion/town/townwest` (expansion_town) | expansion_siege | 14: x 0..1, y 15..21, a 2-tile strip on the west edge (Harrogath's gate to the Bloody Foothills) |
| `expansion/siege/stripseigebar` (expansion_siege) | expansion_ruins | 26: x 0..1, y 14..26, a 2-tile strip on the west edge |
| `act3/jungle/transl`, `transu` (act3_jungle) | act3_docktown | 7 each: x 27..29, y 29..31 |
| `act3/docktown/docktown3` (act3_docktown) | act3_jungle | 17: x 41..46, y 30..33 |
| `act4/fort/fortress` (act4_mesa) | act4_fort | 600 |
| `act3/kurast/*` (act3_kurast, 24 presets) and `act3/travincal/*` (act3_travincal_outdoors, 4) | each other's biome | 1 to 232 each |
| `act1/court` (act1_court), `act1/monastry` (act1_outdoors), `act2/town` (act2_town, 2) | act1_barracks, act1_court, act2_outdoors | not counted |
| `act4/expansion/lavas` (act4_lava), 2 `act3/kurast` presets | `""` (empty name) | `lavas`: 55, x 15..21, y 1..8 (what an empty biome draws is not known) |

- **A dedicated border biome exists**: `act1_court_to_barracks` (10 terrain layers, mixing barracks, tomb and catacomb textures) is the biome of the three gate pieces `preset/act1/barracks/courtwb`, `courtnb`, `courteb`. So vanilla also softens a border by giving the border piece a mixed biome.
- **Lighting per volume.** `VisualDataBoxDefinitionComponent` (`min`, `max` box, a vis `filename`, `priority`, `enableInsideLighting`, `enableGlobalLights`, `enableIBL`) is in 67 presets; `courtwb` has a box switching to `vis/act1_barracks_day.json`. Lava presets carry their own lights (`lavaew.json`: 12 point lights, 20 spot lights, e.g. `act4_diab_lava_fill_point02`, red, power 5000, radius 35); `caveew.json` has none. Note 32's `TransitionInstance` blends the live vis component from several weighted influences every frame, which fits soft box edges (**inference**).
- **The game reads all of it.** Strings present in `D2R.exe` 3.4.95032: `perTileBiomeOverrides`, `biomeFilename`, `TerrainStampDefinitionComponent`, `layerIndexR`, `stompG`, `HeightBlendThreshold`, `tileMaskFilename`, `TileMaskOptions`, `VisualDataBoxDefinitionComponent`, `enableInsideLighting`, `inTransitionTime`, `outTransitionTime`, `Stamp with no terrain biome`, and `Exceeded the maximum number of unique clutter types for all visible biomes` (several biomes are drawn at once). **static** (string presence only; no code traced).
- **#380 Brightness and Warmth** change the whole map's look (note 32: the apply hook works on the one live vis component), so they cannot soften a line between two rooms; they can only bring the two sides' overall exposure closer. **static** (note 32).

What is not known: whether a biome change between two tiles of one terrain is drawn soft (the layers' height and noise blend) or as a hard tile edge, and whether two rooms' terrains blend across the room edge at all. Vanilla always moves the biome change inside one preset (the 2-tile strips), which fits "inside one terrain it looks right" but proves neither. **needs game**.

Verdict: yes, data only. Cost: medium (route in section 6).

### 5. Route 4: orientation and tile picking

- **No neighbour-aware pick in a maze room.** A DS1 cell names one key `(orientation, main, sub)`; the engine takes a tile of that key from the room's files, choosing among duplicates by rarity (comment in `d2r_readers.layout`). The maze pieces are placed whole, cell for cell. **static** (reader), **inference** (no other rule on the maze path).
- **The vanilla neighbour rule is LvlSub, outdoor only.** `lvlsub.txt` rows (`Type`, `File`, `CheckAll`, `BordType`, `GridSize`, `Dt1Mask`, `Prob`, `Trials`, `Max`): Act 1 `Border - Cliffs / Middle / Corner / Border` (rules DS1s `BorderCliffs.ds1` etc.) and the Barricade rules `4x3barricade_rules.ds1` match tile patterns and substitute; the rest scatter decoration. It runs through D2RCore's TileSub callback (`docs/cartographer/plans/cartographer-outdoor-builder.md`), and the Lava slot's SubType is -1 (`docs/cartographer/tilesets/a4-lava.md`), so it never runs on a woven Lava map. It also changes DT1 cells only, so legacy mode only. **static**.
- **Seam direction.** In the isometric view a W/E seam and an N/S seam run along different screen diagonals, and the wall at the seam is a left wall (orientation 1) on one and a right wall (orientation 2) on the other (Cave has 2 of each, Lava's walls 18 and 14). What HD draws is the preset either way; the direction only decides which preset edge gets the override: tiles `x = 0..1` / `x = w-2..w-1` for a W/E join, `y = 0..1` / `y = h-2..h-1` for an N/S join. **static** + **inference**.
- **The adjacent-tile rule Cartographer needs is its own.** The plan knows, per guest cell, which neighbours are host cells; the piece-id write it already does after the picker (note 33 section 10, `maze_hooks.cpp`) can pick the seam variant for exactly those sides.

Verdict: no engine rule; low cost as part of route 3.

### 6. Recommended route: host biome on the guest's seam tiles (HD preset data)

Mechanism: guest pieces are already copies (lvlprest rows 1092..1106 in P1). Give each copy its own DS1 name, so it gets its own HD preset (the preset path follows the DS1 path), and make that preset the vanilla one plus, on the sides that face a host cell, a 2-tile `perTileBiomeOverrides` strip with the host biome (`data/hd/env/biome/act4_lava.json`), optionally a few host stamps and a host point light near the seam. Vanilla Cave and Lava presets stay untouched; nothing changes the engine.

Follow-up steps:

1. **Game check of the mechanism (one issue, test slot, test mod only).** On the P1 setup (note 33 section 10), add in the test mod a copy of `preset/act1/caves/caveew.json` with a 2-tile override strip with `act4_lava` on its W and E edges, next to a copy of `caveew.ds1` under a new name that the copy row 1094 points to. Screenshots of both seams with and without the strip, plus a vanilla control (Harrogath's west gate, `townwest`). Settles: does the override draw in a woven room, is the change soft or a tile edge, does it cross the room edge. If the edge stays hard, try a host stamp (an `act4_lava` `TerrainStampDefinitionComponent` along the strip) and a `VisualDataBox`.
2. **Variant set.** One seam variant per guest exit mask and host-facing subset of its open sides (Cave: 15 masks, 65 non-empty subsets, times the 2 DS1 files per row), or fewer: start with the straight-through masks 3 and 12 P1 uses. Each variant is a lvlprest copy row (same `Dt1Mask`, `File1..2` pointing at the renamed DS1s).
3. **Generation, not committed assets.** A tool (catalogue style) writes the renamed DS1 copies and the edited preset JSON from the player's own game files at install time (or the plugin serves them through D2RLoader's ResourceService, which Cartographer already uses); vanilla-derived files never go into the repository.
4. **Plan rule.** In the woven piece-id write, choose the variant whose seam sides are exactly the cell's host-facing sides; a guest-guest side gets no strip.
5. **Pairs beyond P1.** The same per pair (guest biome strip with the host's biome); a pair's biomes come from the two presets' `biomeFilename`. Legacy mode keeps the hard line (route 1), stated in the tileset docs.

### 7. Not verified

- Everything in section 4's last paragraph: soft or hard biome change, across a room edge or not (step 1 above).
- That a renamed DS1 copy gets its preset by the mirrored path, and that the mod's preset JSON is read over the game's (both how vanilla and mods work per the toolkit; not tried in a Cartographer map).
- What an empty `biomeFilename` override draws (`lavas`).
- Mask `0xfff` (both tilesets in one room) in game.
- Multiplayer: presets are client side; a client with the same files should draw the same, not checked.
