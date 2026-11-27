# Agent handoff — complete Death Recap

Continue on `death-recap/telemetry` in `synpoox/d2r-tools`. Read `AGENTS.md`, this plugin's README/source/tests, [capture architecture](../../docs/reverse-engineering/death-recap.md), and **[verified static capture contracts](../../docs/reverse-engineering/death-recap-static.md)** first. The latter supersedes earlier candidate-only assumptions. Do not restart the plugin or repeat the completed static RE.

## Current state and available evidence

- Portable telemetry model, rolling window, snapshots, summaries, evidence classification, and synthetic tests exist. The plugin DLL intentionally installs no native capture hooks. Windows build and live tests remain unverified.
- Independently disassembled the target D2RLoader 0x6ab3782c unpacked PE and checked unique masked patterns across all executable sections. Verified exact HP/mana commit sites, absorption healing contract, elemental calculation boundary, regen commits, player death mode dispatch and cleanup, and projectile owner replacement. No proprietary binary is committed. Use the owner's local matching dump/toolkit rather than requesting a new dump merely to repeat these findings.
- Player death is NOT adequately detected at 0x44db20: the direct player fatal branch calls 0x42d020 at 0x44af4a with mode 0; cleanup 0x42e7c0 zeroes HP. Mode 17 follows and must not generate a second report.
- The main stat readers/writers and event dispatcher are D2RCore thunks. Existing plugins/PD2 own several interception points. Full mechanics coverage needs matching D2RCore evidence and live probes, not more speculation about the already verified main PE.

## Priority 1 — implement and validate a native capture proof of concept

Use the pinned PluginSDK, d2rAI-toolkit, target game install and verified contract catalog. Build the plugin on Windows. Start with unowned direct HP debit (0x44d093) and mana debit (0x44d0c9) call sites, life/mana regen commits, and guarded death observation (0x42e7c0 or independently verified mode observer). Use SDK mutation ownership and expected-original-target checks. Do not hook occupied entries, replace D2RCore jump slots, or copy game/mod-specific code into this plugin. Verify all callbacks preserve return values, original calls and thread identity. Disabled capture must be harmless.

Observe raw fixed-point before/after at each committed operation, not merely requested values. Confirm local player via lifecycle/game identity, reset on session/life changes, and correlate parent/child operations without adding the same mutation twice. Sample attempted post-mitigation debit separately. Observe the actual mitigation stage and attack/missile provenance where unowned; distinguish unknowns when unavailable. Handle the fractional-zero threshold and absorption heal-before-debit ordering. Link fatal sequence explicitly; freeze only after enclosing transaction completes. Test local deaths, remote deaths, respawn, repeated cleanup and unload. Keep hooks disabled by default until controlled live fixtures demonstrate correctness.

## Priority 2 — close remaining coverage gaps

Acquire or inspect the matching D2RCore implementation and actual running hook ownership (including PD2, DPS Counter, Cartographer and Better Auras). Identify alternate resource writers and their call contracts; do not install invasive generic stat hooks simply to claim completeness. Prefer shared, read-only observers offered by cooperating owners if an interception is already taken.

Validate poison, OW, overlapping regen/potions, Crushing Blow, reflected and persistent damage, ES/absorption, leech, mana costs, max-resource changes, blocked/avoided attacks, death without a direct hit, and both host/client environments. A regen net delta is not proof of gross damage/healing; a mana debit is not proof of ES. For each field maintain confirmed/inferred/unavailable provenance. Client replicated values may be sampled but are not authoritative host combat internals; if supporting cooperating TCP/IP hosts, use a recipient-specific bounded protocol only with supported SDK roles. No Battle.net online support. Record controlled raw traces, exact resource conservation, duplicate suppression and actual coverage. Do not label the feed complete just because endpoints reconcile.

## Priority 3 — finish the requested feature

After the validated capture gate, complete:
- configurable rolling window (10 s default), resource timeline, immutable death snapshot, final blow/attacker/origin/missile/skill/modifier identities;
- pre/post mitigation, actual gross HP/mana loss and restoration, overkill with correct clipping semantics, source/ability/element summaries with explicit unknown buckets, defense/state/curse/aura snapshots and evidence-based observations;
- active compiled tables and localization, native D2R-style death panel, timeline and life graph, revive/controller compatibility;
- local atomic versioned history, JSON export, configurable retention, no game assets or private data committed;
- per-part byte guards, graceful degradation, bounded single-writer capture, off-thread immutable report/UI/persistence, no gameplay changes.

Run the existing and extended portable tests, Windows DLL build, and controlled game-slot tests. Add fixtures for every claimed mechanic and failure mode. Document observed contracts, platform/build/mod scope and missing fields in the RE registry. Update README/status as functionality becomes real. Commit and push the completed work to this branch; do not merge or release without approval.
