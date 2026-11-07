# Testing DPS Counter

Run in a toolkit game slot, in Hell, on a level 90+ character. `dps-counter` in the console prints the live state
(hits counted, the current DPS, the panel).

| # | What | Expected | Status |
|---|---|---|---|
| 1 | Loads | Log: `counter on: damage apply rva 0x44a9b0 (total at +0x134) ...` | pass (Slot10, 2026-11-06) |
| 2 | Melee a monster | `DPS: n` at the top centre; status `hits` climbs | not run |
| 3 | A missile skill (bow, Fire Bolt) | The counter shows; status `hits` climbs | pass: `cheat fstorm` on `cheat mspawn minion1,4` in Hell showed `DPS: 4` (level 1 monsters, little life) |
| 4 | Stop attacking | The text goes away after `window_seconds` | not run |
| 5 | Hit an immune monster | No damage counted | not run |
| 6 | Leave and rejoin a game | The counter comes back; no faults in `dps-counter.log` | not run |
