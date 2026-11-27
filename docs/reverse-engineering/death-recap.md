# Death Recap: capture investigation and validation gate

Date: 2026-11-27. Consumer: death-recap. Target: D2RLoader 1.3.1 engine image 0x6ab3782c, shared by D2R 3.3 (93847) and 3.4 (95032); see the verified static contracts for image identity.

**Evidence level:** Initial desk audit below is retained for context. Follow-up static analysis independently verified the target executable's instructions, resource commit points, player death path and unique masked signatures. See [verified static capture contracts](death-recap-static.md) for the authoritative updated findings and [implementation handoff](../../plugins/death-recap/HANDOFF.md). The old candidate table is historical: in particular, 0x44db20 alone is not player death detection, and its proposed 24-byte guard is not unique. The [native adapter implementation](death-recap-implementation.md) now enables guarded narrow capture and a recap UI/history; live game validation and full mechanic coverage remain outstanding.

## Candidate contracts

| Location | Reported contract and evidence | Guard / remaining work |
|---|---|---|
| 0x44ce80 | Hit path, reported (game, attacker, target, 1, damage*); caller of hit-results path and damage effects. See [DPS Counter](dps-counter.md) and toolkit address registry. | Trace all call sites, all stack arguments, return, life/mana writes and recursive damage. No validated entry signature for this consumer. |
| 0x44a9b0 | Hit results after life deduction, reported (game, attacker, victim, damage*); record +0x134 total in 1/256 units. Existing DPS Counter uses it; 35 call sites documented, not live validated there. | DPS Counter's unique pattern and 25-byte overwrite already in its docs. Cannot recover pre-hit life by wrapping this function. Do not install a competing hook; investigate shared event service or distinct commit point. |
| 0x44df10 | DamageEffects, reportedly calls twelve damage-type steps, with D2RCore thunk at 0x3e2aab0. | Trace resistance, mitigation, absorb and mana diversion stages. Do not equate a record field with realized HP loss. |
| 0x44d570 / 0x5881e0 | Event wrapper / dispatcher; latter reportedly jumps via D2RCore slot 0x3e2a420. Some attacker events occur before damage. | ExecuteEvents name and contract are not independently established. Do not replace an occupied entry or slot. Investigate semantic services and safe independent observation. |
| 0x44db20 | Unit death, reportedly (game, victim, killer or null, flag); already-dead mode early return. | Reported whole-instruction guard: 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50. Verify guard uniqueness and pre/post death transition, local player identity, and nested death ordering. |
| 0x42e600 / 0x42e350 | Player life regen event and mana step, see [Better Auras](better-auras.md). Regen reschedules every frame, even for dead players. | Trace poison, Open Wounds, heal, potion and passive regen separately. Respect Better Auras/Cartographer coexistence. |
| 0x451820 | Toolkit labels AddLife; called by absorb heal and life leech. | Determine whether and when it clips, and whether other heal writers bypass it. |
| 0x2f7d10 / 0x2f34f0 | Toolkit labels set/add stat. | Trace every resource writer, including D2RCore alternatives. Check hot-path impact; do not indiscriminately hook all stats. |
| 0x490300 | Missile owner lookup, (game, missile) -> owner, from toolkit and DPS Counter. | Validate chain depth, ownership across despawns, stale IDs and unknown owners. |
| 0x2f5020 | Read total stat, reportedly (unit, stat, layer). [Player Bars](player-bars.md) uses IDs 6/7/8/9 for life/max life/mana/max mana in 1/256 units. | Validate server versus client unit semantics. Player Bars documented client resource updates coalesced around 0.8 s during potion healing; polling is not authoritative per hit. |
| CalculateTotalDamage | Name proposed by request; no independently verified D2R RVA. | Map against classic reference behavior using disassembly, not by name or assumed layout. |

The toolkit describes a 0x180-byte damage block with flags, small list and callback storage. Its leech notes reference several damage fields and mana/stamina drains, but this does not establish a complete damage-element layout. Do not copy classic D2DamageStrc offsets into the adapter. Trace individual reads/writes and validate against controlled hits.

## Capture architecture and gate

