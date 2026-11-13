# Cartographer: map panel total Density and Magic Find, in game (2026-11-13)

Live in game for issue #392 (PR #412, head `0f8ce8d`, main merged in): slot 6 (slot 11 was held by another lane), Hell, single player, test
save `cartos` (Barbarian, level 99), god mode (`debug_mode`, then `cheat god 1`) before any map. D2R build 93847, D2RLoader 1.3.1-beta.
Staged with `stage_check.ps1 -Slot 6 -MapItems -Tables examples\first-map`; the affix tables (`magicprefix`, `magicsuffix`, `rareprefix`,
`raresuffix`, `cmap` rows) were copied from slot 11's test mod into slot 6's, and slot 6 was restored and unlocked afterwards. The map came
from `cheat spawn t20,typ r,lvl 90,inv 0`, then `cheat idall`, then a right-click in Harrogath and the red portal. Screenshots stay outside the repo.

| # | Check | Result |
|---|---|---|
| 1 | Rare Shorelines map (maps.txt `Density` 50, a Magic Find modifier of 15, no density modifier) | PASS: the panel shows `+50% Monster Density` as the last line under Monsters have and `15% Better Chance of Getting Magic Items` under Players have |
| 2 | Log for the same map | `Density 50%` written to the level; map stats read: monsters life +36, fire +29, cold +28, speed +20, players resists +8 and magic find +15 |

Not checked: a map with a density modifier (the total of `Density` plus the bonus; unit tested only), a map with several Magic Find
modifiers (the merged total; unit tested only), a map with Density 0 (line left out; unit tested only).
