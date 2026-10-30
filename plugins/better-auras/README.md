# Better Auras

A D2RLoader plugin that gives a Diablo II: Resurrected mod Path of Exile-style auras, in offline and TCP/IP games
(never Battle.net):

- **Toggle**: choosing (or, optionally, using) a listed aura turns it on; doing it again turns it off. It stays on until
  you turn it off, you die, or its drain/reservation can't be paid.
- **Drain**: optional mana or life per second. When the pool can't pay, the aura turns off. Life drain never kills
  (one point stays).
- **Reservation**: optional percent-of-max or flat mana/life. It lowers the pool's max while the aura is on and is
  refused when the pool can't cover it. Turning the aura off gives it back.
- **Decoupled from the skill bar**: a toggled aura stays on whatever skill is on the right button. Several auras can
  run at once.

Target: D2R 3.3 (engine addresses for image `0x6ab3782c`: Battle.net 93847 / Steam 94995), D2RLoader 1.3.1,
PluginSDK 0.3.0. On any other game image the plugin installs nothing and says so.

## Install

Better Auras is a game-wide plugin: it runs with every mod D2RLoader starts.

1. With the game closed, copy `build\Release\better-auras.dll` to `<game>\d2rloader\plugins\`. Don't put a copy in a
   mod's own `d2rloader\plugins\` folder (the plugin warns when it finds one).
2. Start the game through D2RLoader once: D2RLoader writes the default config to
   `<game>\d2rloader\config\better-auras.toml` (the repo's `better-auras.toml`, built into the DLL).
3. Close the game, edit that file (below), start again.
4. In game, the console command `better-auras` prints every part's state, the config path, the counters and the active
   auras.

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
| Save and exit / load | auras start off; nothing is written to the save |
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

See `knowledge/d2r-93847-addresses.md` ("Auras") in d2rAI-toolkit. In short: vanilla keeps the right-skill aura alive
with unit timer event 8 keyed -1, and a skill switch removes that event and the aura's state list. The same event can
be keyed with the skill id; those timers survive a switch. A toggled aura is started the vanilla way and moved to its
own timer. Drain and reservation run in the timer's handler; the reservation is a negative max-life/mana stat inside
the aura's own state list, so whatever removes the aura also releases it.

| Hook | RVA | Why |
|---|---|---|
| aura upkeep tick (event 8) | `0x437460` | drain, reservation upkeep |
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
