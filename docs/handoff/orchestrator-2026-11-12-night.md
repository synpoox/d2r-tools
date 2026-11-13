# Orchestrator handoff, 2026-11-12 (night)

Main is at `1cca7bd`. That is "Cartographer: map items are never magic (quality-roll hook) (#360)".

## Owner rules in force

- **Tilesets:** no new tileset work until the open PRs and the remaining non-tileset issues below are resolved. That covers #303 and everything else in the tileset plan. Finishing an open tileset PR, such as #335, is allowed.
- **Worker cap:** at most 5 workers, plus one Opus reviewer per PR. A PR gets 2 fix cycles; whatever is left after that becomes a follow-up issue. Merge with squash only.
- **Game slots:** Cartographer uses slots 5, 6, 7 and 11 only.
  - Every in-game brief names the d2rAI-toolkit `d2r-in-game-testing` skill, and requires `debug_mode` and then `cheat god 1` before any map.
  - Use test saves only, never the owner's characters.
- **Never without the owner:** tagging, releases, the #19 release, or the self-hosted runners.
- **Owner messages:** only for a blocker, an important update, or a check the owner has to do.

## Owner decisions this session

- **Map level and modifiers live on the item.** #309 is the parent issue.
- **Unique maps:** fixed authored stats, in uniqueitems.txt.
- **Rarity:** the game's item quality. Maps are normal, rare or unique, never magic.
  - itemtypes cannot stop magic (`Magic=1` means *always* magic), so the hook in #360 does it.
- **Rare modifiers:** come from the magicprefix and magicsuffix tables (itype `cmap`), and each mod points at a properties.txt row.
  - mapstats.txt and the ItemStats setting are retired.
  - Monster-side properties have their own itemstatcost stats (`cartographer_mon_*`).
- **Density and MF:** each property carries a fixed Density and MF value. A map's bonus is the sum of its properties. Density is uncapped: `cartographer_map_density` is 20 bits, -524288..524287 per item. MF is the vanilla `mag%`.
- **Cube:** a recipe can only raise a map's level. It cannot set an exact level, and there is no follow-up for that.

## Merged this session (main)

- **Maps and items:**
  - #316: ExtraSmall size, uncapped Density.
  - #320: Uniques spread.
  - #322: map level as an item stat.
  - #323: map panel.
  - #350: rarity, closes #311.
  - #355: cube raises map level, closes #330.
  - #360: no magic maps, closes #352.
  - #362: affix Density, closes #313 and #358.
  - #367: cube follow-ups, closes #361.
- **Shapes and messages:**
  - #325 and #339: Shape Open.
  - #349: Jungle Open message, closes #347.
- **Mazes and automap:**
  - #329: maze slot table.
  - #332: Lava, closes #303. #239 stays open.
  - #337: automap reveal after a town trip, closes #306.
- **Siege and events:**
  - #336: Siege.
  - #363: event banner as a plugin panel, closes #357.
- **Docs and check records:** #348, #351, #356, #359.
- **Shared:** #365 removed `merge=union` from `.gitattributes`. Plugin README and CHANGELOG conflicts are now resolved by hand.

## Open PRs, state and next step

| PR | State | Next step |
| --- | --- | --- |
| #335 Ice caves (`cartographer/tileset-ice-caves`, 4c7e729) | Cycle 2 approved. CI passes. GitHub says CONFLICTING with main after #360. | Merge origin/main once, by hand: CHANGELOG, README and the RE index (notes 23–26). Rerun `test_install_stats.ps1`, then merge. Keep `Refs #245`, because the Boss waits on #369. |
| #369 Boss room fallback (`cartographer/maze-boss-room`, d25db2d) | Draft, WIP, `Closes #364`. Unit tests pass. In game, a non-edge cell showed the named Boss. | Game-check the fallback itself: a top-edge cell whose top-left sub-room is void, on Ice once #335 is in. Add a check record and an RE note line, then review. |
| #368 automap follow-ups (`cartographer/automap-followups`, 924c92d) | Draft, `Refs #354`. Skip hardening done, 20/20 tests, Cave town trip checked in game. | Check a real waypoint trip by eye; the worker couldn't find Harrogath's waypoint. Then review. |
| #318 second map cursor (`cartographer/second-map-cursor`, 749798e) | Waits on the owner's real-mouse check (steps in the PR body). It carries #324, which covers #321. | After the owner passes it: merge, close #307 and #321, then do #326. |

## Open issues, priority order (non-tileset first)

