# Testing MultiSpec

Only in a toolkit game slot (`../d2rAI-toolkit/testing/`, docs/testing.md there), never in the owner's own install.
Back up the test character first (`.d2s`, `.d2rl`, `.multispec`). Phase 3 turns these rows into named checks.

## Phase 2: first slot runs (in this order)

| # | What | How | Expected |
|---|---|---|---|
| 1 | Loads | Start the slot, read `d2rloader\logs\multispec.log` (global plugin; the first line gives the DLL path) | `image 0x6ab3782c`, `specswap on: entry bytes ok: 23 functions`, `speccmd on` |
| 2 | Console route | In game, D2RLoader console: `spec` | Spec list for the character. Then type `/spec` in the **chat box** and note whether it is handled (design section 9, check 1) |
| 3 | Client pointer | Same `spec` | No "cannot find your character" line. If it appears, copy the log line: the console's client is not the server client (check 2) |
| 4 | File | After 3 | `<save folder>\<name>.multispec` exists; the log says "next to the save" (not FALLBACK) |
| 5 | Swap | Allocate a few points; in town `spec 2` | "spec 2 loaded (0 points, N unspent)"; skill tree shows all points unspent; spend some |
| 6 | Swap back | `spec 1` | Spec 1's exact levels, unspent as before; skill tree, hotbar and passives right (check 3) |
| 6b | Mouse skills | Pick a left and a right skill in each spec, swap back and forth; repeat with an aura on the right | Each spec gets its two skills back and the aura runs. Note what the second weapon set shows |
| 7 | Save/load | Save and exit, reload the character, `spec` | Same active spec and levels; character still listed in the menu |
| 8 | Refused outside town | Leave town, `spec 2` | "specs can only be changed in town"; nothing changes |
| 9 | Unspent preserved | Level up or use a skill point source in spec 1, then `spec 2` | The new point shows as unspent in spec 2 |
| 10 | Summons | With a summoner's spec active and pets out, `spec 2` (pets' skill drops to 0) | Note what happens to the pets (check 3) |
| 11 | Status | `multispec` | Counters match what you did |

## Phase 4 spike 4: skill tree open and close messages (design section 11)

Part `uiprobe`, console command `msprobe`. In a game slot, in game, D2RLoader console open. Run the rows in order;
the plugin log (`multispec.log`) keeps every `uiprobe #N` line, so the console can scroll away.

| # | What | How | Expected / note |
|---|---|---|---|
| P1 | Probe loaded | Read `multispec.log` after start | `uiprobe on: msprobe (listener idle until msprobe on; widgets yes)` |
| P2 | Start logging | `msprobe on` | `uiprobe: logging UI messages` |
| P3 | Open with the key | Close the console, press `T` | Note every `uiprobe` line that appears (or none: then the key opens the tree without a UI message) |
| P4 | Close with the key | `T` again | Note the lines |
| P5 | Close button | `T`, then click the tree's X | Expect `SkillsTreePanelMessage` `Close` |
| P6 | Escape | `T`, then Escape | Note the lines |
| P7 | Another right panel | `T`, then `I` (inventory) or `Q` (quests) | Note whether the tree's close shows up |
| P8 | Hotbar button | Click the HUD's skill tree button (if your HUD has one) | Note the lines |
| P9 | Plugin opens it | Tree closed: `msprobe toggle`; then again with it open | A `result 0` line; note whether the tree opened and closed |
| P10 | Find by name | `msprobe find` with the tree closed, then open | Note the result and rect both times (decides whether the plugin can tell "open" by looking) |
| P11 | Stop | `msprobe off` | `uiprobe: stopped` |

Then send the `uiprobe` lines from the log.
## Phase 4 spike 5: a test panel docked to the skill tree (design section 11)

Test panel `multispec/ProbePanel`: a strip 170 px wide (HD units) against the tree's left edge, with the word
"Specs" and three stock close-button icons standing in for spec slots. Copy the new DLL in with the game closed.
D2RLoader console open; all results are also in `multispec.log`.

