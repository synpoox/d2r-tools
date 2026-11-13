# Cartographer: unique pack spread check (2026-11-13)

Live in game for #317 (uniques only near the entrance). D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 11, Hell,
single player, test save (level 99 test Barbarian; the save folder was backed up first and put back byte-identical).
`debug_mode` and `cheat god 1` on, `MazeProbe=1`. Maps: Level 90, Density 100, Uniques 100, ExtraLarge or Large. Every room
was visited by `cheat goto` (nearest the entrance first), not walked by a character. Counts come from the `record probe:`
lines (unique count delta per populated room, rooms in activation order). Logs are kept outside the repository.

| Tileset | Build | Uniques made | First / middle / last third of rooms | Verdict |
|---|---|---|---|---|
| Cave (maze) | origin/main | not recorded | 28 / 44 / 28 percent | spread |
| Snow (outdoor) | origin/main | not recorded | 36 / 32 / 32 percent | spread |
| Lava | origin/main | 13 of 14 | 7 of 13 in closed filler rooms; plan rooms 5 / 1 / 0 | bunched at the entrance, wasted on unreachable rooms |
| Lava | this PR | 8 (5 plan, 3 filler) | plan rooms 2 / 2 / 1; all rooms 3 / 3 / 2 | better, not proven (small numbers) |

Cause on Lava: the closed edge-fill rooms populate and count in `+4` but not in `+0xc`, so they took packs and advanced
the pace. The fix gives filler rooms no uniques (budget = current count) and does not count them as progress.

Not measured: Crypt, Jungle (Cave and Snow stand for the maze and outdoor families), a second layout per tileset, a
character walking the map. Residual: 3 uniques still landed in filler rooms on the fixed build and the total (14) was not
reached (8); the cause is not found (the room classification in the sweep script may differ from the plugin's mask, or the
game rolled the packs before the cap applied). So #317 is not shown fixed on Lava; Cave and Snow need no change.
