# Complete Death Recap

Continue implementation on `death-recap/telemetry`. Read `AGENTS.md`, the existing plugin and tests, [capture architecture](../../docs/reverse-engineering/death-recap.md), and [verified RE contracts](../../docs/reverse-engineering/death-recap-static.md). Preserve existing work.

**Target:** D2RLoader 1.3.1, image `0x6ab3782c`, shared by D2R 3.3 (93847) and 3.4 (95032). Use the existing `D2RLoader_6ab3782c_dump.exe`, Ghidra, d2rAI-toolkit and pinned PluginSDK. No separate game dump or D2RCore dump is a prerequisite. See [compatibility evidence](https://github.com/synpoox/pd2-resurrected/blob/main/docs/agents/game-slots.md).

1. **Implement authoritative capture first.** Start from the verified damage, resource, missile and player-death contracts. Measure exact fixed-point life/mana changes, deduplicate nested events, preserve source attribution and link the fatal event. Resolve remaining coverage through targeted RE and controlled game-slot tests; don't repeat completed research.
2. **Complete mechanic coverage.** Capture direct and persistent damage, poison, Open Wounds, Crushing Blow, reflection, absorption, Energy Shield, healing, potions, regeneration, defensive stats and active states. Separate pre/post mitigation, actual loss and overkill. Mark unsupported or inferred fields explicitly.
3. **Finish the plugin.** Native automatic death recap panel, configurable 10-second rolling window, elemental/source/ability breakdowns, timeline, life graph, evidence-based observations, localized active-mod data, retained local history and JSON export.

Follow existing architecture, per-part signature/ownership guards, preallocated capture and immutable reports. Preserve gameplay and plugin coexistence; support offline and supported modded TCP/IP without claiming unavailable host telemetry. Build on Windows, run automated and live tests, document actual coverage and limitations, and commit/push the implementation. Do not stop at scaffolding, merge or release without approval.
