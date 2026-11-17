## 50. Colour grading: where per-map hue and saturation can be reached (issue #442)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Every address is an RVA in the memory-layout dump (capstone over the dump, `.pdata` for bounds). Game files: `data/hd/env/vis/*.json` and `data/hd/env/lut/*.dds` from the toolkit's cache (`modkit fetch --prefix data/hd/env/lut/`). Evidence levels: **static** (disassembly, game files) and **in game** (a throwaway probe build on slot 10, Harrogath only; not committed). Follows note 32 (apply hook `0xf5dad0`).

### Verdict

| Route (issue #442) | Verdict |
|---|---|
| (b) the four floats at component `+0x1d0..+0x1dc` are LUT blend weights | **Ruled out** (static). They are the unit point-light settings (`unitLightPower`, `unitLightAttenuation`, `unitLightDistanceOffsetRatio`, `unitLightDistanceScaler`), read only by the unit light builder `0x74b324`. |
| (a) swap the grading LUT for a plugin-made one | **Works, and is lighter than feared** (static and in game). The renderer keeps a per-frame list of (LUT, weight) pairs; a plugin can make its own 65x65x65 LUT with the game's own default-LUT function and put it in that list. In game the scene was graded through the plugin's LUT, and a desaturated LUT lowered the colour of the picture against an identity LUT under the same weights. Not yet right: adding an entry changes the brightness too (the combine pass is not a plain normalised blend, below), so the implementation should **replace** the level's entry instead of adding one. |
| (c) something cheaper (a saturation constant, a second blend slot) | **None for the screen.** Hue and saturation controls exist only for model colour variants (`ColorTransform`, the "Universal CLUT"), not for the level's grade. The per-frame LUT list is the "second slot". |

So per-map Hue and Saturation are reachable at runtime, without mod files, by route (a) through the LUT list. It needs three new parts and one new hook; it is not a field write in the existing apply hook.

### Route (b): `+0x1d0..+0x1dc` are unit-light settings, not blend weights

- The apply function `0xf5dad0` copies, unless the byte `0x34b6dc5` is set: `+0x1d0` to `0x267c738`, `+0x1d8` to `0x267c73c`, `+0x1dc` to `0x267c740`, `+0x1d4` to `0x267c744` (`0xf5dcfe..0xf5dd3a`). In the same way (unless `0x34b6dc4` is set) `+0x40..+0x50` go to `0x267c724..0x267c734` (`0xf5dcaf..0xf5dced`).
- The only game-code reader of all nine globals is one function, chunk `0x74b324..0x74b8ef`, which fills a point light: power times `1/(4*pi)` (the float at `0x1d617f0`, 0.0795775) stored as the light's colour times power at `[rdi+0x10..+0x18]`, attenuation at `[rdi+0x30]`, radius scaled by the remaining floats. Its player branch reads `0x267c724..0x267c734`, its unit branch `0x267c738..0x267c744` (`0x74b6c3..0x74b7d4`). Nothing in the grading code reads them. A debug ImGui panel also takes their addresses (`0xf25df5` `lea` of `0x267c724` for its "Power" slider, `0xf32b6b` of `0x267c744` for "Offset Ratio"; writes at `0xf25ebb`, `0xf25edd`, `0xf26059`); debug only, so the conclusion stands.
- The vis JSON agrees by order and value: the last four fields of `VisualDataComponent`, after `gradingLutFile`, are `unitLightPower`, `unitLightAttenuation`, `unitLightDistanceOffsetRatio`, `unitLightDistanceScaler` (`act1_caves_day`: 500, 5, 0.5, 1.5; across all 369 vis files only three value sets occur). Which of the four sits at which of `+0x1d0..+0x1dc` follows the JSON order; not checked live.
- Same evidence for `+0x40..+0x50`: the JSON fields after `characterContrastPoints` are `playerUnitLightPower` (2125 in `act1_caves_day`, the value note 32 reads at `+0x40`), `playerUnitLightAttenuation` (5.5), `playerUnitLightDistanceScaler` (1.333) and `playerUnitLightContributions` (1, 0.666), and `0x267c724` is multiplied by `1/(4*pi)` like the unit light power. So `+0x40` is the **player light's power**, not a colour temperature: the Warmth column of #440 scales the warm light around the player. Follow-up: issue #530 (Warmth naming and docs); note 32 has a one-line pointer here.

### The grading path (static)

| RVA | What |
|---|---|
| component `+0x110` | `gradingLutFile` path (char pointer), component stride `0x1e8` |
| component `+0x1c8` | the loaded LUT resource (refcounted object, below) |
| `0xee8780` | the component's LUT-loaded callback. If the resource has no view (`[res+0xf8] == 0`) it logs `Unable to load grading lut: %s. Falling back to generated default.` (string `0x1e09f30`, xref `0xee8913`) and fills the resource with `0xe1ae50(renderer, res)` |
| `0xf5c71f..0xf5c776` | in `TransitionInstance` (`0xf5b450`): per active influence, if its LUT resource is ready (`byte [res+4] == 2`), `renderer = 0xe21140()`, add a reference, `0xe291f0(renderer, &res, weight)`; otherwise logs `Grading lut %s was not ready ...` (`0x1e15460`) |
| `0xe21140` | the renderer getter (a lazily made singleton at `0x34f6480`) |
| `0xe291f0` | `SubmitGradingLut(renderer, LutResource** res, float weight)` (weight in xmm2): stores `{res, weight}` in the vector at `renderer+0x139ad8` (16-byte entries, count at `+0x139ae0`), takes a reference for the list and drops the caller's. Only caller: `0xf5c776` |
| `0xe28d7b..0xe291eb` | builds the combine jobs (vector at `renderer+0x139b30`): entry 0 is A; each further entry B gives a job `{A texture, A view, B texture, B view, wA at +0x20 = sum of the weights so far, wB at +0x24, target}`, ping-ponging between `Combined LUT 0` / `Combined LUT 1` (`renderer+0x13c040 + i*0x3a8`), the last into `Final Grading LUT` (`+0x13c790`). An empty list uses the default LUT (`renderer+0x13cee0`: texture `+0x13cee8`, view `+0x13cfd8`) with weight 1. Then the list is released and cleared every frame |
| `0xe1ae50` | `CreateDefaultLut(renderer, LutResource* out)`: fills a 65x65x65 buffer of RGBA half floats with the identity (`x/64`, constant `0x1d62268` = 0.015625; red is the fastest index, alpha 1), creates a 3D texture (`[device+0x2e40]`, vtable `+0x360`), uploads it with `0xfc3c60` (the buffer is its 5th argument, call at `0xe1b0c3`), makes a view (vtable `+0x378`), stores the texture at `out+8` and the view at `out+0xf8` (also swapped into `+0xf0`), frees the buffer |
| `0xe90d70` | releases one reference of a resource (32-bit count at `+0`) |

LUT resource layout used by the combine: `+0x00` reference count (int32), `+0x04` state byte (2 = ready), `+0x08` texture, `+0xf8` shader view.

The LUT files: every `data/hd/env/lut/*.dds` is 2197128 bytes, a DDS of 65x65x65 RGBA16F (DXGI format 113), the same size and layout as the default made by `0xe1ae50`. `default.dds` (used by 361 of the 369 vis files) is an exact identity (checked at eight points); `act3_kurast_day_1.act3_day.dds` darkens the mid-greys (0.5 to 0.484). So for maps on a `default.dds` biome, a hue/saturation LUT built from the identity is exactly the grade wanted.

Strings near the path: `Combined LUT 0` / `1` (`0x1df7008`, `0x1df7018`, used at `0xe1d3d4`, `0xe1d442`), `Final Grading LUT` (`0x1df7028`), `LUT Combine` (`0x1df5f50`, `0xe22ff5`), `FINAL_LUT_COMBINE` (`0x1df3b20`, a shader define).

### Route (c): the hue and saturation controls that exist are for models

`D2Render\src\debug\debug_lut_view.cpp` (string `0x1e1be70`) has a debug panel, function chunk `0xfb2dff..0xfb334b`, with sliders `Pal Shift`, `Color Adjust`, `Hue Adjust 0 (RYG)`, `Hue Adjust 1 (CBM)`, `Saturation Adjust 0 (RYG)` (`0x1e1c050`, xref `0xfb326e`), `Saturation Adjust 1 (CBM)` (`0x1e1c088`, xref `0xfb331a`), `Color Tint`, over the transforms `Global`, `Random` and `Item Transforms` and the palette names `grey`, `brown`, `invgrey`, ... These are the fields of the per-entity `ColorTransform` component (`hueAdjustment0/1`, `satAdjustment0/1`, `colorTint`, note 32) applied through the `Universal CLUT` (`0x1df7040`, `0xe1d51e`) to models (item and monster colour variants). They do not touch the screen grade, so no cheap saturation constant was found for the level.

### In game (throwaway probe, slot 10, Harrogath at night, build 93847)

Probe (not committed): hooks on `0xe291f0` (after the game's submit, submit our LUT too, weight = the game's weight times a factor) and `0xfc3c60` (only while our call of `0xe1ae50` runs: transform the identity buffer in place with `luma + s * (Rhue * c - luma)`, Rec. 709 luma, rotation about the grey axis); our resource is a zeroed block with a reference count of 2^20. Console `cartographer grade <saturation%> <hue> <factor%>`. Log: `grade probe: upload hook 1, submit hook 1`, `LUT data transformed (274625 texels)`, `created LUT ... texture ... srv ...` (non-null), `submitted LUT (game weight 0.500)` (two influences of 0.5 each in town). No crash, no fault in the plugin log.

Mean RGB, chroma (mean of max-min per pixel) and luma over the central crop (300..1600 x 100..900) of 1920x1080 shots, same spot and character:

| Probe | Mean RGB | Chroma | Luma |
|---|---|---|---|
| off (stock), before | 38.0 / 33.4 / 29.2 | 9.34 | 34.1 |
| identity LUT, factor 100 % | 185.7 / 173.0 / 155.4 | 31.15 | 174.4 |
| saturation 0, factor 100 % | 181.2 / 173.8 / 164.0 | 17.69 | 174.7 |
| identity LUT, factor 10 % | 31.1 / 27.2 / 23.7 | 7.83 | 27.8 |
| saturation 0, factor 10 % | 30.6 / 27.2 / 24.0 | 7.01 | 27.7 |
| saturation 0, factor 1000 % | 93.6 / 94.4 / 94.3 | 1.85 | 94.2 |
| saturation 0, 250 or hue 120, factor 100000 % (first build) | 1.2 / 1.1 / 1.0 | 0.21 | 1.1 |
| off (stock), after | 37.9 / 33.4 / 29.2 | 9.28 | 34.0 |

- The scene is graded through the plugin's LUT (the HUD is not: it stayed normal on the black frame), and `off` gives the stock picture on the next frame.
- At the same weights, the desaturated LUT has much less colour than the identity LUT at the same luma (chroma 17.7 against 31.2; 7.0 against 7.8; near grey at 1000 %). So **saturation is reachable** this way; hue was only tried at the factor that went black.
- Adding an entry is not a normalised mix: an identity LUT added to an identity-based town grade changed the brightness a lot and not monotonically with the weight (factor 10 % darker, 100 % far brighter, 100000 % black). The combine shader's formula was not read. So the implementation must not add an entry.

### What an implementation would take

1. Three parts, each unique in the executable sections of build 93847 (checked by pattern scan of the dump):
   - `LutSubmit`, the function itself: `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 0F 29 74 24 20 48 81 C1 D8 9A 13 00` at `0xe291f0` (hook, 15-byte prologue with no RIP-relative operand).
   - `LutFallbackCall`, the two calls in the LUT-loaded callback: `E8 ?? ?? ?? ?? 49 8B 96 C8 01 00 00 48 8B C8 E8 ?? ?? ?? ??` at `0xee892f`; the first rel32 gives the renderer getter `0xe21140`, the second `CreateDefaultLut` `0xe1ae50`.
   - `LutUploadCall`, the upload call in `CreateDefaultLut`: `E8 ?? ?? ?? ?? 40 38 7D A0` at `0xe1b0c3`, target `0xfc3c60` (hooked, or its call patched, only to put the plugin's data in; 25-byte prologue `40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E8 48 81 EC 18 01 00 00`, no RIP-relative operand).
2. Per map look with Hue or Saturation: build the LUT once (identity times the hue/saturation matrix, in the upload) the first time the look is needed, on the thread that runs `TransitionInstance` (the probe did it inside the submit hook); keep it for the game, keyed by (hue, saturation).
3. While the player is in the map's slot level: in the `LutSubmit` hook, **replace** the game's resource by ours and keep the game's weight, so the combine sees the same weights as stock. Outside the map pass the game's resource through: the list is rebuilt every frame, so nothing is restored.
   References: `0xe291f0` stores `*rdx`, takes a reference for the list (`lock xadd` at `0xe2923b`), then releases the caller's reference with `0xe90d70([rsi])` (`0xe2924c`); the game took that reference at `0xf5c761` before the call. So a replace hook takes a reference on our LUT, passes ours, and releases the game's reference on its own LUT with `0xe90d70`; otherwise the game's LUT leaks one reference per frame.
   Lifetime: the plugin's LUT resource lives for the whole game with balanced references (one owned by the plugin, plus the list's per frame), not the probe's block with a fake count of 2^20. It is never freed on plugin unload while frames that hold it may still be in flight; leaking it at process exit is acceptable.
4. Limits: maps whose biome uses a non-identity LUT (8 of 369 vis files: Kurast day, Act 2 town, character create, lobby) lose that grade while hue/saturation is set, unless the plugin also reads that LUT's data (not done). Legacy (G key) mode does not use this path. The `active` flag and `maps.txt` columns live in `engine/map_lighting.cpp` and `maplight/*` (on main since PR #526); the Hue and Saturation columns go there, the `LutSubmit` hook can sit in its own file.
5. Not checked: the replace mode in game, a hue rotation at a sane weight, a second map on the same tileset, and whether `0xe1ae50` is safe to call on that thread under load (it ran without fault seven times in the probe).

## Implemented (issue #442)

Per-map `Hue` (degrees, -360 to 360) and `Saturation` (percent, 0 to 300) in `maps.txt`, maps only. Code: `src/lutgrade` (colour math), `src/engine/lut_grade.cpp` (hooks), `src/maplight` (the look), `tools/check_lut_grading_signatures.py` (checks the parts against a dump).

- Colour math: the Rec.709 luma is kept; the colour difference (Cb, Cr) is rotated by the hue and scaled by the saturation, as one 3x3 matrix applied to each texel of the 65x65x65 identity table, clamped to 0..1.
- Parts: `LutSubmit`, `LutFallbackCall`, `LutUploadCall` and `LutReleaseCall` (the release call at `0xe2924c`, extended with the padding and the next function's first bytes because the short form matched three sites), plus `LutUpload` (the upload at `0xfc3c60`, 25-byte hook). Any part missing turns off only hue and saturation, with a warning.
- The upload hook transforms the data only while the plugin's own `CreateDefaultLut` call runs (thread local). One table per (hue, saturation), made on first use, kept for the whole game, never freed. Its resource is a 4 KB block (reference 1, state 2) allocated by the plugin.
- The `LutSubmit` hook, while a look with a grade is active, takes a reference on the plugin's table, submits it with the game's weight, and releases the game's reference with the game's own function. Outside the map the game's table is submitted untouched.
- Checked in game (slot 10, build 93847, 2026-11-17, three Cave maps, one game each): `Hue` 120 turned the cave and the red portal green and the town portal was red again on leaving; `Saturation` 0 gave a grey picture with the HUD in colour; a map with neither was untouched and made no `lut grade` log line. Mean brightness of the three frames was 18.7, 15.1 and 17.9 (different spots; no brightness swing). Town looked normal after leaving. No renderer fault, assert or crash report came from the 4 KB resource block.
- Not checked: a long session with many different (hue, saturation) pairs, the legacy (G key) look, a biome with its own colour grade.