1. Identify the local player per session using SDK lifecycle identity and verify matching authoritative server unit and game context. Never use unit type alone, never count other players. Reset per game/life, handle leave/rejoin, respawn and unit ID reuse. Distinguish host, joiner and offline. No Battle.net online play.
2. Begin with read-only controlled instrumentation of the actual resource commit path, resource endpoints and confirmed death transition. Save before/after fixed-point life/mana, max resources, per-operation gross mutations, and validated damage stages. Do not infer gross loss or overkill from a net delta. Call original exactly once with unchanged arguments/results.
3. Trace nested callbacks: assign a canonical transaction ID at the committed operation, track parent/child causality, and count each mutation once. Event callbacks and hit result records enrich metadata, never add a second loss. Death may be nested inside the hit; defer snapshot publication until the fatal transaction and all its relevant mutations finish.
4. Use a preallocated, single-writer ring (already implemented). Marshal a frozen const report to UI/persistence workers; no engine pointers, allocations, disk I/O, UI calls or logging on the capture path. If producer threading differs, implement explicit handoff and test it.
5. Per field, retain confirmed / inferred / unavailable and provenance. Never use zero for missing measurements. Reconcile end minus start = restored minus lost + unresolved net separately for life and mana. Missing endpoints, sequence gaps, overwritten events and partial windows must invalidate completeness. The core detects duplicate/nonmonotonic sequences, sequence gaps, absent exact-window baseline and missing death-end samples. The exact boundary rule is deliberately conservative: an event must establish confirmed resource endpoints at the cutoff; a future adapter can supply a proven continuous-stream boundary. Complete() is accounting-only, not proof of full capture. KillingBlow() requires an explicit confirmed death-to-damage sequence link; the last hit is not sufficient.
6. Resolve active compiled tables and localization via SDK DataTableService and LocalizationService; copy stable IDs and strings off hot path. Use a versioned schema and explicit unknowns for unsupported fields. Snapshot defenses using the core's stat ID/layer/value records and bounded state lists with truncation flags. Validate those IDs and lifetimes.
7. TCP/IP clients may see only replicated resource snapshots; do not claim host-only damage internals. If a supported host/client protocol is needed, verify SDK network roles and compatibility; send only the recipient's bounded report. Never require gameplay modification. Fail closed on unavailable authoritative data and fail open for gameplay.
8. Keep the panel native via the SDK resource, panel, widget and thread services. Player Bars documents successful native panels and unsuccessful OverlayService drawing in its tested build; verify modern SDK services against the pinned SDK. Keep the death screen, revive and controller input functioning.
9. Persist versioned immutable reports atomically outside saves, bounded retention, no credentials/account details, optional JSON export. Defensive observations use confirmed states/stats and measured damage only. Unknown element/attacker/ability must have an explicit unknown bucket. Don't apportion actual life loss by element without evidence.

## Live proof-of-concept checklist

Use the existing toolkit Ghidra workflow and an isolated test game slot. Reproduce entry guards with Disasm/RangeAsm, xrefs and decompilation for candidate functions. Inspect RCX/RDX/R8/R9 and stack argument stores at every call site; check D2RCore and competing plugins before hooking. Record target image SHA-256, unique signature matches, whole-instruction lengths, call contracts, thread, and failure path for each enabled part. Prefer per-part byte guards, not blanket game-version gates, per AGENTS.md.

Controlled fixture sequence:
- Physical and each elemental hit; mixed damage; resistance/DR; block/avoid; absorption including simultaneous heal/loss; Energy Shield including insufficient mana; mana burn.
- Poison, Open Wounds, persistent ground effects, Crushing Blow, reflection, overlapping effects; distinguish separate damage ticks and unknown source.
- Natural/potion/rejuvenation healing, life leech, mana costs and regeneration; compare exact raw endpoints at every mutation.
- Multiple players, owned missiles/minions, missing or despawned attacker, local vs remote deaths, multiple hits in a frame, death within nested callbacks, re-entry and respawn.
- Unsupported build, pattern zero/multiple matches, hook conflict, overflow, missing events, shutdown and queued UI callbacks. Verify no game behavior changes.

Gate before native UI: repeatable fixture traces with exact life/mana conservation and no duplicate events, documented missing telemetry, death identity and fatal-transaction ordering verified. Validate offline, TCP/IP host and TCP/IP join separately. No assertion of supported remote telemetry until demonstrated.

## Validation status

- Game-independent C++20 tests passed on Linux with GCC and Clang, -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG, and GCC with AddressSanitizer and UndefinedBehaviorSanitizer. Committed source/test blob hashes matched tested copies. Aggregation tests check separate origin/attacker/skill unknown buckets and element coverage.
- Windows/MSVC DLL build: now passes with installed VS2019; SDK pin inspected and unchanged. See the implementation evidence for current tests.
- Target image static analysis: completed for the contracts and patterns in [death-recap-static.md](death-recap-static.md). Alternate mutation/event paths and live game-slot behavior still require validation; no separate dump is a prerequisite. Narrow native capture, UI and persistence are implemented; full mechanic coverage remains incomplete.
- Fail-open behavior: development DLL installs each independently guarded capture part through SDK mutations. Missing/owned signatures disable only that part; every report is marked partial. No runtime game checks; do not distribute as a validated release.
