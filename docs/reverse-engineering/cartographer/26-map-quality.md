## 26. Map items are rolled normal, rare or unique, never magic (issue #352)

Two hooks stop the game making a map magic: the item quality roll, and the magic fallback the item maker takes when a set, rare or
unique maker fails (`src/engine/map_quality.cpp`, decisions in `src/mapquality`). The `Magic` column of `itemtypes.txt` cannot do
this: `Magic` 1 means always magic, 0 only means "not forced".

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #352, parent #309) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Location | Item quality roll `0x444200` `(item, request) -> quality` (1 low, 2 normal, 3 superior, 4 magic, 5 set, 6 rare, 7 unique, 8 crafted); magic fallback `0x442750` `(a, request, &item, &level)`; normal fallback `0x442820` `(a, request, &item, level)`. See the chain below |
| Guard | Three byte signatures, each found once (`ItemQualityRoll`, `MagicFallback`, `NormalFallback`); the two hooks cut the first 16 bytes of their function (whole instructions). The item is a map when its `misc.txt` row has pSpell (`+0x94`) 16, read through `UnitClassOf` and `ItemRecord` (both found by signature; the data flag is the byte at unit `+0x1bd`). `tools/check_map_quality_signature.py <image>` re-checks all of it against a dump: every signature unique at its RVA, guards whole instructions, and the item maker's roll call at `0x442e7b` targets the roll and returns to `0x442e80` |
| Contract | Roll hook: calls the original; a magic result for a map item is changed to normal (2), unless `request+0x40` is nonzero (a forced quality, which the game applies over the roll anyway). Fallback hook: for a map, calls `0x442820` (normal) with `*level` instead of `0x442750`. Everything else is returned untouched |
| Failure mode | Fail open: if a part a hook needs is missing, or its guard does not match, that hook is not installed and the game does what it did before; `cartographer check` then says the block is partial or off |
| Evidence | See "In game" below |

### The fallback chain

The item maker `0x442d60` calls the roll at `0x442e7b` and acts on the result (a nonzero `request+0x40` wins over it, via `cmovne`
at `0x442e8c`). The roll only uses ratios: it does not check that a set or unique version of the base exists, so a map can roll set (5).
The quality then picks a maker from a jump table:

- magic (4): the magic affix maker;
- set (5): set maker `0x58acd0`; for `cmap` there is no set item, so it fails and falls to `0x442750`;
- rare (6): rare maker `0x58ac70`; if it fails, `0x442750`;
- unique (7): unique maker; if it fails, it calls the roll a second time (`0x443206`, result thrown away) and runs the rare path above.

`0x442750` is the magic fallback: it calls the roll (result thrown away), sets quality 4 on the item, writes `request+0x40 = 4` and jumps to
the magic affix maker `0x442c60`. `0x442680` (superior, 3) and `0x442820` (normal, 2) are the same shape with other qualities. The
three also call the roll, so the roll hook runs in them; those calls are not counted. Without the fallback hook a map that rolled set
(or failed rare or unique) would come out as a full magic map; the fallback hook sends a map to `0x442820`. `0x442750` takes its level by
pointer in `r9`, `0x442820` by value in `r9d`, so the hook passes `*level`.

### The map level line (issue #422)

No new address, signature or table row. The quality roll hook (`0x444200`, main roll only) also gives a new map item its `maps.txt` Level as the `cartographer_map_level` stat (`StampMapLevel` in `map_quality.cpp`; the pure decision is `statplan::mapLevelToStamp`), through the existing `ItemMagicStat` / `SetItemMagicStat` (list maker + `SetListStat`, notes 14, 15 and 25). It runs before the forced-quality early return, so `typ m` and `typ r` spawns get it too. The line sorts first because the row's `descpriority` is 255 (the installer value from #310, above the monster stats at 250 and every vanilla row, whose highest is 180).

