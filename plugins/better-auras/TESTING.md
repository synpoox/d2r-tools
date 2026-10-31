# Better Auras: in-game tests

Setup: better-auras.dll in `<game>\d2rloader\plugins\` (game-wide), a Paladin with Might, Prayer, Concentration,
Vigor and Conviction learned (a test save, backed up first), the default config D2RLoader writes to
`<game>\d2rloader\config\better-auras.toml`, plugin log under `<game>\d2rloader\logs\`. Run `better-auras` in the console after
each step. After a session, `python testing/game_checks.py run ba-scenario --slot N` reads the log for the evidence.

**Who tests what:** agents test only inside the starting town, in a game slot. Area changes and anything outside town
(rows 9 and 10, and any fight) are the owner's to run: agents don't walk out of town, take waypoints or portals, or
fight.

Status: rows with a result ran on 2026-10-31 in slot 2 (level 90 test Paladin, Rogue Encampment, Normal), no `FAULT`
line. Empty rows are unverified.

| # | What | How | Expected | Log evidence (check) | Result |
|---|---|---|---|---|---|
| 1 | Load | start the mod | every part `on`, no `REFUSED`, no `MISMATCH` | part lines (`ba-loads`) | pass |
| 2 | Toggle on/off | choose Might for the right button; choose it again | aura on (glow, damage up), then off | `aura 98 on`, `aura 98 off (toggled off)` (`ba-toggle`) | pass |
| 3 | Survives skill switch | Might on, choose Attack / another skill, attack | Might stays on, damage bonus still in the character screen | `aura 98 kept through the skill switch` (`ba-survives-switch`) | pass |
| 4 | Several auras | Might and Prayer on, switch skills | both on, both effects | two `on` lines, status lists both | pass |
| 5 | Drain stops at 0 | Prayer on with little mana, cast skills until mana runs out | Prayer turns off when mana can't pay 1.5/s | `aura 99 off (drain could not be paid)` (`ba-drain-stop`) | pass; Prayer's own vanilla mana cost comes on top of the drain |
| 6 | Reservation caps / releases | full mana, Might on; drink a mana potion; Might off | max unchanged; orb 25% empty, text 75/100 style; the potion can't fill past the cap; after off the value stays where it was (not refilled) and regenerates | `reserve N mana` in the on line (`ba-reserve-apply`) | pass: 59/148 with Might + Concentration, max unchanged; mana potion not tried |
| 6b | Life drain | `drain = { resource = "life", perSecond = 5 }` on an aura, turn it on | life orb goes down smoothly; aura turns off before life runs out; never dies from it | `off (drain could not be paid)` | pass at 30/s: the stop comes on the upkeep tick, so life reached 1, not dead |
| 7 | Insufficient pool refusal | Might + Concentration on (60%), then Conviction (50%) | Conviction refused | `aura 123 refused: not enough to reserve` (`ba-reserve-refusal`) | pass |
| 8 | Life reservation | Vigor on | life orb capped 20 below max, max unchanged | `reserve 20 life` | pass: 273/293, a life potion stays at the cap |
| 9 | Death | auras on, die | all off, no cap after respawn | `off (death)` lines (`ba-death`) | owner |
| 10 | Area change | auras on, take a waypoint to another act | still on | no `off` line | owner |
| 11 | Town | Might on, enter town | glow/stats per InTown (paladin auras have InTown=1: stays active) | no `off` line | |
| 12 | Save / load | auras on, save and exit, load | auras off, no cap; character file unchanged in size/stats | no reservation left (status empty) | pass after the fix (the right-skill aura used to come back on at load); file size not compared |
| 13 | Level-up | Might (25%) on, gain a level / equip +mana | reservation follows the new max | status `reserves` value changes | |
| 14 | Mercenary / party | Might on with a mercenary / second player near | they get the aura as vanilla | — | |
| 15 | TCP/IP second client | host + client both with the plugin; client toggles Might, switches skill | aura works and stays on the client; host log shows the lines | host log | |
| 16 | Cast trigger | set `trigger = "cast"` for Might, use it | toggles on/off **[assumption: may not reach the server]** | `aura 98 on` | **fail in town**: two right clicks with Might on the button never reached `0x43b3e0`, and the aura could not turn on. Outside town: owner |
| 17 | Off switches | `<game>\d2rloader\config\better-auras-off.txt` with `decouple` | Might drops on skill switch | part line `decouple off` | pass: `decouple` drops the aura on a switch; all four words give a plain vanilla aura (mana 113/148, above the cap) |
| 18 | Weapon swap | aura on the right button, press W twice | aura stays on | `kept through the skill switch`, no `off` line | pass |
