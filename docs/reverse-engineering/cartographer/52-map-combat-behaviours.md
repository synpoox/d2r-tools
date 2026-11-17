# 52. Map combat behaviours: extra groups, life steal, physical as element (issue #540)

Build 93847. Keys and their table are in [tables.md](../../cartographer/tables.md); this note holds the engine facts.

## extra_groups

The region record of a level (`*(game+0x1d0)[level]`) holds up to 13 monster groups: count at +0x10 and +0x12, weight sum at +0x11 (one byte, at most 255), groups from +0x14, stride 0x34 (row word at +0, weight byte at +2, members byte at +3). The level row hook (`LevelRowBeforeOpen`) runs before the portal opens and before the map's stats are applied, so the extras reach it through the `MapDef` copy (`extraGroups`, monstats Id and count), which `portal.cpp` fills from the map item's routing. `MergeExtraGroups` (levelrow.cpp) adds weight to a row already held, appends a new row while there is room, and scales weights to a sum of 255 at most. The old `MaxMonsters` (10) is now `RecMaxGroups` (13).

## lifesteal_pct: the leech step

| | |
|---|---|
| RVA | 0x450c90 (unique match in the 93847 image, `tools/check_portal_signatures.py`) |
| Signature | `40 55 56 57 41 56 48 83 EC 58 41 83 B9 20 01 00 00 00` |
| Hook | 18 bytes (whole instructions); the first relative instruction (`jnz`) is at +30, so max patch 30 |
| Arguments | rcx game, rdx attacker unit, r8 target unit, r9 damage block |

The step opens with `cmp dword [r9+0x120], 0` and runs for a hit that carries a life or mana leech. Damage block: +0x00 flags (bit 0x40: element damages count), +0x18 physical, +0x20 / +0x2c / +0x30 / +0x34 element damages, +0x120 life leech, +0x124 mana leech. For a monster attacker that is not a minion the +0x120 value is a flat amount (the game caps it at the damage); for a player or minion attacker it is a percent, so those are never touched.

The hook adds `pct` percent of (physical plus element damage) to +0x120 (cut to 2^25, as the game shifts it by 6) for an attacker that is a monster, hostile (stat 172 = 0) and stands in an opened map with `lifesteal_pct` above 0. Units are the block's own (1/256 life).

## phys_as_<element>

Done where the map's monster stats are applied (`MonsterSetup`, after the game's own setup): the monster's stats 21 and 22 (min and max damage) are read, and that percent of each is added in the same stat list as the map's other monster stats: fire 48/49, lightning 50/51, magic 52/53, cold 54/55 with length 56 (50 frames), poison 57/58 per frame (value times 256 over the length) with length 59 (100 frames).

## Test path

`CARTOGRAPHER_TEST_ROUTING` (`<key>[:<param>]=<value>;...`) adds route sums to the map's, in both the item open and the console open. Unset: nothing happens. A game check only.

## Game check

Slot 11, Shorelines Cave, `CARTOGRAPHER_TEST_ROUTING='extra_groups:skeleton1=4;lifesteal_pct=100'`:

- extra_groups: log "the map now has 3 group(s), weight sum 12 (rows 63,5,0)"; skeletons spawned and fought in the map (screenshots).
- phys_as: statprobe lines show the element stats added to the spawn stat list.
- lifesteal_pct: statprobe "leech in the block 0 -> 14005" on a monster hit, hook ran. The attacker was at full life (1385728 before and after), so a life rise was not observed.
