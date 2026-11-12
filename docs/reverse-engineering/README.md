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
- [Cartographer](cartographer.md): native addresses for the maze and the map item and red portal parts (signed; the item and portal parts validated in game on 93847 / D2RLoader 1.3.1-beta, the maze parts checked statically), and for the outdoor level builder (signed and exercised live by the throwaway spike); the rest is stubs from the pd2-resurrected research (maze steering, level regeneration) with no signatures or index rows yet; section 5 holds static engine checks for the maze slot (level rect, lookup, `0x3eb850` out parameter); section 6 holds static checks for map stats, region density and level, event entry points and the entry text path. Section 7 records the map stats parts (stat list functions and the monster setup hook; written, signed statically, not run in a game). Docs: [docs/cartographer/](../cartographer/README.md).

The pre-registry documents above (all except Quick NPC and the Cartographer stub) keep their original format; bring each to the
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
| D2R.exe | `0x42e600` / `0x42e350` | Player regen tick (every frame; hpregen 74) / mana step (manarecovery 26); `0x42e350` hooked by Cartographer (PlayerTick; unit id matched `LocalPlayerReady.playerId` in game on 93847 / D2RLoader 1.3.1-beta), `0x42e600` hooked by Better Auras | [better-auras.md](better-auras.md#4-player-regen) |
| D2R.exe | `0x3eb850` / `0x3eb960` / `0x3e9820` / `0x3e77a0` / `0x32c6a0` | Cartographer maze: entrance and exit placer / piece picker / ring grower / add-room / LvlMaze lookup; byte signatures, static only | [cartographer.md](cartographer.md#1a-maze-signatures-issue-29) |
| D2R.exe | `0x360bb0` / `0x360520` / `0x360470` / `0x360c80` | Cartographer maze: link two rooms / overlap checks; byte signatures, static only | [cartographer.md](cartographer.md#1a-maze-signatures-issue-29) |
| D2R.exe | `0x3271c0` | Level build `(dataFlag, level)`: dispatches on level type; type 3 goes to D2RCore. Hooked and replaced for a slot level in the Cartographer spike (signed; build 93847, stamp `0x6ab3782c` logged live at load) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x3ee320` / `0x3fe2e0` / `0x3ecfc0` / `0x3ee780` | Outdoor grid builder / Act 5 grid allocation / piece stamp `(dataFlag, level, cx, cy, lvlprest, file, 0)` / grid to preset rooms (signed) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x327650` / `0x327400` | Cartographer: level rect write at allocation `(level, row)` / world-space room lookup `(x, y)`, which can call `0x3271c0` lazily; unsigned, masked prefixes, static only | [cartographer.md](cartographer.md#5-maze-slot-engine-checks-issue-62) |
| D2R.exe | `0x3e2b5ce` -> `D2RCore.dll` | D2RLoader `FF 25` stub taken by `0x3271c0` for type 3; the act lookup stub `0x3e2b5c8` is the same kind (found by following calls, not signed) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x5817f0` / `0x581680` | Item spell check / use dispatchers `(game, player, item, target)`; Cartographer hooks (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x314110` / `0x349860` / `0x34b440` / `0x2efc10` | Item record `(dataFlag, row)` / unit class `(unit, file, line)` / unit room `(unit)` / room level id `(room)` (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x491960` / `0x5da800` / `0x5d88b0` | Client message `(player, 0x15, player)` / open red portal `(game, player, key, level, max)` / game counter `(game + 0x1aac8, &key) -> int*` (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x2f5020` / `0x34a040` / `0x517f90` | Unit stat `(unit, stat, layer)`, a D2RLoader thunk / unit coords `(unit, int xy[2])` / find free spot (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x494a60` / `0x493cc0` / `0x49dd70` / `0x492cf0` | Cartographer: monster after-create fixup (hookable, one caller) / spawn core / monster level setter (table callback at `0x23964b0`) / spawn request; unsigned, static only | [cartographer.md](cartographer.md#q1-applying-stats-to-monsters-the-player-and-the-map-item) |
| D2R.exe | `0x2f7300` / `0x2f72f0` / `0x2f7c00` / `0x2f2cb0` / `0x2f5b80` / `0x3354c0` | Cartographer: stat list create / set state / set stat (thunk) / attach (thunk) / find on unit / set state bit; masked prefixes, static only | [cartographer.md](cartographer.md#stat-functions-q1) |
| D2R.exe | `0x508f80` / `0x32c700` / `0x508740` / `0x50acf0` | Cartographer: monster level `(game, level, diff, ex)` / levels row MonLvl / region lookup (array at `game + 0x1d0`) / room populator (call-only); static only | [cartographer.md](cartographer.md#q2-overriding-monster-density-and-level-for-the-slot-level-only) |
| D2R.exe | `0x4042a0` / `0x408670` / `0x48f190` / `0x499ba0` / `0x441300` / `0x592c40` | Cartographer: room first activation (hookable) / per-game activation loop, which runs the same steps inline and never calls `0x4042a0` / preset loop, called by both paths (hookable, unique 21-byte prefix) / superunique / treasure class roll / object operate dispatcher (slot table `0x23a51c0`); unsigned, static only | [cartographer.md](cartographer.md#q3-entry-points-per-event-kind) |
| D2R.exe | `0x477a30` / `0x4016c0` | Cartographer: send chat packet / game chat copy; static only | [cartographer.md](cartographer.md#q4-server-to-client-text-for-entry-messages) |

This table is an index. The feature documents hold the guards and evidence; an entry here is not enough to justify
an unguarded hook.
