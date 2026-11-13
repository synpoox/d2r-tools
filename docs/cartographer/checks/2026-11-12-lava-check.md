# Cartographer: Lava maze tileset (PR #332), reveal kept after a town trip for Crypt (#303), Cave still opens (2026-11-12)

Live in game: the plugin built from the merge of `origin/main` into `cartographer/tileset-river-of-flame` (`7483501`), copied into the slot
by `stage_check.ps1`. D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 6, single player, Hell, test save `cartos` (a Barbarian set to level 99,
experience 3520485254, 490 stat points, 98 skill points, Hell opened). `debug_mode` and `cheat god 1` were on before every map was opened.
Maps were opened with `cartographer open <Name>` from the console in Harrogath (`cheat goto 109`), not from the map item. Maps: `LavaA`
(Lava, slot level 140, offset 2500,0), `CryptA` (Crypt, 3000,0), `CaveA` (Cave, 3500,0). A maze slot builds one map per game, so each map
was a new game. The logs and screenshots are not kept in the repository.

## Results

| # | Check | Result |
|---|---|---|
| 1 | A Lava map opens and can be entered (level 140, installer from this branch) | PASS: portal opened, entered, lava floor and lava pools, "LavaA", monster level 85; log `built as planned (10 rooms, entrance an ordinary room)` |
| 2 | Town trip and return from the Lava map | PASS: rebuilt from the same plan, same map name, no second automap skip, no crash. The reveal itself could not be compared: Lava drew no automap lines at all, even after walking (see Notes) |
| 3 | A Crypt map at 3000,0 opens and keeps its reveal (#303) | PASS: walked from the arrival, the automap showed wall lines; after `cheat goto 109` and the portal back, the lines near the arrival were still drawn |
| 4 | A Cave map still opens | PASS: level 138, `built as planned (12 rooms, entrance an ordinary room)`, portal room is a cave with a torch |

Skipped: the map item path, Normal and Nightmare, multiplayer, walking through a whole layout, the Lava boss and event rooms (only the
spawn setup lines in the log), a Cave reveal after a town trip.

## Notes

- Lava draws no automap cells. With the automap on (Tab) after several walks the Lava level showed only the party markers and the level
  text. Town, Crypt and Cave pieces draw wall lines. So "keeps its reveal" is untestable for Lava until the Lava pieces have automap
  entries; what was seen is that the town trip needed no second skip and no read of another map's cells (`not read back for this new map (level 140)`
  once, on entering).
- The first entry of each map logged `the revealed cells of an earlier map in layer N were not read back for this new map` (layer 78 for
  level 140, 10 for 139, 2 for 138), the intended `AutomapRestore` skip from #337.
- Clicks come from `game_ctl.py click <fx> <fy>` as fractions of the window; the portal in Harrogath is at about 0.469, 0.40.
- The slot was put back with `stage_check.ps1 -Restore` and unlocked. The three `cartos` files it kept in the save folder were deleted by hand.
