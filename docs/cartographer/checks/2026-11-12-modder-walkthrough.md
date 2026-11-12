# Cartographer: modder walkthrough of first-map.md (2026-11-12)

A first-time modder follows [first-map.md](../first-map.md) as written (origin/main 85014f0). Live in game: D2R 3.3.0 build
93847, D2RLoader 1.3.1-beta, slot 5 (test mod `D2RToolsTest`, Hell test save `cartoh`, a level 40 Barbarian), single
player. Release layout built by hand from `dist\cartographer` plus `release-files.txt` (the DLL, README, CHANGELOG,
`tools\install.ps1`, `examples\first-map`); `tools\build.ps1 -Plugin cartographer -Test` passed (18 of 18 tests). Slot
locked, staged and restored with `stage_check.ps1` (owner `modder-walk`); the slot is unlocked again. Screenshots and the log
are not kept in the repository.

How the slot was staged, which differs from a real modder's start: `stage_check.ps1` needs `-Dll`, so it had already placed the
DLL and run the installer once (steps 1.1, 1.3 and 4 were therefore repeated, not first runs); the slot's test mod already held
the levels of an earlier lane (`Cartographer Outdoor`, `Cartographer Snow`); and it has no `misc.txt`, `weapons.txt`,
`armor.txt` or JSON files, so `stage_check.ps1 -MapItems` was used once to seed `misc.txt` and the two JSON files from the
vanilla data (the guide gives no other source), and the three `cmap` rows were then added to `misc.txt` by hand from
`map-items.txt`.

## Result per step

| Step | Result | Notes |
|---|---|---|
| 1.1 DLL to `d2rloader\plugins\` | pass | loaded, "cartographer: 0.1.0 loaded" |
| 1.2 `bosses.txt` to the excel `d2rloader\cartographer\` folder | pass | the folder already existed (not tested on a mod without it) |
| 1.3, 1.4 `tools\install.ps1 -ModDirectory ...` | pass | wrote the `superuniques.txt` row; the other lines said "already there" because of the earlier staging run |
| 2 `misc.txt` `cmap` rows | pass, with friction | rows per `map-items.txt`; the tooltip read "Shorelines Map" |
| 2 `item-names.json`, `items.json` | fail as written | no path, no format, no id rule; see friction 2 |
| 3 `maps.txt` | pass | example file copied |
| 4 `cartographer check` | pass, with friction | `Cartographer 0.1.0: 3 map(s), 0 problem(s). The portal is on.` |
| Item into the inventory | `debug_mode`, `cheat spawn t20,inv 0` | the guide offered only `stage_check -MapItems`; see friction 3 |
| 5 open a Cave map in Hell | pass, with friction | `Shorelines is open.`, red portal, `maze slot level 138: built as planned (24 rooms, entrance placed)` |
| 5 Boss log line | pass, with friction | `map Boss 'Shoreline Warden': spawned in the boss room (31240,1000) 40 x 40 (as a superunique)` |

## Friction (and what first-map.md now says)

1. "You need ... `levels`, `lvlmaze` and `misc`": `check` also reads `weapons.txt` and `armor.txt` (item codes) and the
   installer needs `itemtypes.txt`. With no `weapons`/`armor` the log warned `some of the game's tables could not be read, so
   Monsters, Boss and Item names are not checked`, which the guide never mentions. The list and step 4 now say so.
2. Step 2: "Add each name to `item-names.json` and the code to `items.json`." gives no folder, no record format and no id rule,
   and does not say that the mod's file replaces the game's whole file. Step 2 now names both paths, shows one entry of each and
   states the id rule. The vendor `Min`/`Max` columns (blanked by `stage_check`) are now mentioned as well (not tested by hand).
3. "To test, `tools\stage_check.ps1 -MapItems` ...": that is a repository script a modder does not have. The console route
   (`debug_mode`, then `cheat spawn t20,inv 0`) is now in step 2.
4. Step 4 "ends with:" is not what the console shows (the log continues with part lines), and it does not say that `check`
   can be run again after editing `maps.txt`. Now "prints" and the re-read is stated (confirmed: lowering `MinCharLevel` and
   running `check` again took effect without a restart).
5. Step 5: the example map `Shorelines` has `MinCharLevel` 80; the first right-click answered `Shorelines needs character level
   80 (you are 40).` The guide says only "at least `MinCharLevel`". Now it says to lower it for a low-level test character.
6. The Boss line is not logged "when its room is built" (on entering the portal): it appeared only after the player came near the
   boss room at the far end of the maze (here a `cheat goto 138,31262,1022` warp; the plan's boss room is in the `eventprobe`
   log line). Reworded.
7. Not hit, only inferred: the `d2rloader\cartographer\` folder may not exist in a mod that never had the installer run, so
   step 1.2 now says to create it.

## Not covered

No second map (the Jungle and Cave Loop maps), no Save and Exit, no client, no refusal messages other than the level one; the
example maps' `Level` 90 and the level 40 character were left as they are.