1. #335 → #364/#369 → then #245 (Ice) can close.
2. #366: Lava draws no automap lines. The owner is testing this in their own game now; see "Owner's game" below.
3. #354 / #368: waypoint check.
4. #318 → #326, plus #307 and #321 (they close with #318).
5. #353: rarity follow-ups.
   - Still to see in game: modifiers on spawned monsters, the character sheet, a rare map with real `cmap` affix rows that succeeds, dropped or traded maps, and multiplayer.
   - The `mon-speed` floor is still to do.
6. #328: crash at 0x1255802.
   - It did not recur with only Cartographer loaded (11 round trips; #359).
   - Next: a run with the other plugins and Cartographer off.
   - Read the assert string at 0x1c783d0.
   - Look at the `warpDefs` assert (LvlTbls.cpp:817) seen once at the first Shorelines entry.
7. #317: Uniques now spread out (#320). The issue is still open; ask the owner whether the spread fix is enough or wants a higher count.
8. #111: mostly settled (#351, #356). Left: multiplayer, the shrine effect on health, and whether a Boss Drop roll can come out rare.
9. #239: Lava. Still to check in game: opening from the map item, the Boss and event rooms, and the lava edges.
10. Later: #269 (after release), #247 (it conflicts with the "docs for modders" rule; ask the owner), #142, #73, and #19 (release, owner only).

Tileset plan issues #238–#293 and #236 stay paused per the owner.

## Owner's game (slot 1, ProjectForever mod), installed at the owner's request

- `d2rloader\plugins\cartographer.dll` is the main build at `a32a092`. The old copy is saved beside it as `cartographer.dll.before-20261112-212855`.
- `tools\install.ps1` was run on `mods\ProjectForever`. It:
  - added the Crypt (141), Lava (142) and Siege (143) levels;
  - moved Maze to 3500,0 and Snow to 1500,0;
  - added the stat rows at ids 376–384 and their properties;
  - turned off Magic for `cmap`.
- For the Lava test: a misc.txt item `t23` "Molten Depths Map" (a copy of t20) and a maps.txt row "Molten Depths" (Lava, Corridor, Large, no Boss).
  - Backups sit beside the files as `*.before-lava-20261112-212855`.
  - The installer's own `.bak` copies are kept too.
- The build at `a32a092` does not include #360 (no magic maps) or #367 (cube follow-ups). Reinstall from main if the owner wants those.

## Hazards

- **Wrong-slot input:** three times, a worker's boot or input script acted on another lane's slot (slots 6 and 7).
  - Every brief must make the worker confirm the window and PID belong to its own slot before sending input.
  - The real fix belongs in d2rAI-toolkit: scripts should refuse unless the caller holds the slot's lock. The owner has not decided; don't change that repo without them.
- **Stale local merge checks:** `git merge-tree` run from an older worktree uses that worktree's `.gitattributes`. That gave false CLEAN results after #365. Run merge checks from a worktree on current main.
- **RE note numbers:** assign the number in each brief. Main has 22 (two notes), 23 Lava, 24 Ice (#335), 25 cube, 26 map quality. The next free number is 27.
- **int16 automap rule:** for slot levels whose Layer is not 79, every corner of (tx-ty)*8 and (tx+ty)*4 must stay within ±32767.
  - Spots: Cave 3500,0, Crypt 3000,0, Lava 2500,0, Ice 2000,0, Snow 1500,0.
  - The next maze row takes x 1000 or less, clear of Snow.
- **install.ps1:** edit only with the Edit tool. Keep it CRLF with no BOM and check the backslash count. Run `plugins/cartographer/tools/test_install_stats.ps1` after any change; every maze tileset needs a fixture level there.
- **Slot state:**
  - Slot 11's test mod has hand-added `cmap` affix rows, a test unique, and the density row widened to 20 bits.
  - Slot 7 holds `cartoh.*` saves that belong to no current lane.
  - Slot 5 has a `cartoh` test character.
  - All Cartographer slots are unlocked and stopped.
- **Dungeon WIP:** branch 46623064 is paused (#244); don't resume until the owner allows tileset work.

## Leftover worktrees

None of these hold uncommitted Cartographer work from this session; every open PR branch is pushed.

- `.claude/worktrees/orchestrator-bugs-features-c39fa9`: this session's own worktree.
- `.claude/worktrees/cartographer-d2r-crash-3b370d` and `orchestrator-handoff-continue-c7fe23`: from earlier sessions.
- `d2r-tools-*` sibling worktrees for other plugins (automap-marker, better-auras, dps-counter, multispec-class, no-pet-collision, player-bars, quick-npc-hooks): not touched this session.
