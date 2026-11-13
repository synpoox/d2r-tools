# Cartographer: rare map in-game check (2026-11-12)

Live in game: issue #353 (rarity follow-ups merged in #373), slot 5, Hell, single player, test save `cartoh` (Barbarian,
raised to level 99 with `cheat level 99` because the example `maps.txt` needs level 80), god mode (`debug_mode`, then
`cheat god 1`) before any map. Plugin built from origin/main `3bae54d`, D2R build 93847, D2RLoader 1.3.1-beta. Logs and
screenshots stay outside the repo. Multiplayer was skipped.

Setup beyond the installer: appended `cmap` rows (itype1 `cmap`, version 100, spawnable, rare 1) to the slot's test mod:
`magicprefix` (mon-life 20-40 with mag% 15, mon-damage 10-30, mon-speed 5-15), `magicsuffix` (res-all 5-9, mon-fire-res
10-20 with str 3-6, mon-cold-res -20 to -10), and two `rareprefix` and two `raresuffix` name rows. The name rows have no
string keys, so rare names show `Missing string` (expected for a mod that adds none). The rows were removed afterwards.

| # | Check | Result |
|---|---|---|
| 1 | Rare map with real `cmap` affixes | PASS |
| 2 | Monster modifiers reach spawned monsters | PASS |
| 3 | Character sheet shows the player-side modifiers | PASS |
| 4 | Drop, pick up, Save and Exit, reload keeps the map | PASS |

## 1. Rare maps roll with affixes

Forced `cheat spawn t20,typ r,lvl 90,inv 0` (and `t21`, `lvl 50`): every map is rare with affixes from the new rows, for
example `Plague Missing string Shorelines Map`: +12% Monster Speed, +10% Monster Fire Resistance, +25% Monster Damage,
+23% Monster Life, +4 Strength, 15% Better Chance of Getting Magic Items; and `Rune Missing string Tunneling Depths Map`:
-15% Monster Cold Resistance, +18% Monster Fire Resistance, +10% Monster Damage, +4 Strength, All Resistances +6.

Unforced `cheat spawn t20,num 60` repeated (clearing the grid and ground between rounds): `cartographer check` went from 12
normal and 1 rare to 467 normal (31 of them magic rolls made normal), 8 rare, 2 unique, 0 magic, 9 set (running totals, so 7
new rares). The 9 set/rare/unique makers that failed (fallbacks sent to normal) equal the 9 set rolls, so none of the rare
rolls failed; the unique count was not separately checked. Only one natural rare was identified (`cheat idall`) and looked
at: a rare map with affixes. The hook sees only about half of the spawned maps (about 31 of 60
per spawn), as in the quality check.

Rare maps spawn unidentified (red inventory cell, `Unidentified` tooltip) until `cheat idall`.

## 2. Monster modifiers

Opening the natural rare map logged:

```
map stats for 'Shorelines' (open ..., slot level 138): 10 item stat(s) read, monsters: item_maxhp_percent(76)=+25,
damagepercent(25)=+27, fireresist(39)=+12, velocitypercent(67)=+7; players: strength(0)=+3, fireresist(39)=+9,
lightresist(41)=+9, coldresist(43)=+9, poisonresist(45)=+9, item_magicbonus(80)=+15
```

The per-monster probe lines show the stats landing on six monsters in the map: max life x1.25 exactly (for example
856064 to 1070080), velocity 75 to 82, fire resist 0 to 12, damage percent 0 to 27, item_maxhp_percent 0 to 25.

## 3. Character sheet

The log line for entering: `player 1 entered the map: totals [0=30 39=0 41=0 43=0 45=0 80=0] -> [0=33 39=9 41=9 43=9
45=9 80=15]`. The character sheet in the map shows Strength 33 in blue (30 plus 3) and Hell resistances at -91% (-100
plus 9). The map panel lists the monster and player modifiers.

## 4. Drop, pick up, Save and Exit, reload

Two rare maps (`Plague ... Shorelines Map`, `Rune ... Tunneling Depths Map`) with `cheat savecheatitems 1` on:

- Ctrl+click dropped the Shorelines map; the ground label read the same name; clicking it picked it up and it went back
  into the inventory with an identical tooltip (same name, same six lines).
- Save and Exit, then Play, Hell: both maps were back in the inventory with identical names and identical affixes.
- Opening the reloaded Shorelines map logged the same stats (`item_maxhp_percent=+23, damagepercent=+25, fireresist=+10, velocitypercent=+12; players: strength=+4,
  item_magicbonus=+15`), so quality and affixes survived. The item level could not be seen (see Findings), so it is not claimed.
  The stat read (10 stats with correct values) also covers the decode guard in effect.
- `d2rloader.log` has no `Character save failed` line.

## Findings

No bug confirmed; two open questions, filed as #383 and #384: the hook saw only about half of the spawned maps (31 of 60),
so the unseen half was not examined (#383); and the item level label reads `Item Level: 1` whatever `lvl` is, so the item level
could not be seen (#384) and its survival across save and reload is not checked. Also, a modder who adds `rareprefix`/`raresuffix` rows without string
keys sees `Missing string` in the name.

Not checked: multiplayer, traded maps (need multiplayer), item level survival, automap lines, a unique map, maps from the cube or vendors, and other game builds.
