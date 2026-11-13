# Cartographer: cube map level check (2026-11-12)

Live in game: a cube recipe raises a map's level (#330), slot 6, Hell, single player, test save (slot 6's mod tables and plugin
restored and the slot unlocked afterwards). D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, test mod `D2RToolsTest`. The runs used a
build with extra logging and the same logic as the code before the review fixes; the final code was built, unit tested, signature
checked and loaded in game (both hooks logged `hooked`), but a transmute was not repeated with it. The slot's `maps.txt` was edited
for the run (three Cave maps) and put back. Logs are kept outside the repo.

| Check | Result |
|---|---|
| 1. The recipe sets the stat | PASS: a Cave map with a Town Portal scroll came out with `cartographer_map_level` 10. |
| 2. A second transmute adds again | PASS: stat before 10, after 20, cube input 10, target 20, read back 20. |
| 3. The map opens at it | PASS: using the map logged `map level 20 (from the item's stat)` and opened a portal (CaveB). |
| 4. The tooltip shows it | Not seen on a map: map items show no property lines in this game. The line shows on non-map outputs (see the #310 check in `docs/reverse-engineering/cartographer/14-item-stats.md`). |
| Stability | No `[FAULT]` lines from the plugin during the transmutes. |

One test mistake, already reported: a first boot script targeted slot 7, which another lane had locked and was running. A few
console commands (`debug_mode`, `cheat god 1`, `cheat easykill 1`) and clicks reached that game before it was noticed; the slot was
not touched again.