Seen in game, 2026-11-13, build 93847, D2RLoader 1.3.1-beta, slot 7, Hell, test save, [results in PR #425](https://github.com/synpoox/d2r-tools/pull/425): the stat written during the roll (item made, list made by the stat writer) is kept by the later makers: normal, magic (forced) and rare maps show `Area Level: N` as the first line, ahead of `+x% Monster ...` and player stats; it survives Save and Exit and a restart; the map opens at it (`map level 85 (from the item's stat)`). The affix makers add their lines after it. The stat is on the base magic list the open reads (`from the item's stat` logged).

Later stamp points (same build, slot 5, 2026-11-13, [results in PR #425](https://github.com/synpoox/d2r-tools/pull/425)): (1) the stamp is also done after the cube executor `0x5269c0` returns, for every output on the cube page that has no stat yet (`StampMapOutput` via `StampCubeOutputs`; the cube-page items are class 4, page byte 0x55 == 3); a map that was already stamped by the roll hook is left alone. (2) A `map-level` property of an affix or unique runs `ItemPropApply` after the roll hook stamped the item; `PropApplyHook` clears the stamped value first (the item pointer is remembered by `NoteStampedMap`), so the modder's value replaces the Level and is not added to it. A cube apply is different: it adds to the stamped Level, starting from the input's level or, for a fresh output, the `maps.txt` Level (`MapsLevelOfItem`). Seen in game: unique `map-level 95` on a Level 85 map logged as replacing (stat 95; the unique was unidentified, so no hover); `usetype,nor,mod` 85 to 95 to 105 on the same map; `t22,mod` from a Level 90 input gave 100; a plain `t22` output showed 85 (stamped by the roll hook, so the post-cube stamp was not proven by this run); a `testtreasure mon=19,num=40` drop stamped 40 maps (log lines, ground names; the pickup hover was not read).

Limits: existing items get no stat; `compactsave` 1 on the `misc.txt` row keeps no stats (note 16; not retested here, the check slot's rows were switched to 0 first). A magic or rare affix with a `map-level` property was not tried. Vendor, chest and gamble maps were not checked (the third creator caller `0x48de7b` stays unexamined).

### What it does not cover

- A quality forced by the caller (`cheat spawn ...,typ m`) stays magic: the game puts it over the roll.
- Maps that already exist (in a save, in a stash) stay as they are.
- The test is the misc.txt spell, the same one `portal.cpp` uses to treat an item as a map, so any item of a mod with spell 16 is affected, whether or not it is in `maps.txt`.
- The cube: `0x527009` calls the item creator with flag 0 and no request, so the item maker and the roll never run; the recipe's own quality (for example `t20,mag`) is set elsewhere. A map output with `mag` is not covered. Not verified live (see the coverage section).
- The third caller of the creator (`0x48de7b`, flag from `edi`) was not examined (probably vendors or gambling).
- Multiplayer and other builds were not checked.

### In game

Slot 6, build 93847, [2026-11-12-map-quality-check.md](../../cartographer/checks/2026-11-12-map-quality-check.md): of 148 natural map rolls the hook saw, 142 were normal (5 of them magic rolls made normal), 1 rare, 2 unique, 3 set and 0 magic; the 6 set, rare and unique rolls all failed their maker (the test mod has no `cmap` affix rows) and were sent to the normal fallback, and so were 3 forced rare spawns. Forced magic spawns stayed magic. A rare or unique map that succeeds was not seen.

### Coverage: why the log "saw half"

Build 93847, D2RLoader 1.3.1-beta, 2026-11-13, [check record](../../cartographer/checks/2026-11-13-quality-hook-coverage.md). The creator `0x43ef10(game, Unit** item, request, flag)` calls the item maker `0x442d60` only when `flag != 0` and `request != null`. Callers:

| Caller | Flag, request | Used by | Reaches the roll |
|---|---|---|---|
| call site `0x43cf18` (item spawn `0x43cd50`; return address `0x43cf1d`) | 1, set | monster and chest drops, `cheat spawn` | yes |
| call site `0x527009` (cube executor) | 0, null | cube outputs | no |
| call site `0x48de7b` | `edi`, not examined | probably vendor or gamble | not checked |

The earlier "148 of 304" is most likely a reporting gap, not a missed path: the summary printed normal, rare, unique, magic and set, but left out low (1) and superior (3). In game on 2026-11-13 the scratch diagnostic hooks logged every maker call and every main roll (return `0x442e80`) of `cheat spawn`. The three log snapshots (20, 133 and 240 maker calls) are cumulative, not separate runs: the counts only grow, so the session made 240 maps (the ground area stops spawning at about 130 items per area, and the game was restarted between area fills). Of the 240, every maker call had a main roll (240 of 240):

| Rolled | low 1 | normal 2 | superior 3 | magic 4 | set 5 | rare 6 | unique 7 |
|---|---|---|---|---|---|---|---|
| Maps | 114 | 97 | 23 | 4 | 0 | 1 | 1 |

Final quality after the maker was normal (2) for all 240: the plugin makes magic normal, the failed rare and unique go through the normal fallback, and the game itself gives a map normal for a low (114) or superior (23) roll (observed, not traced in the code). So low and superior rolls (137 of 240) never show in the final quality, and a summary that lists only the qualities of the finished items counts about half of the rolls as missing. Cause: inferred from these counts and the summary code; the old run (304 spawned, 148 counted) was not repeated with the old build. Monster drops (`testtreasure mon=19,num=10`): 10 maker calls, 10 main rolls, all normal. Drop roll breakdown was not kept.

Item level: the maker's request holds the level at `request+0x18`. For `cheat spawn` it is 100 (240 of 240 maker calls: `request+0x18` 100, unit level 100 before and after the maker); drops from `testtreasure` in town had level 1. The "Item Level: 1" on a ground label is the UI thread's client copy, which reads 1 even for level 100 units.
