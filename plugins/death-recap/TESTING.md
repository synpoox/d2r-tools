# Death Recap testing

## Synthetic UI preview

In a game, run `death-recap preview` in the loader console. This constructs a display-only report and opens the real
native panel. The title explicitly says **SYNTHETIC PREVIEW (no character death)**. It never writes game resources,
enters the capture feed, changes the death latch, or saves/exports a synthetic report. `death-recap open` and F8 return
to actual history. Escape dismisses; Page Down/Page Up navigate the preview timeline.

The fixture contains ten seconds of HP samples, five synthetic hits, two heals, mana losses, and a synthetic fatal
marker. Its tests verify independent resource reconciliation and the explicit killing-blow link. These amounts are
not observations from the character running the preview.

## 2026-11-27 smoke check

Environment: isolated test Slot11, D2R **3.4.0 / 95032**, D2RLoader **1.3.1-beta**, engine image `0x6ab3782c`,
1920x1080, Hell Harrogath, existing level-99 RotW test barbarian. No combat or character death was induced.

Compared the same peer set with and without Death Recap:
Better Auras, Better Automap, Better XP Bar, Cartographer, DPS Counter, MultiSpec, Player Bars, Quick NPC,
Scrollable Waypoints, plus the test harness. The peer binaries were existing local builds; this is not a release-suite
qualification or a PD2-core coexistence check.

| Check | Result |
|---|---|
| Test mod uses its isolated slot save path | PASS |
| Death Recap loads with nine peers | PASS; native panel ready, parts `0xff`, capture available, zero dropped events |
| Each capture part installs | PASS; LifeDebit, ManaDebit, LifeRegen, ManaRegen, Mode, Hit, Effects, Heal all on |
| Console preview appears in the real game | PASS; synthetic label, graph, subtotals and timeline visible |
| Preview text fits the panel at 1920x1080 | PASS after widening the panel and shortening labels |
| Escape dismisses and console preview reopens | PASS |
| Page Down advances to timeline page 2 | PASS |
| Character remains alive and resources unchanged across previews | PASS; life 14080/14080 raw, mana 2560/2560 raw, living mode 5 |
| History remains unchanged | PASS; existence/hash unchanged |
| Warning/error comparison including shutdown | PASS; zero new messages relative to baseline |
| Crash files | PASS; no new files |
| Cleanup | PASS; stopped game, restored slot plugins/config/mod data/test saves, released lock |
| Actual death capture, combat accounting and fatal ordering | SKIPPED; owner requested no character death |
| Controller interaction, other resolutions, TCP/IP and PD2 core plugins | SKIPPED; outside this quick check |

The baseline is **not error-free**. It has a Better Auras config warning, four Cartographer missing-data warnings,
and four existing item assertions from the test save without its prior map data. The comparison has thirteen
normalized entries (warning/error lines plus assertion messages), unchanged with Death Recap installed. This checks
for newly introduced diagnostics; it does not resolve those existing fixture problems.

The slot initially contained a Cartographer data fixture whose missing localization key prevented startup. Its data
was backed up and temporarily replaced with an empty test mod for both comparison runs. All original slot files
and test saves were restored afterward. The real game install and its characters were not changed.

Screenshots, binary identities, raw logs and comparison JSON remain outside the repository in the local smoke-test
run. No screenshot, save, dump or log is committed. The screenshot delivered to the owner shows the synthetic preview.

## Automated validation

`tools/build.ps1 -Plugin death-recap -Test`: DLL build and all three host test executables pass after the preview and
layout fixes. These checks cover telemetry/summary, preview/accounting/settings/history, and production adapters with
fake engine originals. They do not substitute for the skipped combat and multiplayer scenarios.
