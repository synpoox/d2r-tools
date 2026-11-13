# Cartographer: map level stat, slot 6 check (2026-11-12)

Issue #310. Plugin built from this branch (`cartographer/map-level-stat`), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 6, Hell, single player, test saves `cartos` and `cartoh` (test saves only; slot 6 locked by `carto-310`, tables re-seeded with `stage_check.ps1`, restored and unlocked afterwards). Logs and screenshots are not kept.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Plugin loads with the new rows; the `Map Level: %d` text is registered | PASS (`map stat text: registered the map level text`) |
| 2 | A map item without the stat opens at the `maps.txt` Level | PASS: `map level 85 (the maps.txt Level; the item has no usable map-level stat)` for Overgrown Reach (Harrogath, Hell) |
| 3 | Cube recipe (`usetype,mod`, then `usetype,mag,mod`) sets the stat to 95 | FAIL / not settled: the map came out unchanged, name white, tooltip showed no `Map Level` line, and the Town Portal scroll was not consumed (so the recipe may not have matched at all) |
| 4 | `Map Level: N` shown on hover for an item that has the stat | Not checked (no item with the stat was made) |
| 5 | Opening a map with the stat logs `from the item's stat` | Not checked |

Not found: why the recipe did not transmute. Next step for whoever picks it up: test the same `cubemain.txt` row with a vanilla input pair to see whether recipes are read in this test mod at all, then try a map item with a magic quality.
