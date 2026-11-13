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

## Cause and fix of the lost reveal (issue #306; build 93847, live)

- The layer save (`0xd6230` -> `0xd5fe0`) writes each cell's x and y as 16-bit words and the restore (`0xd5710`, read
  from the disassembly) sign-extends them back and sets the cell type to 1. A cell's x is `(tx - ty) * 8` and its y
  `(tx + ty) * 4` (tile coordinates). The Maze level sat at OffsetX 6000, OffsetY 0, so x was about 48000, past 32767:
  restored cells came back 65536 too small and drew far off screen. The cell count was unchanged (655 before and after
  the trip in the probe), which is why the earlier runs saw "restored" and nothing on the map.
- A swap only happens when the layer differs from the one being left: Cave (Layer 2) against Harrogath (Layer 79)
  swaps; the Jungle slot (Layer 79, shared with the Act 5 town) never does, so it was never affected.
- The fix keeps the whole level inside the 16-bit range: the installer puts the Maze level (360 x 360) at OffsetX
  3500, OffsetY 0 (tx - ty stays within 3141..3859, tx + ty under 4220), and moves an older install's
  row that is out of range. Live check, Tunneling Depths in slot 7: revealed walls far from the arrival were still
  drawn after `cheat goto 109` and the red portal back, and more were added on top. Limit: any level origin where
  `(tx - ty)` leaves -4096..4095 or `tx + ty` reaches 8192 loses its map the same way. The installer checks it for the tilesets flagged `Range` (the Maze level and Snow, whose layers are not the town's 79; Jungle and Siege share 79 and are never swapped). The flag is per tileset, not read from the written row's Layer, moves an older install's row (Snow was at 6000,3600, out of range) and refuses an out-of-range `-<Param>OffsetX/Y`.

## Stale reveal on a new map (issue #306 review; build 93847)

- The map memory is kept per character, per layer and per game seed (`0xd5710` reads a layer's cells back from the
  `.map` file when that layer becomes current; `0xd5fe0` writes them on leaving). A Cave slot map shares Layer 2 and a
  Crypt slot map Layer 10 with a game level, and a single-player seed carries over, so map A's revealed walls were
  drawn on map B's different maze in the next game. Seen live (slot 7, Cave then Crypt maps), before the fix.
- Fix: part `AutomapRestore` (signature in `part_signatures.h`, 17 bytes hooked: three whole instructions, `push rbp`,
  `lea rbp,[rsp-210h]`, `sub rsp,310h`). The function reads its state from globals but its first call (`0xd5450`, the map
  file path) uses `rcx` as the caller left it, so it must be treated as taking the four argument registers: a hook with
  no arguments that logged before calling the original made `0xd5450` fault (seen live, waypoint trips). The hook passes
  `rcx`, `rdx`, `r8`, `r9` and the result straight on. The build of a slot map (the Maze build, and the outdoor build for
  Snow; Jungle and Siege share the town's layer 79, so nothing is skipped for them) arms one skip. The map is built when
  its portal opens, not when the player enters, so the skip lands only on the read made while the player's tick says
  the player stands in the slot level (a trip to another area between opening and entering reads its cells as usual) and
  another layer than the one left is current (current layer object read through the `cmp qword ptr [rip+x],0` at +0x22;
  key = first dword). Building the same map again in the same game (the return from a town trip rebuilds it from the same
  plan) arms nothing. Live (slot 5): a waypoint trip and town trips before entering kept their reads (layers 27 and 79,
  log lines "read of layer N kept"); entering skipped the read of layer 2 (Cave) with the player in level 138 and layer 84
  (Snow) in level 140, in two games in a row; a town trip after the skip and the return showed the map's own reveal
  again, no second skip.
- Cost: the skipped read also drops the stock level's reveal on that layer for that seed (Cave Level 1 shares Layer 2,
  the Crypt template Layer 10, Barricade Snow Layer 84): the leave writes only the new map's cells. A follow-up could let
  the read run and then drop only the cells inside the slot's rectangle.
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
