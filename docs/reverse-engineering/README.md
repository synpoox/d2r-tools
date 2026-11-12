# Reverse-engineering registry

All D2R and D2RCore native knowledge for this repository lives here: addresses (RVAs), byte guards, structure
offsets, call contracts and the evidence behind them. One document per feature or plugin; this README indexes them so
nothing is found twice. Plugin source keeps only the constants it needs, with a comment that points back here.

How the records are produced: [Finding RVAs and interception points](RVA-AND-INTERCEPTION-WORKFLOW.md).
General engine facts that are not tied to one plugin also belong in d2rAI-toolkit's `knowledge/` (its rule 10);
record them here first, then copy them there.

## Feature documents

- [Quick NPC](quick-npc.md): NPC left-click path, modifier keys, the NPC menu table, menu option handlers
  (Trade, Identify, Resurrect, Travel), the `0x101910` and `0x1141d0` hooks.
- [Better Automap](better-automap.md): automap, mini map and mouse-wheel hook sites with evidence.
- [Better XP Bar](better-xp-bar.md): XP and level readers; [dump inventory](better-xp-bar-dump-inventory.md).
- [Monster Count](monster-count.md): RE spike (nothing hooked yet); [address notes](monster-count-addresses.md).
- [MultiSpec](multispec.md): skills and respec functions; [recon](multispec-recon.md).
- [Better Auras](better-auras.md): aura upkeep timers, right-skill select, player life/mana regen, proposed aura
  and regen interception points.
- [Player Bars](player-bars.md): life and mana reads, screen shift, orb widgets, native bars.
- [DPS Counter](dps-counter.md): the server damage apply `0x44a9b0` (hooked) and the missile owner.
- [Cartographer](cartographer.md): stub of native addresses from the pd2-resurrected research (maze steering, outdoor
  builder, level regeneration); nothing validated here yet, so it has no index row below until the S0 probe produces byte signatures. Docs: [docs/cartographer/](../cartographer/README.md).

The pre-registry documents above (all except Quick NPC, and including the Cartographer stub) keep their original format; bring each to the
[recording format](#recording-format) and add its useful rows to the index below when its plugin is next changed.

## Tested compatibility profiles

| Profile | Module | Identity | Consumers |
|---|---|---|---|
| D2R 3.3 build 93847 (Battle.net) / 94995 (Steam) under D2RLoader 1.3.1 | D2R.exe (unpacked image) | PE `TimeDateStamp` `0x6ab3782c` | every plugin in `plugins/` |

The packed on-disk `D2R.exe` differs between Steam and Battle.net; the unpacked image is the same. Plugins check the
image stamp plus the entry bytes of every hook, never a build number. Record a file SHA-256 here when one was
actually tested.

## Recording format

Every feature document records, per address or contract:

| Field | Required content |
|---|---|
| Date | Discovery or validation date |
| Consumer | Plugin and part using it |
| Module | `D2R.exe` or `D2RCore.dll` |
| Build identity | Image stamp (and SHA-256 when known) |
| Location | RVA, never a process address on its own |
| Guard | Expected entry bytes (whole instructions), pattern or layout check |
| Contract | Signature, inputs, outputs, side effects, thread and lifetime rules |
| Evidence | How it was found: disassembly, xrefs, strings, dump checks, with a reproducible command |
| Validation | Unit tests, dump verification and in-game results, listed separately |
| Failure mode | What the plugin does when the guard fails (it must fail open: log and leave that part off) |
| Coexistence | Other plugins or D2RLoader hooks on the same code |

Plugins do not gate on this image: there is no version check, and addresses here are evidence from the tested build. New engine code should find them by byte signature, and each plugin's `crash_guard.h` logs faults.

## Known high-value entries (image `0x6ab3782c`)

| Module | RVA / layout | Purpose | Details |
|---|---:|---|---|
| D2R.exe | `0xfe3b0` | Player action `(player, action, x, y)`: builds the client command flags | [quick-npc.md](quick-npc.md#1-left-click-on-an-npc) |
| D2R.exe | `0x101910` | Interact with unit `(cmd, unitType, unitId)`; hooked by Quick NPC | [quick-npc.md](quick-npc.md#5-hooks) |
| D2R.exe | `0x1141d0` | Open NPC menu `(npc, flag)`; hooked by Quick NPC | [quick-npc.md](quick-npc.md#5-hooks) |
| D2R.exe | `0x113990` / `0x1147a0` | NPC menu row lookup / menu builder | [quick-npc.md](quick-npc.md#3-npc-menu-table) |
| D2R.exe | `0x112f30` / `0x114000` / `0x113130` / `0x1162f0` | Menu handlers Trade / Identify / Resurrect / Travel, `bool(u8 dataFlag)` | [quick-npc.md](quick-npc.md#4-menu-option-handlers) |
| D2R.exe | `0x2a48830` (+8 count) | NPC menu table: vector of `0x48`-byte rows | [quick-npc.md](quick-npc.md#3-npc-menu-table) |
| D2R.exe | `0x2a4875c` / `0x2a48760` / `0x2a48764` / `0x2a486a4` / `0x2a487b0` | Open-menu NPC id / has-NPC / class / NPC UI state / menu widget | [quick-npc.md](quick-npc.md#4-menu-option-handlers) |
| D2R.exe | unit `+0` / `+4` / `+8` | Type (`0x34b9d0`) / class (`0x349860`) / unit id (`0x34a330`) | [quick-npc.md](quick-npc.md#6-unit-layout) |
| D2R.exe | `0x22a92b0`, `0x22a9820` | Default key-binding tables (Run = Ctrl, Stand Still = Shift, Show Items = Alt) | [quick-npc.md](quick-npc.md#2-modifier-keys) |
| D2R.exe | `0x437460` | Event 8 handler: aura upkeep tick `(game, unit, key, a4, a5)`; proposed hook | [better-auras.md](better-auras.md#1-interception-points) |
| D2R.exe | `0x43b2b0` / `0x48b890` | Schedule event 8 (key -1 right skill, or skill id) / remove timer events by key | [better-auras.md](better-auras.md#2-aura-system-server) |
| D2R.exe | `0x438a70` (hook at `0x438a7a`) | Server skill select `(unit, isLeft, skill, a4)`: tears down the old right-skill aura | [better-auras.md](better-auras.md#1-interception-points) |
| D2R.exe | `0x43b680` | Start the right-skill aura `(game, unit, skill)`; proposed hook | [better-auras.md](better-auras.md#1-interception-points) |
| D2R.exe | `0x42e600` / `0x42e350` | Player regen tick (every frame; hpregen 74) / mana step (manarecovery 26); `0x42e600` proposed hook | [better-auras.md](better-auras.md#4-player-regen) |

This table is an index. The feature documents hold the guards and evidence; an entry here is not enough to justify
an unguarded hook.
