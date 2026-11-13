# Map rarity and item modifiers check (#311)

Slot 11, D2RToolsTest mod, test save `cartos`, D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, single player, Hell. `debug_mode` and `cheat god 1` first.

Setup: `tools\install.ps1` run on the test mod (cmap Magic and Rare on, seven monster stats and properties added); test rows added by hand to `magicprefix.txt`, `magicsuffix.txt`, `rareprefix.txt`, `raresuffix.txt` and `uniqueitems.txt` (a `mon-life` prefix with a fixed `mag%` mod, resistance and strength suffixes, one unique map with `mon-life` 30, `mon-fire-res` -20, `res-all` 15, `map-level` 95).

| Check | Result |
|---|---|
| `cheat spawn t20,typ r,lvl 90,inv 0`, `t21,typ m`, `t22,typ u` | A rare, a magic and a unique map item; rare and magic carry rolled affixes, the unique its fixed ones. |
| Hover | The map level, the monster modifiers (`+30% Monster Life`) and the player modifiers show. |
| Item stat read | 7 stats on the unique, 16 bytes an entry (see reverse-engineering 14, section 16); a first read as 8-byte pairs gave wrong stats and was fixed. |
| Opening the unique map | Log: `map level 95 (from the item's stat)`, `6 item stat(s) read`, monsters `item_maxhp_percent` +30 and `fireresist` -20, players the four resists +15. |

Not checked: the stats on a spawned monster after the corrected decode, the player's stats on the character sheet, Save and Exit and reload with a modified map item, dropped and traded items, multiplayer.
