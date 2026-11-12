# Cartographer: Boss names (Latin-1) and superunique Drop check (2026-11-12)

Live in game: the plugin built from `main` at `8874045` (DLL SHA-256 `6D577A9A...DB31D2`, copied into the slot by `stage_check.ps1`; the
installed DLL hash equals the built one and the log's first line is `0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`
at game start), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 5, Hell, single player, test save `cartoh`. `stage_check.ps1` parked
the PD2 `core-maps`, `core-rules` and `core-ui` plugins, so the slot ran with Cartographer and `test-harness` only. `debug_mode`,
`cheat god 1` and (for kills) `cheat easykill 1` were on. Three games: two with `bosses.txt` and `maps.txt` saved as Latin-1 (the
file bytes for `é` and `Ö` are single bytes), one with the same tables saved as UTF-8. Parts of issues #186, #193 and #111. The logs and
screenshots are not kept in the repository (screenshot names below are from the run folder of the lane).

Setup (tables written for this check, no Cartographer example files): `bosses.txt` with `Gérard Östberg`, `Röné, Jr.` and
`Frost-Eye, Jr.` (base `zombie1`, Mods `5`, `Minions 0`); `maps.txt` with `HellA` (Cave Corridor Small, Boss `Gérard Östberg`,
Monsters `zombie1`) and `HellB` (same, Boss `Frost-Eye, Jr.`); `EventChance 100`, `Properties 0`; `mapevents.txt` with one Boss event,
`Spawn cartoboss:R_F6n_E9_2C_20Jr_2E` (the escaped key of `Röné, Jr.`), `Count 1`, `Drop Runes 10`. Each map was opened with
`cartographer open <Name>`; the Boss room and the event room were reached with `cheat goto`.

## Results

| # | Check | Result |
|---|---|---|
| 1 | #186/#193 Boss names on hover | `Frost-Eye, Jr.` (ASCII): PASS. `Röné, Jr.` and `Gérard Östberg` from a Latin-1 file: FAIL (`é` and `Ö` drawn as `*`). The same names from a UTF-8 file: `Gérard Östberg` PASS (`Röné, Jr.` not hovered in that game) |
| 2 | #111 Drop roll for a superunique Boss | Roll fires: PASS in 2 of 2 kills. What dropped: not settled |
| 3 | Save and Exit in a map, reload | PASS (no "Character save failed" in 2 exits). One other assert line, below |

## 1. Boss names

Hover shots show the panel title over the Boss (each hover was by mouse position without a click; the pack's zombies were hovered
too and read `Zombie`, so the Boss was told from them by its name and its mods line).

- **`Frost-Eye, Jr.` (ASCII, map Boss of `HellB`)**: the panel read `Frost-Eye, Jr.` with `Undead, Extra Strong, Spectral Hit, Cursed`
  (`g2-hover-frost2.png`). PASS.
- **`Gérard Östberg` (Latin-1 file, map Boss of `HellA`)**: the panel read `G*rard *stberg` with `Undead, Extra Strong, Magic Resistant,
  Spectral Hit` (`g1-hover-b.png`). FAIL: the two accented letters are drawn as `*`.
