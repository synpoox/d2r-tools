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
- [Cartographer](cartographer.md): native addresses for the maze and the map item and red portal parts (signed; the item and portal parts validated in game on 93847 / D2RLoader 1.3.1-beta, the maze parts checked statically), and for the outdoor level builder (signed and exercised live by the throwaway spike); the rest is stubs from the pd2-resurrected research (maze steering, level regeneration) with no signatures or index rows yet; section 5 holds static engine checks for the maze slot (level rect, lookup, `0x3eb850` out parameter); section 6 holds static checks for map stats, region density and level, event entry points and the entry text path. Section 7 records the map stats parts (stat list functions and the monster setup hook; written, signed statically, not run in a game). Section 8 holds static answers for the outdoor builder (grid flags, teardown, portal arrival, Act 5 stamps, level size). Section 9 records the map events parts (the room populator hook and the two spawn calls; written, signed statically, not run in a game). Section 10 records where the red portal tooltip and the area name get their text (issue #90; static only). Section 11 records how an event `Spawn` name can be resolved from the game's compiled monstats and superuniques tables (issue #95; static only). Section 12 holds desk research for multiplayer map sync (issue #98: plugin messages, client level build, portal gate, kill credit; static and SDK only). Section 13 holds static checks for the Ambush, Treasure and Shrine events (issue #102: monster flee setup, TC drop and name map, chest loot path, object spawn recipe, boss `Drop`; static only). Docs: [docs/cartographer/](../cartographer/README.md).

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
| D2R.exe | `0x3eb850` / `0x3eb960` / `0x3e9820` / `0x3e77a0` / `0x32c6a0` | Cartographer maze: entrance and exit placer (hooked) / piece picker / ring grower (hooked) / add-room / LvlMaze lookup; byte signatures, static only (the two hooks are written, not run in game); `0x3e96d0` maze walk (hooked, signature in section 1a) is recorded there | [cartographer.md](cartographer.md#1a-maze-signatures-issue-29) |
| D2R.exe | `0x360bb0` / `0x360520` / `0x360470` / `0x360c80` | Cartographer maze: link two rooms / overlap checks; byte signatures, static only | [cartographer.md](cartographer.md#1a-maze-signatures-issue-29) |
| D2R.exe | `0x3271c0` | Level build `(dataFlag, level)`: dispatches on level type; type 3 goes to D2RCore. Hooked and replaced for a slot level in the Cartographer spike, and hooked for the maze slot level by the maze steering (written, not run in game; the outdoor builder must share that one hook) (signed; build 93847, stamp `0x6ab3782c` logged live at load) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x3ee320` / `0x3fe2e0` / `0x3ecfc0` / `0x3ee780` | Outdoor grid builder / Act 5 grid allocation / piece stamp `(dataFlag, level, cx, cy, lvlprest, file, 0)` / grid to preset rooms (signed) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x327650` / `0x327400` | Cartographer: level rect write at allocation `(level, row)` / world-space room lookup `(x, y)`, which can call `0x3271c0` lazily; unsigned, masked prefixes, static only | [cartographer.md](cartographer.md#5-maze-slot-engine-checks-issue-62) |
| D2R.exe | `0x3e2b5ce` -> `D2RCore.dll` | D2RLoader `FF 25` stub taken by `0x3271c0` for type 3; the act lookup stub `0x3e2b5c8` is the same kind (found by following calls, not signed) | [cartographer.md](cartographer.md#4-outdoor-s0-and-s1-spike-results-issue-22) |
| D2R.exe | `0x5817f0` / `0x581680` | Item spell check / use dispatchers `(game, player, item, target)`; Cartographer hooks (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x314110` / `0x349860` / `0x34b440` / `0x2efc10` | Item record `(dataFlag, row)` / unit class `(unit, file, line)` / unit room `(unit)` / room level id `(room)` (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x491960` / `0x5da800` / `0x5d88b0` | Client message `(player, 0x15, player)` / open red portal `(game, player, key, level, max)` / game counter `(game + 0x1aac8, &key) -> int*` (in game on 93847 / D2RLoader 1.3.1-beta) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x2f5020` / `0x34a040` / `0x517f90` | Unit stat `(unit, stat, layer)`, a D2RLoader thunk / unit coords `(unit, int xy[2])` / find free spot (in game on 93847 / D2RLoader 1.3.1-beta, success path only) | [cartographer.md](cartographer.md#3-phase-1-parts-the-map-item-and-the-red-portal-pluginscartographer-issue-20) |
| D2R.exe | `0x494a60` / `0x493cc0` / `0x49dd70` / `0x492cf0` | Cartographer: monster after-create fixup (hookable, one caller) / spawn core / monster level setter (table callback at `0x23964b0`) / spawn request (signed and called by the map events in [section 9](cartographer.md#9-map-events-entry-message-decision-boss-and-horde-issue-92); not run in a game) | [cartographer.md](cartographer.md#q1-applying-stats-to-monsters-the-player-and-the-map-item) |
| D2R.exe | `0x2f7300` / `0x2f72f0` / `0x2f7c00` / `0x2f2cb0` / `0x2f5b80` / `0x3354c0` | Cartographer: stat list create / set state / set stat (thunk) / attach (thunk) / find on unit / set state bit; masked prefixes, static only | [cartographer.md](cartographer.md#stat-functions-q1) |
| D2R.exe | `0x508f80` / `0x32c700` / `0x508740` / `0x50acf0` | Cartographer: monster level `(game, level, diff, ex)` / levels row MonLvl / region lookup (array at `game + 0x1d0`) / room populator (hooked in [section 9](cartographer.md#9-map-events-entry-message-decision-boss-and-horde-issue-92): signed, written, not run in a game) | [cartographer.md](cartographer.md#q2-overriding-monster-density-and-level-for-the-slot-level-only) |
| D2R.exe | `0x4042a0` / `0x408670` / `0x48f190` / `0x499ba0` / `0x441300` / `0x592c40` | Cartographer: room first activation (hookable) / per-game activation loop, which runs the same steps inline and never calls `0x4042a0` / preset loop, called by both paths (hookable, unique 21-byte prefix) / superunique (`0x499ba0` signed and called by the map events in [section 9](cartographer.md#9-map-events-entry-message-decision-boss-and-horde-issue-92); not run in a game) / treasure class roll / object operate dispatcher (slot table `0x23a51c0`) | [cartographer.md](cartographer.md#q3-entry-points-per-event-kind) |
| D2R.exe | `0x477a30` / `0x4016c0` | Cartographer: send chat packet / game chat copy; static only | [cartographer.md](cartographer.md#q4-server-to-client-text-for-entry-messages) |
| D2R.exe | `0x3faa50` / `0x3ede00` / `0x3edda0` / `0x3efaf0` / `0x3f95e0` | Cartographer outdoor: grid op `(grid, x, y, value, op)` / void cell setter (flag `0x100`) / clear cell / outline and border pass / per-room grid pass (not the outdoor flag grid); unsigned, masked prefixes, call-only or hookable per row, static only | [cartographer.md](cartographer.md#8-outdoor-builder-engine-checks-issue-83) |
| D2R.exe | `0x327c40` / `0x3ee220` / `0x3f9a90` / `0x3fa730` / `0x360730` | Cartographer outdoor: level teardown / outdoor free / outline list free / grid free / neighbour list free (NULL behaviour per function); unsigned, static only | [cartographer.md](cartographer.md#8-outdoor-builder-engine-checks-issue-83) |
| D2R.exe | `0x3feb80` / `0x3ff420` / `0x3feee0` / `0x3ff0e0` / `0x3fe9e0` / `0x3edfb0` | Cartographer outdoor: Act 5 barricade pass (level `0x6f`) / restamp at a cell / entrance and exit stamps / outline helper / cave entrance table walk / piece id at a cell; unsigned, static only | [cartographer.md](cartographer.md#8-outdoor-builder-engine-checks-issue-83) |
| D2R.exe | `0x3daad0` / `0x334ed0` / `0x402820` / `0x3e75bc` / `0x3277e0` / `0x32c360` | Cartographer: Vis exit lookup / tile to subtile (hookable) / `WarpToPreset` debug command handler and its consumers; unsigned, static only | [cartographer.md](cartographer.md#8-outdoor-builder-engine-checks-issue-83) |
| D2R.exe | `0x32c4a0` / `0xc7a30` / `0xc17e0` / `0xc0420` / `0x14751e0` / `0x34ad40` / `0x38fd00` | Cartographer (issue #90): levels row getter `(dataFlag, levelId)` (extended 82-byte prefix) / LevelName getter (call-only; misses the portal tooltip) / object hover text builder, where the portal tooltip reads the LevelName key at row `+0xfd` (hookable) / unit label builder / automap info text builder / portal destination level id (a jump into D2RLoader code widens it to 16 bits; the prefix is unique only through the loader's `E9` byte, so loader-patched and fragile) / objects row getter; masked prefixes unique, static only | [cartographer.md](cartographer.md#10-red-portal-tooltip-and-area-name-issue-90) |
| D2R.exe | `0x1471bc0` | Cartographer (issue #90): party info text builder (strings "PartyInfoText", "LocationText"); calls the LevelName getter `0xc7a30` at `0x1471dcd` (hookable, 14-byte cut, no relative operand; static only) | [cartographer.md](cartographer.md#10-red-portal-tooltip-and-area-name-issue-90) |
| D2R.exe | `0x3e2b736` / `0x3e2b730` | D2RLoader `FF 25` thunks into `D2RCore.dll`: text lookup by key (1217 callers) / by numeric id; call-only (issue #90) | [cartographer.md](cartographer.md#10-red-portal-tooltip-and-area-name-issue-90) |
| D2R.exe | `0x976e0` / `0x397220` / `0xa121f0` / `0x3a9050` / `0x3a9150` / `0x300a90` | Cartographer (issue #95): MonStats row getter (hookable) / SuperUniques row getter / name to row lookup `(map, name, logOnMiss)` / by-name resolvers for MonPreset and MonPet (load-time) / data tables base `(dataFlag)` (hookable); unique masked prefixes, the monstats `Id` is inline at row `+0` and the superuniques key only in the name map at `dt+0x1090`; static only | [cartographer.md](cartographer.md#11-event-spawn-names-from-the-games-compiled-tables-issue-95) |
| D2R.exe | `0x597030` / `0x4a2690` / `0x5cd060` / `0x34c2c0` / `0x34c730` | Cartographer (issue #102): monster flee setup `(game, source, target, r9, arg5, arg6)` (sets state 56 through a record and a D2RCore thunk, then AI mode 11; 3 callers; call-only) / AI set-mode `(game, monster, [monData+0x30], mode)` / AI step that returns to mode 0 when state 56 is gone / its two target checks; masked prefixes unique, static only. `0x596d40` is a spot search, not the flee setup | [cartographer.md](cartographer.md#13-ambush-treasure-and-shrine-events-static-checks-issue-102) |
| D2R.exe | `0x447b80` / `0x4404f0` / `0x5930b0` / `0x396430` / `0x58e2f0` | Cartographer (issue #102): monster death drop (hookable by the rule, 15 bytes) / TC roller `(game, a, b, compiled TC ptr, 6 stack args)` / chest TC pick, 28 callers, too broad to hook (hookable by the rule, 18 bytes) / chest TC lookup / chest operate handler, slot 4 of `0x23a51c0` (hookable by the rule, 16 bytes); the TC name map is `dt+0xd70` through `0xa121f0` into the `dt+0xd78` vector; masked prefixes unique, static only | [cartographer.md](cartographer.md#13-ambush-treasure-and-shrine-events-static-checks-issue-102) |
| D2R.exe | `0x4022a0` / `0x402f50` / `0x490410` / `0x38fc70` | Cartographer (issue #102): `SpawnObject` debug command handler (the object spawn recipe) / free-spot search (no unique masked prefix: byte-identical copy at `0x418d00`; find it from the call at `0x40239f`) / object create `(2, row, x, y, game, room, ...)` / objects row count `*(dt+0x1530)`; static only | [cartographer.md](cartographer.md#13-ambush-treasure-and-shrine-events-static-checks-issue-102) |

This table is an index. The feature documents hold the guards and evidence; an entry here is not enough to justify
an unguarded hook.
