# Cartographer: slot 11 check, map affix Density and magic find (2026-11-12)

Live in game: the plugin built from branch `cartographer/map-affix-density` (#313, PR #362) before the stat was widened to 20 bits, copied into the slot,
D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 11, Hell, single player, test save `cartos` (a level 1 Barbarian; the save was put back from a
backup afterwards). `debug_mode` then `cheat god 1` were run in each game before any map. A second map in one game is refused, so each item was
opened in its own game. The logs and screenshots are not kept in the repository.

Setup: the slot's test mod got the installer's `cartographer_map_density` stat (id 376) and `map-density` property, a `magicprefix.txt` row
`Brutish` (`cmap`, `mon-life`, `mag%` 15) and a `uniqueitems.txt` row `Cartographer Density Unique` (code of the Shorelines map, `map-density` 40 and 35,
`mag%` 25, `mon-life` 10). The Shorelines `MinCharLevel` was lowered to 1 for the check and put back.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Rare map, `mag%` 15 from the prefix | PASS: the panel showed `+15 Magicbonus` on the player; Advanced Stats read `15% Better Chance of Getting Magic Items` |
| 2 | Rare map, `maps.txt` Density 50 | PASS: the level record at `0x2b8` went 600 to 900 (no density affix on that item) |
| 3 | Unique map, `map-density` 40 + 35 | PASS: `map density: 50% from maps.txt +75% from the item's affixes = 125%`, record `0x2b8` 600 to 1350 |
| 4 | Unique map, `mag%` 25 | PASS: `players: item_magicbonus(80)=+25`; the density stat was not listed as a player stat |

Log lines:

    map monsters:   record 0x2b8: 600 -> 900
    map stats for 'Shorelines' ...: 9 item stat(s) read, monsters: item_maxhp_percent(76)=+22, fireresist(39)=+15, coldresist(43)=+29; players: ..., item_magicbonus(80)=+15
    map density: 50% from maps.txt +75% from the item's affixes = 125%
    map monsters:   record 0x2b8: 600 -> 1350
    map stats for 'Shorelines' ...: 2 item stat(s) read, monsters: item_maxhp_percent(76)=+10; players: item_magicbonus(80)=+25

Not checked: the widened (20-bit) stat on a saved and reloaded item, and values above 511 on an item. The wider rows and the repair of an
old-width row are covered only by the offline `tools\test_install_stats.ps1`.