- **`Röné, Jr.` (Latin-1 file, the event's Boss)**: the panel read `R*n*, Jr.` with `Undead, Extra Strong, Magic Resistant, Stone Skin`
  (`g1-hover-d.png`). FAIL, same cause.
- **`Gérard Östberg` (UTF-8 file, game 3, installer rerun, tables restaged)**: the panel read `Gérard Östberg` correctly
  (`g3-h4.png`). So the game draws these letters; what fails is the Latin-1 byte file. Whether the plugin or the file format is to
  blame is not decided here (the docs do not say which encoding `bosses.txt` should have). Tracked in [#253](https://github.com/synpoox/d2r-tools/issues/253). Only the UTF-8 file's `Gérard Östberg` was
  hovered; `Röné, Jr.` was not tried in UTF-8.

The spawn lines, from the kept plugin log (the `?` is a Latin-1 byte the log viewer does not decode):

    spawn 'cartoboss:G_E9rard_20_D6stberg': superuniques row 66 from the game's table, class monstats row 5
    map Boss for 'HellA': 'G?rard ?stberg', its own superuniques row
    map Boss 'G?rard ?stberg': spawned in the boss room (30520,40) 40 x 40 (as a superunique)

With the UTF-8 file the key changed to `cartoboss:G_C3_A9rard_20_C3_96stberg` and the log line read `map Boss for 'HellA': 'Gérard Östberg',
its own superuniques row`.

## 2. Drop roll for a superunique Boss (#111)

The event Boss was the `Röné, Jr.` entry (a `superuniques.txt` row of its own, `Minions 0`, with one zombie from the event's `Count 1`).
The log said `event 'Rune Lord' (Boss): spawned 2 of 2 in the event room (...) (1 as a superunique, 0 refused by the game)`. Kills by
`easykill`, one per game. Lines from the kept logs, game 1 (unit id 26) and game 2 (unit id 45):

    boss drop: Drop 'Runes 10': name map 00000001149B4300, the game has 1519 treasure classes, lookup gave 195
    boss drop: tracking unit 000000098E34E050 (type 1, id 26)
    boss drop: the tracked boss (unit 000000098E34E050, id 26, type 1) died: rolled
    boss drop: rolling 'Runes 10' (treasure class 195) for unit id 26 after the game's own drop: killer 000000098E33A390, level value 88, arg3 0
    boss drop: the roll returned
    boss drop: tracking unit 000000098E346B10 (type 1, id 45)
    boss drop: the tracked boss (unit 000000098E346B10, id 45, type 1) died: rolled
    boss drop: rolling 'Runes 10' (treasure class 195) for unit id 45 after the game's own drop: killer 000000098E345D10, level value 88, arg3 0
    boss drop: the roll returned

(Each line has the prefix `eventprobe:` in the log.) Each superunique Boss got one `died: rolled`, one `rolling` and one `the roll returned`.
The earlier record left this case open ([boss-drop-check](2026-11-12-boss-drop-check.md)); the roll fires for a superunique Boss in these two kills.

Not settled: **what the roll dropped.** After game 2's kill the ground showed potions, white weapons and item glints
(`g2-after2.png`, `g2-items-zoom.png`); after game 1's kill potions and a polearm (`g1-kill-zoom.png`). No rune was recognised in either
picture, and the item labels were not read, so this run does not show that the `Runes 10` roll produced a rune (the zombie minion
and the game's own drop add items). The log shows the roll was called and returned, not what it made.

## 3. Save and Exit inside a map, then reload

Save and Exit was used from inside the open `HellA` map (game 1, after the kill) and the open `HellB` map (game 2, after the kill).
The loader log after each (`d2rloader.log`, kept per game) has no `Character save failed` and no `encoding-error` line; `cartoh.d2s`
was rewritten each time (timestamps 14:29:28 and 14:32:56). Reload: after game 2 the character list showed `Level 5 Barbarian`
(`menu3.png`), up from level 1 at the start (the level-ups came from the kills), so the save kept the progress. Game 3 was left with
the game stopped, not exited, so it has no save line.

One other line in the loader log, at game 1's map entry (14:25:54) and game 3's (14:35:30, `g3-d2rloader.log` line 125); game 2 was clean ([#254](https://github.com/synpoox/d2r-tools/issues/254)):

    ASSERTION - a game check failed ... BC_ASSERT: i != dataTable->warpDefs.end() ... D2Common\src\DataTbls\LvlTbls.cpp:817
    "Error in DRLG related to stairs that connect levels"

"Continuing because assertion dialogs are disabled"; the game went on, the map built and the later saves worked. It is not the `BC_ASSERT nStat`
of the earlier save failures. Not investigated here, and not shown to come from Cartographer or from this setup.

## Other

- 0 `ERROR` or `FAULT` lines in the plugin log (all three games, one appended file).
- A level-1 character in god mode was used; the Boss rooms were reached with `cheat goto`, so no walking from the entrance was tested.
- Staging: `stage_check.ps1 -Slot 5 -Owner carto-names-drop`, tables and the `cartoh` test save from a scratch folder; the second stage (UTF-8
  tables) re-ran the installer ("wrote 3 boss row(s), replacing 3"); `-Restore` put the three parked plugins back and unlocked the slot.

## Not tested

- `Röné, Jr.` from a UTF-8 file; a Latin-1 name on the Snow or Jungle level.
- The contents of the `Runes 10` drop; a third kill; a `Drop` on a map Boss (only events have a `Drop` column).
- Nightmare, a second player.
