# Better Auras: in-game tests

Setup: better-auras.dll in `<game>\d2rloader\plugins\` (game-wide), a Paladin with Might, Prayer, Concentration,
Vigor and Conviction learned (a test save, backed up first), the default config D2RLoader writes to
`<game>\d2rloader\config\better-auras.toml`, plugin log under `<game>\d2rloader\logs\`. Run `better-auras` in the console after
each step. After a session, `python testing/game_checks.py run ba-scenario --slot N` reads the log for the evidence.

Status: **not run yet**. Every row below is unverified.

| # | What | How | Expected | Log evidence (check) | Result |
|---|---|---|---|---|---|
| 1 | Load | start the mod | every part `on`, no `REFUSED`, no `MISMATCH` | part lines (`ba-loads`) | |
| 2 | Toggle on/off | choose Might for the right button; choose it again | aura on (glow, damage up), then off | `aura 98 on`, `aura 98 off (toggled off)` (`ba-toggle`) | |
| 3 | Survives skill switch | Might on, choose Attack / another skill, attack | Might stays on, damage bonus still in the character screen | `aura 98 kept through the skill switch` (`ba-survives-switch`) | |
| 4 | Several auras | Might and Prayer on, switch skills | both on, both effects | two `on` lines, status lists both | |
| 5 | Drain stops at 0 | Prayer on with little mana, cast skills until mana runs out | Prayer turns off when mana can't pay 1.5/s | `aura 99 off (drain could not be paid)` (`ba-drain-stop`) | |
| 6 | Reservation lowers / restores max | note max mana, Might on, then off | max mana down 25%, back after off; current mana clamped | `reserve N mana` in the on line (`ba-reserve-apply`) | |
| 7 | Insufficient pool refusal | Might + Concentration on (60%), then Conviction (50%) | Conviction refused | `aura 123 refused: not enough to reserve` (`ba-reserve-refusal`) | |
| 8 | Life reservation | Vigor on | max life down 20 | `reserve 20 life` | |
| 9 | Death | auras on, die | all off, max mana/life back | `off (death)` lines (`ba-death`) | |
| 10 | Area change | auras on, take a waypoint to another act | still on | no `off` line | |
| 11 | Town | Might on, enter town | glow/stats per InTown (paladin auras have InTown=1: stays active) | no `off` line | |
| 12 | Save / load | auras on, save and exit, load | auras off, max mana/life full; character file unchanged in size/stats | no reservation left (status empty) | |
| 13 | Level-up | Might (25%) on, gain a level / equip +mana | reservation follows the new max | status `reserves` value changes | |
| 14 | Mercenary / party | Might on with a mercenary / second player near | they get the aura as vanilla | — | |
| 15 | TCP/IP second client | host + client both with the plugin; client toggles Might, switches skill | aura works and stays on the client; host log shows the lines | host log | |
| 16 | Cast trigger | set `trigger = "cast"` for Might, use it | toggles on/off **[assumption: may not reach the server]** | `aura 98 on` | |
| 17 | Off switches | `<game>\d2rloader\config\better-auras-off.txt` with `decouple` | Might drops on skill switch | part line `decouple off` | |
