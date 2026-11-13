## 23. Map items are never magic (issue #352)

The game's item quality roll is hooked; a map item that rolls magic comes out normal (`src/engine/map_quality.cpp`, decision in
`src/mapquality`). The `Magic` column of `itemtypes.txt` cannot do this: `Magic` 1 means always magic, 0 only means "not forced".

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #352, parent #309) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Location | Item quality roll `0x444200` `(item, request) -> quality` (1 low, 2 normal, 3 superior, 4 magic, 5 set, 6 rare, 7 unique, 8 crafted). Callers: the item maker `0x442d60` (calls at `0x442e7b` and `0x443206`) and the affix fallbacks `0x442680`, `0x442750`, `0x442820`, which ignore the result. A nonzero `request+0x40` (forced quality, as `cheat spawn ...,typ m`) overrides the roll in `0x442d60`, so only natural rolls change |
| Guard | Byte signature `48 89 54 24 10 41 55 41 57 48 83 EC 58 4C 8B EA 4C 8B F9 E8 ?? ?? ?? ?? 66 83 F8 01 72 ?? 41 8B 45 40 85 C0 0F 85 ?? ?? ?? ?? 48 89 5C 24 70 49 8B CF`, found once; the hook cuts the first 16 bytes (five whole instructions). `tools/check_map_quality_signature.py <image>` re-checks it against a dump (unique at `0x444200`, whole instructions, call at +19). The item is a map when its `misc.txt` row has pSpell (`+0x94`) 16, read through `UnitClassOf` and `ItemRecord` (both found by signature; the data flag is the byte at unit `+0x1bd`) |
| Contract | Calls the original; only a magic result for a map item is changed, to normal (2). Everything else is returned untouched, so no other item is affected |
| Failure mode | Fail open: if any of the three parts is missing, or the guard does not match, the hook is not installed and the game rolls as before (`cartographer check` says "not blocked") |
| Evidence | Static: signature unique in the dump. In game (slot 6, build 93847): `cheat spawn t20,num 20,lvl 85` run six times (about 120 Shorelines maps), the hook logged 8 conversions and every ground label was white |

Not covered: a map forced to magic by the spawn command (`typ m`) stays magic, by design; multiplayer and other item sources
(cube, vendors) were not checked.
