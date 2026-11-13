## 32. How D2R HD lights and colours a level (issue #380)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Every address is an RVA in the memory-layout dump (capstone over the dump, `.pdata` for bounds; game files read with the toolkit's CascLib reader). Static only: no game was started, nothing is hooked, no plugin code changed. Everything is from disassembly or game files unless marked inference.

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
