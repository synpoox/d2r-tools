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
- Game crashes at `0x1255802`: see the next section. Cause not established; no evidence that Cartographer or the probe causes them.

## Crash at `0x1255802` (issue #328)

Build 93847, slot 5. Read-only look at `d2rloader/crashes`, the logs and `D2R.exe`; no game was started.

- Three last-resort reports hold it (start times UTC 2026-11-07 17:15, 2026-11-13 01:06 and 01:15). Each is a first-chance
  access violation, a write to address 0 (`[00]=1`, `[01]=0`), `RIP` at `0x1255802` inside the function
  `0x1255790..0x125581a` (138 bytes, from the plaintext `.pdata`). A null write from a tiny function is a deliberate
  fail-fast, as the issue guessed.
- All three have the same call chain: return addresses in `0x12f4a49..0x12f60ca` (5761 bytes), `0x12e4e80..0x12e57e0`,
  `0x12fa4b5..0x12fa895`, `0x12da1f0..0x12da5d5` and `0x139a87c..0x139abc7`, and the same registers (`RCX=RBX=0x16`,
  `RDX=0x141c783d0`, `RDI=0x1435b4d70`). Only heap pointers differ. So it is one deterministic path, not memory damage.
- Not the probe: the first report (11-07) is five days before the #306 runs. It was a different run (events, tomb and
  portal screenshots in `runs/agent-ms8b-events`), with no layer-object probe.
- Not shown to be Cartographer: it was seen only in slot 5, which also ran many other plugins (logs for
  `charm-inv`, `ruffneckk-*`, `pyre-lootfilter`, `w6-give` and others are kept), and the unit under the assert is unknown.
  The 11-13 crashes came 7.7 and 5.1 minutes into a game, in the middle of the #306 round trips.
- The assert text and the caller's purpose are **not recorded**. The loader's full report (with "Most Recent Session
  Assert") was not written for these three (last-resort report only), the per-run `d2rloader.log` and
  `cartographer.log` were overwritten by later runs, and the code on disk is encrypted, so the function and its
  string at `0x1c783d0` cannot be read statically. None of these addresses is in the plugins or the other notes.
- Other last-resort reports in the folder fault at other addresses (mostly reads); only `0x12d3907` and `0x140d9e8e7` are
  also writes to address 0, one report each.

To settle it (needs a game): reproduce in a slot with only Cartographer, then with Cartographer off, same save and
steps; keep `d2rloader.log` and the full crash report; read the string at `0x1c783d0` and the caller in a live dump.
