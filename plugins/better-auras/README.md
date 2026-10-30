# Better Auras

A D2RLoader plugin that gives a Diablo II: Resurrected mod Path of Exile-style auras, in offline and TCP/IP games
(never Battle.net):

- **Toggle**: choosing (or, optionally, using) a listed aura turns it on; doing it again turns it off. It stays on until
  you turn it off, you die, or its drain/reservation can't be paid.
- **Drain**: optional mana or life per second, taken smoothly every frame by the game's own regen (as negative
  regeneration). When less than a second of drain is left at its upkeep tick, the aura turns off. Life drain never kills (one point stays).
- **Reservation**: optional percent-of-max or flat mana/life. Max stays the same; the current value can't rise above
  max minus what is reserved, so the orb shows the reserved part empty and its text reads unreserved/max
  (Path of Exile style). Refused when the pool can't cover it. Turning the aura off lifts the cap: you keep the value
  you have and regenerate from there.
- **Decoupled from the skill bar**: a toggled aura stays on whatever skill is on the right button. Several auras can
  run at once.

Tested on: D2R 3.3 (Battle.net or Steam; engine addresses from image `0x6ab3782c`), D2RLoader 1.3.1,
PluginSDK 0.3.0.

## Install

Better Auras is a game-wide plugin: it runs with every mod D2RLoader starts.

1. With the game closed, copy `build\Release\better-auras.dll` to `<game>\d2rloader\plugins\`. Don't put a copy in a
   mod's own `d2rloader\plugins\` folder (the plugin warns when it finds one).
2. Start the game through D2RLoader once: D2RLoader writes the default config to
   `<game>\d2rloader\config\better-auras.toml` (the repo's `better-auras.toml`, built into the DLL).
3. Close the game, edit that file (below), start again.

The plugin is `Shared`: in TCP/IP every player needs the same version. All aura state lives on the host.

## Config (`<game>\d2rloader\config\better-auras.toml`)

Only skills listed under `[skills.<name or id>]` are changed. Each one starts from `[defaults]`.

```toml
[defaults]
enabled = true          # false: the skill stays vanilla
toggle  = true          # false: vanilla "on while selected"; drain and reservation still apply
trigger = "select"      # "select": choosing it for the right button toggles it; "cast": using it toggles it

[skills.Might]          # skills.txt name, or id: [skills.98]
reserve = { resource = "mana", mode = "percent", value = 25 }

[skills.Prayer]
drain = { resource = "mana", perSecond = 1.5 }
```

Dotted keys (`drain.resource = "mana"`) and sub-tables (`[skills.Might.drain]`) work too. Names come from the mod's
loose `skills.txt` when there is one, else from D2R's aura names. A listed skill that uses no aura function (the
`aura` bit, or srvdofunc 65/66/81/82) stays vanilla and the log says so.

## Rules it follows

| Situation | What happens |
|---|---|
| Death | every aura turns off, newest first, and its reservation is released |
| Area change, waypoint, town portal | auras stay on |
| In town | as vanilla: a skill without `InTown` does nothing in town (and drains nothing) but stays toggled |
| Save and exit / load | auras start off, also the one on the right button; nothing is written to the save |
| Weapon swap | auras stay as they are: only choosing a skill yourself toggles it |
| Several reservations | each is checked against the pool's max before any reservation; at least one point stays free; released newest first |
| Level-up, gear changes | percent reservations follow the new max; flat ones stay. If the pool can't cover them all, the newest aura turns off |
| Two listed auras with the same `aurastate` | the second one is refused |
| Aura strength, range, mercenaries, party | unchanged: the vanilla aura functions do the work |
| Monsters' and mercenaries' own auras | never touched (players only) |

## Off switches and diagnosis

`<game>\d2rloader\config\better-auras-off.txt` (next to the config), words separated by spaces: `toggle`, `drain`,
`reserve`, `decouple` (and the template's `config`, `envguard`, `soundguard`, `typeids`). Restart after a change. The
plugin's log (the file D2RLoader gives it, under `<game>\d2rloader\logs\`) has one line per part at load and one line
per aura on/off/refusal with the reason.

## How it works

<<<<<<< HEAD
Addresses, guards and evidence: [docs/reverse-engineering/better-auras.md](../../docs/reverse-engineering/better-auras.md). In short: vanilla keeps the right-skill aura alive
=======
See [docs/reverse-engineering/better-auras.md](../../docs/reverse-engineering/better-auras.md). In short: vanilla keeps the right-skill aura alive
>>>>>>> 7ad78a7 (better-auras: drain through the game's regen, reservation as a cap on current life/mana)
with unit timer event 8 keyed -1, and a skill switch removes that event and the aura's state list. The same event can
be keyed with the skill id; those timers survive a switch. A toggled aura is started the vanilla way and moved to its
own timer.

Drain is a negative `hpregen` (stat 74) or `manarecovery` (stat 26) inside the aura's own state list: the per-frame
player regen tick (`0x42e600`, mana step `0x42e350`) applies it, and it goes away with the list. Reservation is a cap
the plugin applies right after that regen tick every frame: current life/mana at most max - reserved.

Hook sites (image `0x6ab3782c`; the registry above is authoritative):

| Hook | RVA | Why |
|---|---|---|
| aura upkeep tick (event 8) | `0x437460` | drain stop check, percent reservations follow the pool |
| player regen tick (event 3, every frame) | `0x42e600` | reservation cap |
| start right-skill aura | `0x43b680` | toggled skills don't start on select |
| skill select (past its first branch) | `0x438a7a` | select trigger; rebuild a toggled aura's list after a switch |
| start skill | `0x43b3e0` | cast trigger |
| unit death | `0x44db20` | release everything |

## Build

From the monorepo root (the PluginSDK is the shared `third_party/PluginSDK` submodule: `git submodule update --init`):

```
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin better-auras
```

Standalone: `powershell -ExecutionPolicy Bypass -File plugins\better-auras\build.ps1` (uses the same submodule).

Output: `build\Release\better-auras.dll`. Checks for d2rAI-toolkit's `testing/` framework: `tests\better_auras_checks.py`;
manual steps: `TESTING.md`.
