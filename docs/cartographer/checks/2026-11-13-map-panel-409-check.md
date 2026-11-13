# Cartographer: map panel size, input and SetListStat, in game (2026-11-13)

Live in game for issue #409 (follow-up to PR #419): slot 7, Hell, single player, test save `cartos` (Barbarian, level 99), god mode (`debug_mode`,
then `cheat god 1`) before any map. D2R build 93847, D2RLoader 1.3.1-beta. Staged with `stage_check.ps1 -Slot 7 -MapItems`; the map rows in
`misc.txt` were set to `compactsave 0`; `cartographer check` was run before the map was opened. A rare Shorelines map with six affixes and
duplicate stats came from a throwaway affix pool (six `cmap` rows in the test mod's `magicprefix` and `magicsuffix`, not committed) and a
real t20 item from `-MapItems`, used from the inventory. Saves were backed up and restored with the game stopped; the slot was restored and unlocked.
Screenshots stay outside the repo.

| # | Check | Result |
|---|---|---|
| a | Full text on a rare map with many properties | PASS: 20 lines (4 monster header and property lines with merged sums, 12 player lines) all show, none cut off at the bottom |
| b | Panel takes no clicks | PASS: an inventory item inside the 760x1540 rect was picked up normally with the panel up |
| b | Panel does not block movement | PASS: a click inside the rect moved the character |
| b | Panel does not cover right-side HUD | PASS for input; with the inventory open the panel text draws over the top of the equipment area (text only, no input effect). No resize needed |
| c | `SetListStat` sets or adds | SET: Strength applied twice in a test build (same value, then value + 1) gave the second value on the character sheet (base 30, +31 = 61), not the sum. Same-stat affixes are merged by `buildPlan` before applying, so the sheet equals the panel sum |
| c | Merged sums against the character sheet | PASS: Dexterity +10, Vitality +10, Energy +10, Fire Resist and the others matched the panel |

The panel needs at most 20 lines on a real map, not the 27-line worst case, but the extra height does nothing to input, so the panel was not
resized and no code changed.

Finding (not fixed here): `ReadMapItemStats` reads at most 16 item stats. On a 19-stat rare, monster speed, poison resist and light resist were
dropped; the log says "the item has N stats; only the first 16 are read", so it is not silent.

Not checked: a map close to the 27-line worst case (a real map cannot reach it with six affixes), a Magic or Normal map, and other resolutions
than 1920x1080.
