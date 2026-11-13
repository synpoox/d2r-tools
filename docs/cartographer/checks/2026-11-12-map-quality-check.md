# Cartographer: map quality check (2026-11-12)

Live in game: issue #352 (a map the game rolls as magic is made normal), slot 6, Hell, single player, test save, god mode
(`debug_mode`, then `cheat god 1`) before any item. Plugin built from `847fe54` (PR #360), D2R build 93847. The test mod
has the installer's `Map` item type (`cmap`) and the three map items (`t20` to `t22`, spell 16), but **no `cmap` affix rows
and no unique map**. Logs and screenshots stay outside the repo.

Items were made with `cheat spawn t20,num 20` (no `typ`, so the game rolls the quality itself). `cartographer check` printed the
plugin's counts of what the item maker was given for a map: only natural rolls, not forced ones, not the rolls inside the fallbacks.

| Check | Result |
|---|---|
| Hooks live | PASS: the log shows `ItemQualityRoll` and `MagicFallback` hooked, `NormalFallback` found. |
| Natural sample | PASS: 148 map rolls seen (the game made fewer than the 304 spawned; the rest were not seen by the hook and are not counted): 142 normal, 1 rare, 2 unique, 3 set, **0 magic**. 5 of the 142 normal were magic rolls the hook made normal. |
| Set, rare and unique rolls | 6 of 6 (3 set, 1 rare, 2 unique) went to the magic fallback because their maker failed (the test mod has no `cmap` affix rows or unique map); the fallback hook sent all 6 to the normal one. Without it, these would have been magic maps with affixes. |
| Forced rare (`typ r`) | 3 spawned: the rare maker failed the same way, 3 more fallbacks sent to normal (9 in all). |
| Forced magic (`typ m`) | 3 spawned: not counted and not changed. They stay magic, as written in the docs. |
| Ground labels | Only the item under the cursor shows its name (the others show `Item Level: 1`); the ones looked at were white `Shorelines Map`. The labels are **not** a count of the sample; the counts above are. |
| Stability | PASS: no error, assert or crash line in `cartographer.log`. |

Not checked: a rare or unique map that makes it through (it needs `cmap` affix rows or a unique map in the mod), a second
sample at a high item level (`cheat isl 85` did not change the level shown, `Item Level: 1`), multiplayer, maps from the cube
or vendors, and other game builds.