| # | What | How | Expected / note |
|---|---|---|---|
| D1 | Panel registered | Log line `uiprobe on: ...` | Ends `probe panel multispec/ProbePanel` (else the reason is there) |
| D2 | Opens alone | Tree closed: `msprobe panel` | `result 0`. Screenshot: where the strip is and what it looks like |
| D3 | Beside the tree | Open the tree (T), then `msprobe panel` twice if it closed | Does opening one close the other? Screenshot with both open: is the strip flush against the tree's left edge? |
| D4 | Follows the tree | `msprobe panel` to close it, `msprobe dock`, then T, T, the X button, Escape, I | Log lines `dock open` / `dock close` with `result 0` each time; the strip appears and goes with the tree. Note any time the tree closes when the strip opens |
| D5 | Clicks reach the plugin | Tree and strip open: click each of the three icons | `uiprobe: click MultiSpec 'Probe' '1'` (2, 3) |
| D6 | Clicks stay on the strip | Click empty space on the strip, in town and in the field | The character must not walk or attack; the tree stays open |
| D7 | Escape | Both open: Escape | Note which close (both expected) |
| D8 | Off | `msprobe dock off` | The strip no longer follows |

Send the log's `uiprobe` lines and the D2-D3 screenshots.

### Spike 5, run 2

Changes: the icons sit at the strip's right edge; Escape is left to the tree; a 4th icon sends a stock message (closes
the strip); any UI message naming `MultiSpec` or `Probe` is logged as `uiprobe: click? ...`. Use only `msprobe dock`,
not `msprobe panel` (that opens the strip without the tree, for testing only).

| # | What | How | Expected / note |
|---|---|---|---|
| E1 | Flush | `msprobe dock`, open the tree | Screenshot: icons right next to the tree's frame |
| E2 | Escape | Both open: Escape | Both close (log: `dock close`) |
| E3 | Custom click | Click icons 1-3 | Any `uiprobe: click?` line? Copy them |
| E4 | Stock click | Click icon 4 | Does the strip close? (Then open the tree again to bring it back) |
| E5 | Click-through | Click empty strip space, in town and in the field | The character must not walk or attack |
| E6 | All messages | `msprobe on`, click icon 1 once, `msprobe off` | Copy the lines between |

### Spike 5, run 3

Icons 1-3 now send `PanelManager:OpenPanel:multispec/ProbeSwap<n>`. Only `msprobe dock`, then open the tree.

| # | What | How | Expected / note |
|---|---|---|---|
| F1 | Slot clicks | Click icons 1, 2, 3 | `uiprobe: slot click 'multispec/ProbeSwap1' OpenPanel (consumed)` (2, 3); nothing else opens or closes |
| F2 | Stock click | Click icon 4 | Does the strip close? |
| F3 | Escape | Both open: Escape | Do both close? |
| F4 | Click-through | Click empty strip space, in town and in the field | Does the character walk or attack? |

## Phase 4 spike 6: placement of the docked strip (design section 11)

Same DLL as spike 5. Each row: in town, `msprobe dock`, open the character sheet (C) and the skill tree (T), take one
screenshot. Change the window size in windowed mode (Options > Video), or the resolution in full screen.

Prediction (HD units): the left panel ends at 236 + 1162 = 1398 from the left edge, the tree starts 1394 from the
right edge, and the strip takes 170 more. So the strip touches the character sheet when the screen is narrower than
about 2962 units: fine at 16:9 and 16:10, an overlap of about 80 at 4:3 (if D2R's UI is 2160 units tall at every
size).

