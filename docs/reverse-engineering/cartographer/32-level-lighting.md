## 32. How D2R HD lights and colours a level (issue #380)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Every address is an RVA in the memory-layout dump (capstone over the dump, `.pdata` for bounds; game files read with the toolkit's CascLib reader). The first sections are static (disassembly and game files); "Live results" at the end is from a hooked game (slot test save) and supersedes the verdict where they differ.

### Data path (game files)

- A level's HD preset points at a biome: `data/hd/env/biome/<biome>.json` (e.g. `act1_caves`). The biome lists `visualDataFilenames`: six `BiomeVisualDataFile` entries, `data/hd/env/vis/<biome>_{dawn1,dawn2,day,dusk1,dusk2,night}.json` (some biomes also have `_desecrated`), plus `iblSettings` (cubemap `.ibls`) and terrain data. So maps that share a tileset share a biome and so the same vis files; there is no per-level vis.
- A vis file is a `VisualDataDefinition` with one `VisualDataComponent` entity (`vis_data`) and directional-light entities (`global_key_light`, `global_fill_light`, `global_back_light`, `player_*`, `monster_*`; each has `color` x,y,z, `power`, orientation). `VisualDataComponent` fields: `exposureValue`, `iblIntensity`, `keyLightShadowPitchDeg`, `unitLightSpecOrientation`, `characterShadow*`, `characterBrightnessFactor`, `characterContrastPoints`, `playerUnitLight*`, `itemBrightnessFactor`, wind, `bloom*`, `fog*` (`fogDiffuse`, `fogEmissive`, ...), `depthFog*`, `heightFog*`, `gradingLutFile` (a `data/hd/env/lut/*.dds` colour-grading LUT), `unitLight*`. Example `act1_caves_day`: exposure 4.0, iblIntensity 2.0, LUT `default.dds`.
- Time-of-day and events (DenOfEvil, DiabloAppeared, TaintedSun) come from `data/hd/env/weather/vis_event_map.json`. The `ColorTransform` component (`hueAdjustment0/1`, `satAdjustment0/1`, `colorTint`) is per entity (models), not per level.

### Engine path (`D2Render\src\visual_data_system.cpp`)

| RVA | What |
|---|---|
| `0xf5d1a0` | `VisualDataSystem::Update(this, float dt, FrameSettings*)` (2340 bytes). Calls TransitionInstance at `0xf5d7e3`, then the apply function at `0xf5d837` |
| `0xf5b450` | `TransitionInstance(instance, system)`: real entry (prologue `4C 8B DC 55 53 56 57 48 8D AB 08 FC FF FF 48 81 EC D8 04 00 00`, the pdata chunk at `0xf5b476` is its tail). Zeroes and re-accumulates, every frame, the live `VisualDataComponent` of the instance from all active influences (stack entries of 32 bytes, weight at +0x14/+0x18): `dst += weight * src` per float. Exposure is blended in log space (call `0x12e9770`, pow) |
| `0xf5dad0` | Apply: `(system, registry, VisualDataComponent* comp, FrameSettings* fs)` in rcx, rdx, r8, r9. Copies component fields to `FrameSettings` (and a few globals) and gathers the directional lights into it (`0xf55d60`, `0xf51f00`). Two callers (raw `E8` scan): `0xf5d837` in `Update` with the current instance's pooled component (`[system+0x140]` set), and `0xf5d9b2`, taken when `[system+0x140]` is null (`je 0xf5d841` at `0xf5d7de`) with a component built on the stack (`r8 = rbp+0x100`), so no instance. A hook sees both; it must skip the `0xf5d9b2` call (not a level's look) |
| `0xf56690` | `get<VisualDataComponent>(registry, entity)`, returns pool base + index * `0x1e8` (the component is 0x1e8 bytes) |
| `0xf3c2f0` | the instance's root entity (`[instance+0x48]`); the instance's registry is `[instance+8]`; the system's current instance is `[system+0x140]` |

Component offsets seen in the blend (floats unless noted): `+0x00` exposure, `+0x04` iblIntensity, `+0x18..0x24` a vec4 (inference: characterShadowOrientation), `+0x28`, `+0x2c`, `+0x30..0x3c` vec4 (inference: characterContrastPoints), `+0x40..0x50` (copied to globals unless a debug flag is set), `+0x54..0xe4` the bloom and fog and light-scalar groups, `+0x1d0..0x1dc` four floats (also copied to globals). The mapping of names to offsets is inference from order and is NOT yet checked against a live component.

### Verdict (research stops here; nothing validated)

A runtime path looks feasible and independent of mod files: hook the apply function `0xf5dad0` (signature and guard: the 19-byte prologue `4C 8B DC 55 41 56 41 57 49 8D 6B A1 48 81 EC 00 01 00 00`, whole instructions with no RIP-relative operand (`mov r11,rsp; push rbp; push r14; push r15; lea rbp,[r11-0x5f]; sub rsp,0x100`; the next instruction at `0xf5dae3` is the RIP-relative cookie load); it is unique in the dump, count 1, so no longer signature is needed. Where it does not match, log it and leave the part off), and change the component before calling the original (exposure, ibl, fog colours) or `FrameSettings` after it (light colours and powers). Inference, not yet checked live: because TransitionInstance rebuilds the component every frame, no restore is needed and the next frame after we stop modifying would be the stock look. TransitionInstance only runs when an instance exists, and it was not confirmed that every field is re-zeroed, so the plugin must verify this (a change made in one frame is gone in the next) before relying on it. Hue and tint would be done by rotating or multiplying the RGB of the lights and fog (the colour-grading LUT is a file and cannot be edited per map this way).

