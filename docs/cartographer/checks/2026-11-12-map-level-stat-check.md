# Cartographer: map level stat, slot 6 check (2026-11-12)

Issue #310. Plugin built from this branch (`cartographer/map-level-stat`), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 6, Hell, single player, test saves `cartos` and `cartoh` (test saves only; slot 6 locked by `carto-310`, tables re-seeded with `stage_check.ps1`, restored and unlocked afterwards). Logs and screenshots are not kept.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Plugin loads with the new rows; the `Map Level: %d` text is registered | PASS (`map stat text: registered the map level text`) |
| 2 | A map item without the stat opens at the `maps.txt` Level | PASS: `map level 85 (the maps.txt Level; the item has no usable map-level stat)` for Overgrown Reach (Harrogath, Hell) |
| 3 | The cube reads the test mod's `cubemain.txt` | PASS: the vanilla recipe 3 Rejuvenation Potions made a Full Rejuvenation Potion in the bottom-right cube slot (output items appear there) |
| 4 | Cube recipe `useitem,mod` (`version` 100, `numinputs` 2, `mod 1` `map-level` 95) sets the stat on a map | FAIL: the recipe fires and sets the stat, but the output is a Hand Axe, not a map; same with input type `cmap` and with the vanilla `isc` |
| 5 | `Map Level: N` shown on hover for an item that has the stat | PASS (on the Hand Axe from check 4: `Map Level: 95`); the stat was still there after a save and reload |
| 6 | `usetype,mod`, `usetype,mag,mod`, plain code output `t22` | Give a map without the stat (the `mod` is dropped on a Normal item); no `Map Level` line |
| 7 | Opening a map that has the stat logs `from the item's stat` | Not checked: no map could be given the stat |
| 8 | Stat adds to an existing value on a second transmute | Not checked |

Cause (shown, not traced): the cube's `useitem` rebuilds a misc item as a Hand Axe, whatever the misc item (map code, map type, vanilla `isc`). Open: another route to put the stat on a map (outputs that keep the map drop the `mod`).
