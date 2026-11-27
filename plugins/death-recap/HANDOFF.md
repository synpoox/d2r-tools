> Native UI update (2026-11-27): the mockup-driven recap, three collapsed detail sections, native texture/clipping,
> damage bars, step graph, scrolling, keyboard/controller mappings and post-resurrection opening are implemented.
> See TESTING.md for real-death and visual refinement evidence. The earlier broad capture acceptance matrix below
> remains outstanding; unavailable fatal/element/ability evidence is still represented honestly.

# Death Recap â€” implementation handoff

Continue the D2RLoader Death Recap plugin on `death-recap/telemetry` in `synpoox/d2r-tools`. Do not restart or repeat completed RE. The portable core now has native call-site capture, optional context/healing hooks, local death observation, a native panel, controller/keyboard controls, active-table names, bounded history and export. See [implementation evidence](../../docs/reverse-engineering/death-recap-implementation.md) for exact scope and tests. The initial pass skipped game checks; a subsequently authorized [loading/coexistence and synthetic UI smoke check](TESTING.md) passed without a character death. Real capture acceptance and full mechanic coverage remain outstanding. Do not label the plugin complete or release-ready.

## 1. Resources and setup

- Read [AGENTS.md](../../AGENTS.md), [CONTRIBUTING.md](../../CONTRIBUTING.md), the plugin [README](README.md), `src/telemetry.h`, `src/summary.h`, `src/plugin.cpp`, `tests/telemetry_test.cpp`, [capture architecture](../../docs/reverse-engineering/death-recap.md), and [verified static contracts and signatures](../../docs/reverse-engineering/death-recap-static.md). The static findings supersede the earlier candidate table.
- Target D2RLoader 1.3.1's engine image `0x6ab3782c`, shared by D2R 3.3 (93847) and 3.4 (95032); see [compatibility and game-slot evidence](https://github.com/synpoox/pd2-resurrected/blob/main/docs/agents/game-slots.md). Use active game/mod data, not hardcoded table names/rows.
- Existing dump: `D2RLoader_6ab3782c_dump.exe` in the documented Ghidra workspace (`C:\ghidra-work`); Ghidra project `project\D2R.gpr`. If absent locally, the owner has `D2RLoader_6ab3782c_dump.zip` under Google Drive's `D2R Dumps/3.3`. The static RE document records the analyzed image's hash and size. No new or separate D2RCore dump is a prerequisite.
- Use [d2rAI-toolkit](https://github.com/synpoox/d2rAI-toolkit), its `knowledge/d2r-93847-addresses.md`, `knowledge/engine-re.md`, `tools/ghidra_query.ps1`, and test-slot tooling. Read the pinned `third_party/PluginSDK` headers in d2r-tools (initialize submodules); do not change the pin. Study [Player Bars](../../docs/reverse-engineering/player-bars.md) and its source for native UI, [DPS Counter](../../docs/reverse-engineering/dps-counter.md) for combat context, and existing guarded mutation/config/lifecycle patterns. Follow the [interception workflow](../../docs/reverse-engineering/RVA-AND-INTERCEPTION-WORKFLOW.md).

## 2. Implement native capture first

1. Replace the inert entry point with independently guarded capture parts and real config/lifecycle registration. Establish authoritative local-player identity per life/game, producer thread, baseline, reset and unload. Distinguish offline/host from joined-client replicated state.
2. Start with the verified life debit `0x44d093`, mana debit `0x44d0c9`, life/mana regen commits `0x42e6d1 / 0x42e540` and player-death dispatch `0x42d020` (mode 0, fatal call `0x44af4a`; cleanup `0x42e7c0`). Do not use monster death `0x44db20` as the player detector or report mode-17 follow-up twice. Read stats 6/7/8/9 via verified getter/SDK on the correct unit. Preserve the original ABI, return and call count.
3. Measure before/after and isolated gross HP/mana mutations in raw 1/256 units. Gather attempted damage, mitigation stages, attacker/skill/missile metadata using a bounded scoped transaction context; a stat-write callback alone does not receive the damage block. Carry metadata across missile-to-owner replacement and nested events without double counting. Capture absorption before direct debit and handle fractional-zero flush and overkill as documented. Enrich events without recording a second loss; publish one confirmed death marker after relevant mutations finish.
4. Use the existing `recap::Rolling`, `Event`, `KillingBlow` and `Summarize` contracts; extend them as necessary (cause/provenance, explicit coverage, resource boundary anchors, per-life IDs and health graph). Preserve confirmed/inferred/unavailable fields and explicit unknown buckets. No engine pointers in reports; no allocation, logging, disk or UI on damage callbacks. Freeze on the serialized producer or after quiescence, then hand immutable reports to UI and storage.
5. Check signature uniqueness, whole instructions, original target and runtime mutation ownership. Use SDK mutations and per-part guards, not a version gate; add a repeatable dump checker. Do not compete with DPS Counter, Cartographer, Better Auras or PD2-owned sites, overwrite foreign detours or replay gameplay. Choose unowned equivalent call sites or a cooperative SDK observation path; disable only the affected part if unsafe.

## 3. Close coverage gaps and verify capture

Use the existing image, targeted disassembly and controlled game-slot traces; follow live targets when necessary. Extend the feed for poison, Open Wounds, Crushing Blow, persistent/ground/reflected/self damage, absorption, Energy Shield, mana burn/costs, healing, leech, potions and regeneration. Capture source/owner/skill/missile/modifier IDs, effective defenses, curses, auras and states before death cleanup. Do not equate poison rate to instant damage, mana drain to Energy Shield, net change to gross loss/healing, or current skill to projectile provenance.

For each scenario record expected and observed raw endpoints, event ordering, attribution and coverage. Test each element, mixed hits, avoid/block, resistance/DR, shield with insufficient mana, simultaneous healing/damage, overkill, nested and same-frame hits, overlapping persistent effects, max-resource changes, missing/despawned sources, remote vs local deaths, death without a direct hit, respawn and rejoin. Test overflow, missing events, signature/ownership failure and unload. Require one recap per local death; reconcile HP and mana independently (end - start = restored - lost + unresolved net). Accounting equality alone does not prove complete capture. Never manufacture certainty.

Offline and supported modded TCP/IP are the scope. Validate host and joiner separately; where a joiner cannot observe authoritative data, provide clearly labeled partial reports or a recipient-scoped protocol through supported SDK roles if appropriate. Do not claim unsupported remote telemetry.

## 4. Finish the player-facing plugin

- Native D2R-style auto-open death recap panel; dismiss/reopen latest, history navigation, keyboard/controller and revive compatibility. Use SDK panel/widget/resource/thread services and plugin-owned UI; no mod-specific layouts required.
- Configurable rolling window (10 s default), killing blow, attacker/ability/modifiers, timeline and HP graph, source/skill/element pre/post breakdown, actual HP/mana loss and restoration, overkill, defense/state snapshot and evidence-backed observations. Distinguish unavailable from zero and show partial coverage visibly.
- Resolve localized names from active compiled tables and SDK localization; copy stable data before asynchronous use. Versioned local history with atomic writes, configurable bounded retention, clear-history and JSON export; keep files outside character saves. Handle corrupt/missing history without affecting gameplay. Consume embedded TOML settings rather than leaving placeholders.

## 5. Definition of done

Build from the repo root with VS2022/CMake and the pinned SDK:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin death-recap -Test
```

Extend portable tests for adapter accounting, ordering, death deduplication, window boundaries, summaries, persistence and partial coverage. Run the dump checker, clean Windows build, install into an isolated test slot, and execute the capture matrix plus panel/history/controller and coexistence checks. Test both supported game-data versions when available. Record actual results separately from untested cases.

A working DLL must collect real incoming events, open exactly one useful recap per local death, preserve gameplay, and retain/reopen reports. Do not label the plugin complete while capture is inert or claim full coverage without evidence. Update README/config documentation, the RE registry and changelog fragment; commit and push the implementation on this branch. No merge/release or game binaries/dumps/saves/logs committed.