| # | Setting | Expected / note |
|---|---|---|
| G1 | 1920 x 1080 (16:9) | Strip flush to the tree; a gap to the character sheet |
| G2 | 2560 x 1440 or 3840 x 2160 (16:9, a larger size) | Same picture as G1, scaled |
| G3 | 1920 x 1200 or 1680 x 1050 (16:10) | Gap smaller, no overlap |
| G4 | 1600 x 1200 or 1024 x 768 (4:3) | Note whether the strip covers the sheet's right edge, and whether a click there goes to the strip or the sheet |
| G5 | 2560 x 1080 or 3440 x 1440 (21:9), if your screen allows | Note whether D2R keeps the panels at the screen edges or pulls them in, and the strip with them |
| G6 | Any size, then G (legacy graphics) | The strip has no legacy layout: note whether it disappears, stays in HD, or errors (log) |
| G7 | Options > Accessibility, large font mode, if present | Note whether the tree moves and the strip still sits flush |

Send the screenshots (and the log if G6 shows an error).

## Phase 4 spike 8: strip background, option A (design section 11, "Strip art")

The strip now draws D2R's own panel background (the same image as the tree's) starting at its left edge, and sits
below the tree (`priority` -5), so the tree should cover the rest and only a 170-wide wing should show. It also has a
small magenta square (a test sprite the plugin makes itself) under the icons. `msprobe dock`, then open the tree.

| # | What | How | Expected / note |
|---|---|---|---|
| H1 | Loaded | Log line `uiprobe on: ...` | Ends `swatch sprite registered (result 0)` |
| H2 | Wing | Tree open | Screenshot. Does the tree draw over the background (only the wing shows), or does the whole background cover the tree? |
| H3 | Looks | Same screenshot | Does the wing read as part of the tree, or as a second panel (two borders side by side)? |
| H4 | Clicks | Click icons 1-3 | `uiprobe: slot click ...` lines, as in spike 5 |
| H5 | Tree still works | Click a skill tab and a skill in the tree | The tree reacts as normal; nothing is blocked |
| H6 | Plugin sprite | Look under the icons | A magenta square: a plugin's own sprite works (option B possible). Nothing there: it doesn't |
| H7 | Left panel | Open the character sheet too | Note whether the wing is still drawn and in front of the game world |

### Spike 8, run 2: own art

The strip is now our own art, drawn by the plugin at load: a stone wing with a metal frame and corner rivets, only as
tall as its 3 slots, starting at the tree's tab row; numbered sockets centred in it (gold rim, brighter on hover);
"Specs" in gold. `msprobe dock`, then open the tree.

| # | What | How | Expected / note |
|---|---|---|---|
| J1 | Loaded | Log line `uiprobe on: ...` | Ends `art registered (result 0)` |
| J2 | Looks | Tree open | Screenshot. Note what to change: colours, frame width, height, where it starts, the numbers' size |
| J3 | Hover | Move over each socket | The rim brightens |
| J4 | Clicks | Click 1, 2, 3 | `uiprobe: slot click 'multispec/ProbeSwap1' ...` (2, 3) |
| J5 | Low texture quality | Options > Video, texture quality low (if present) | The lowend art shows, same look, no missing image |

## Spec panel

Run in a toolkit slot with the test harness. The arrows are at x 988, y 221 and 298 at 1600 x 900. Post `WM_MOUSEWHEEL` to the game window for the wheel.

| # | What | How | Expected |
|---|---|---|---|
| S1 | Loaded | Log | `specui on: multispec/SpecPanel, 3 specs` |
| S2 | Opens with the tree | `T` | Wing beside the tree, the slot shows the active spec |
| S3 | Arrows | Click up, then down | Log `spec 1 -> 2 done`, then `2 -> 1`; the number follows |
| S4 | Wheel | One notch up, one down, then one with the tree closed | Two swaps, none with the tree closed |
| S5 | Closes with the tree | Escape, then `T` twice | Panel gone each time the tree closes |
| S6 | Real points | Spend points in each spec, swap back and forth | Each spec's levels return exactly |
| S7 | Outside town | Take a waypoint out of town, open the tree | Arrows greyed out; a wheel notch is refused. The automap names the area: fences don't mark the town edge |
| S8 | `spec_count` lowered | `spec_count = 99`, `spec 80`, exit, set `spec_count = 3`, open the tree, click down | The slot shows 80, then 3 after the click; the file keeps all 99 specs |
