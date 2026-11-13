# 22 Automap memory across a town round trip (issue #306)

Build 93847. Dump evidence plus a live memory probe in game (slot 5, test save, Hell, god mode). No code changed.
Nothing here is wired as a plugin part.

## Layout (RVAs from the module base)

- Per-character map memory (`<char>.map`, `.ma0`..`.ma3`): a 100-entry u32 bucket table (0x190 bytes) indexed by
  levels.txt `Layer`. Each bucket chains records `{next, layer, f1, seedHi, count1, count2, ...}`.
- A record is used on load only when its layer equals the object key and its seed hi equals the game seed.
  Seed pointer `0x2a233b8` points at `{lo, hi}`.
- `0xd5360(layerKey)`: finds or creates the layer object and activates it. Object: key `+0`, f1 `+4`, four cell lists
  at `+8`, `+0x30`, `+0x58`, `+0x80` (sizes at `+0x28`, `+0x50`, `+0x78`, `+0xa0`), next `+0xa8`. The object list head
  is read by the `mov` at `0xd536a`.
- `0xd1710`: level switch. Saves the previous current object (`0xd6230` -> `0xd5fe0`), sets the current-object global
  (`0x2a2cf68`), loads the new one (`0xd5710`).
- `0xd4400` region: reveal pass. Throttled, runs after the player moves 80+ units, calls `0xd5360` for the player's
  level layer.
- `0xd2240` / `0xd22f0`: per-level cell build (town levels 40, 103, 109 activate a layer object temporarily).
- Only the current layer object holds cells; the others are emptied after being saved.

## Observations (probe: object list sizes and current object, sampled at each step)

- Cave (slot Maze, Layer 2), god mode: leaving to town saved 441 cells; returning restored them (487 = 441 + about
  46 arrival cells). The memory survives the round trip.
- Jungle (slot Outdoor, Layer 79, shared with Act 5 town 109): sizes `[100,88,1,6]` unchanged through town and back,
  current object stayed layer 79. Shared layer, so no save or reload happens.
- Runs with a dead or very low health character: the town layer was never activated (current stayed 2), about 49
  cells were dropped when the slot level was freed, and the return showed only the arrival reveal. This matches the
  reported symptom but was not shown to happen with a healthy character.
- The earlier "Jungle needs its own Layer" theory is wrong: stock levels share layers and keep their reveal.

## Not established

- No reproduction of the loss in a healthy run, and no fix. A far-from-arrival screenshot comparison was not obtained
  (the overlay only drew cells near the player; `cheat goto` stopped moving the character after a return).
- Two game crashes at `0x1255802` (a deliberate fatal-assert stub) happened during the runs; unrelated to the
  automap as far as seen.
