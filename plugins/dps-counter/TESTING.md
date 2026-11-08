# Testing DPS Counter

Run in a toolkit game slot, in Hell, on a level 90+ character. `dps-counter` in the console prints the live state
(hits counted, the current DPS, the panel).

| # | What | Expected | Status |
|---|---|---|---|
| 1 | Loads | Log: `counter on: damage apply rva 0x44a9b0 (total at +0x134) ...` | pass (Slot10, 2026-11-06) |
| 2 | Melee a monster | `DPS: n` just above the life globe; status `hits` climbs | pass (Slot7, Hell, level 95 Barbarian, 2026-11-08): one 3000-damage hit on a Zombie showed `DPS: 1,378`, `hits` 1 |
| 3 | A missile skill (bow, Fire Bolt) | The counter shows; status `hits` climbs | pass: `cheat fstorm` on `cheat mspawn minion1,4` in Hell showed `DPS: 4` (level 1 monsters, little life) |
| 4 | Stop attacking | The text goes away after `window_seconds` | pass (Slot7): 6 s after the last hit, status `dps 0`, `shown ""`, no text on screen |
| 5 | Hit an immune monster | No damage counted | pass (Slot7, Hell): `cheat fstorm` on a Minion of Destruction (fire resist 120 in Hell): 381 hits reached the hook, `dps 0`, no text. (In Normal its fire resist is 50, so fire counts there.) |
| 6 | Leave and rejoin a game | The counter comes back; no faults in `dps-counter.log` | pass (Slot7): after Save and Exit and a rejoin, a hit showed `DPS: 1,126`; `faults 0` |
| 7 | Label position | Just above the life globe, over `Life:` | pass (Slot7, Hell, level 95 Paladin): `DPS: 1,800` drawn right above `Life: 8191 / 8191` |
| 8 | `enabled = false` | No label, no hook; log `counter off: enabled = false in dps-counter.toml` | pass (Slot7, Hell): zombies killed, no label; status `feed off, hits 0` |
