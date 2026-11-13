# Cartographer: Shrine event and Boss equipment Drop check (2026-11-12)

Live in game: the plugin built from `main` at `6b822c7`, D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7, Hell, single player, test save
`cartos` (a level 1 Barbarian, not the level 99 test character: god mode and `easykill` were on). `debug_mode` and `cheat god 1` were sent right after each
game started, before any map was opened. Part of issue #111. The logs and screenshots are not kept in the repository.

Setup: `cartographer.txt` with `EventChance 100`, a Crypt map `BossDrop` (Corridor, Small, `zombie1` only), opened with `cartographer open BossDrop`
(the character must be in Harrogath: `cheat goto 109` first), entered by the portal; the event room was reached with `cheat goto 138,<x>,<y>` in
server units (the room's DRLG x and y times 5, plus 20), read from the log's `maze event room` line. One `mapevents.txt` row per game:

| Game | Event row | Result |
|---|---|---|
| 1 | Shrine, `Spawn` `ShrineHealth` (by name) | object placed in the event room |
| 2 | Shrine, `Spawn` `172` (by row) | object placed, used: "You feel healthy." |
| 3 | Boss `zombie1`, Count 3, `Drop` `Act 5 (H) Equip C` | the Boss rolled the Drop once |

The slot's test mod has no `objects.txt` of its own for the by-row game; for the by-name game one was copied in by hand (the plugin needs the file
to turn a name into a row).

## Passed

- **Shrine by name (game 1).** `ShrineHealth` resolved to row 172 (`objects.txt: 588 lines, game: 587 rows`, the `Expansion` marker line is not a
  row), a free spot was found on the first try and the object was created in the event room (`created objects row 172: unit ... (type 2, id 41)`).
  It showed in the room as a stone well.
- **Shrine by row (game 2).** `Spawn 172` was taken as a row number (`objects row 172 from the number in Spawn (the game has 587 rows)`); a spot was
  found in the event room (`near (17520,140) radius 5 ... (in the event room)`) and the object was created (`type 2, id 39`). Hovering it showed the name
  **Health Shrine**; a click on it showed the game's message **"You feel healthy."** So the object is a working game shrine, not an inert copy.
- **Boss `Drop` with equipment (game 3).** The Boss was tracked (`type 1, id 16`), spawned with 3 minions (`spawned 4 of 4`), and the log showed one
  `the tracked boss ... died: rolled`, `rolling 'Act 5 (H) Equip C' (treasure class 360)`, `the roll returned`. After the kills the ground had a
  **War Fist** with a white name (normal quality) that was not there before the Boss died. A magic (blue) **Shako** was also on the ground, but it was
  already there before the Boss died, so it came from a minion's own drop and not from the roll.
- 0 `ERROR` or `FAULT` lines in the plugin log (all three games).

## Not settled

- **Game 1's click**: the click on the by-name object showed no change. Game 2 showed the same object (row 172) works when the click lands on it, so game 1's click most likely
  missed it (the harness only moves the player and clicks at a pixel); it was not repeated.
- **Quality across many rolls**: one equipment item (normal) was seen, from one roll. Normal is what an `Equip C` class can give but one item does not show the quality mix.
- **Which item came from the Boss's roll**: the log lists no items; the War Fist is attributed to the roll because it appeared after the Boss died.
- **Multiplayer**: not checked here; a Shrine or Boss Drop for clients has not been tested.
- The shrine's effect on health was not measured (the health orb was not lowered by the cheats tried).

## Other

- Loader asserts at start-up and around `cheat goto` (PopcornUber/PopcornLiquid render pipelines, `BC_VERIFY ptWarp`, a `PathUtil.cpp:740` assert), as in earlier checks; the game went on.
- With `cheat god` off the Zombie minions killed the test character; god mode stayed on for the rest.
- The slot was restored and unlocked by `stage_check.ps1 -Restore`; the stray `cartos.*` save files it left were deleted by hand.