### Not done

- Names to offsets for `VisualDataComponent` and the `FrameSettings` layout (needs a logging hook and a live dump compared with the vis JSON values above, slot 7).
- Legacy (G key): `LoadActPalette` `0x9041a0` loads `data/global/palette/act<N+1>/pal.pl2` (format `%s\palette\act%d\%s` at `0x1d840e0`, xref `0x9041c7`; file name `pal.pl2` at `0x1d840f8`, xref `0x9041e0`; `LoadActPalette` is only called at the end of the load screen, so a palette edit would have to be made after that load and undone on leaving the map); not read further. A per-map hue there means editing the loaded palette after the load.
- How the plugin learns that the player is in a slot level and which `maps.txt` row it is (the existing level hooks carry this), the `maps.txt` columns, tables.md, CHANGELOG, tests.
- In-game check on two maps sharing a tileset.

### Live results (build 93847, in game)

- The hook on `0xf5dad0` (signature above, guard matched) sees the instance call (return RVA `0xf5d83c`) every frame with the level's component; the other call (`0xf5d9b2`) is skipped. The component is rebuilt by TransitionInstance each frame, so scaling a field before the original needs no restore: leaving the map (the plugin stops scaling) gives the stock look on the next frame (town after a map looked stock).
- Component (cave `act1_caves_day`, stride `0x1e8`), what a change does:
  - `+0x00` = 4.0, the exposure value. It acts inversely: a higher value is a darker picture (12 darker, 1 brighter). The plugin multiplies it by `100 / Brightness`.
  - `+0x40` = 2125, a colour temperature (1000: warmer and darker; 6500 and 20000: cooler and brighter). The plugin multiplies it by `100 / Warmth`. `+0x40..0x50` and `+0x1d0..0x1dc` are copied by the apply function to globals (not into FrameSettings), so brightness and temperature are not part of the `FrameSettings` bytes it writes.
  - `+0x44` 5.5, `+0x48` 1.333, `+0x4c` 1, `+0x50` 0.666 (not changed). `+0x18..0x28` light direction and intensity (copied to FrameSettings `+0x58..0x68`). `+0xa4..0xac` and `+0xb0..0xb8` (RGB 0.976, 0.922, 0.976) are the fog and bloom group (FrameSettings `+0x74..`); no visible effect in caves (fog density 0). `+0x7c` = 1; 0 makes the picture brighter.
- Writing floats into `FrameSettings` after the original had no visible effect.
- Two maps on the Cave tileset (one slot level), mean RGB over the central crop of a 1920x1080 screenshot: unchanged cave 28.6 / 18.7 / 15.6 (luma 20.6), Brightness 50 and Warmth 60: 12.9 / 7.0 / 6.4 (luma 8.2), Brightness 250 and Warmth 300: 47.5 / 32.9 / 26.4 (luma 35.6). The hook set each look only while the player's level was the slot level ("map look: applied" and "off" in the log).
- Not found: a hue, saturation or per-channel RGB handle in the component or the frame settings the apply function writes, so Hue, Saturation and Tint columns are not offered. Light colours (the `lights` entities of the vis JSON) are gathered elsewhere (`0xf55d60`, `0xf51f00`) and were not explored; a per-map light tint would start there.
- Legacy (G key) mode was not examined (`LoadActPalette` `0x9041a0`, above); Brightness and Warmth do nothing there.
- Only one Maze level can be built per game, so a second map on the same tileset needs a new game (a restart of the game with the same character was used).

### Warmth on its own (build 93847, in game, fix cycle 1)

Brightness blank, Warmth only, the same map row (Shorelines, Cave tileset, base temperature 2125 K), a fresh game per value, the map opened with `cartographer open`. Mean RGB over the central crop (300..1600 x 100..900) of a 1920x1080 screenshot; the layout is random per game, so the ratios are only a trend:

| Warmth | Scale on 2125 K | Mean RGB | R/B | G/R | Luma |
|---|---|---|---|---|---|
| blank (stock) | 1.0 | 25.1 / 16.3 / 14.1 | 1.78 | 0.649 | 18.0 |
| 50 | 2.0 (4250 K) | 22.1 / 15.0 / 13.1 | 1.69 | 0.679 | 16.4 |
| 300 | 1/3 (708 K, clamped to 1000 K) | 23.9 / 13.4 / 10.7 | 2.23 | 0.561 | 15.4 |
| 400 | 1/4 (531 K, clamped to 1000 K) | 22.3 / 13.4 / 10.6 | 2.10 | 0.601 | 15.1 |

Warmth above 100 raises R/B (warmer) clearly: 1.78 to 2.23 and 2.10, with G/R falling. The cooler side (Warmth 50: R/B 1.69 against 1.78) is within the noise of random layouts, so the warmer side is what settles the direction of `100 / Warmth`. The earlier pair of screenshots mixed Brightness into the ratios (tone mapping), which is why they did not show it. Before the clamp existed, Warmth 300 (708 K) was shown in the Brightness 250 and Warmth 300 map above and gave a normal picture (no black or NaN). The plugin now clamps the resulting temperature to 1000..20000 K (only 1000, 6500 and 20000 K were tried on the raw value), so 531 K was not shown unclamped. Also in this cycle: one look is kept per slot level (a map opened later on another tileset does not replace the first one's), and the per-frame readable check runs only while a look is active.
