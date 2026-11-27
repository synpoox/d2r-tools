# Death Recap testing

## Synthetic UI preview

In a game, run `death-recap preview` in the loader console. This constructs a display-only report and opens the real
native panel. The header explicitly labels the report **Synthetic preview - no character death**. It never writes game resources,
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


## 2026-11-27 native UI refinement and real-death checks

Environment: reserved Slot11, D2R 3.4.0 / 95032, D2RLoader 1.3.1-beta, engine image `0x6ab3782c`.
The slot's existing level-99 RotW softcore test barbarian was used. Its plugins, config, mod data and test saves
were backed up before the run. The pre-existing Cartographer fixture was temporarily disabled and its mod data
parked, leaving the test harness and Death Recap with an empty isolated mod. These checks do not qualify the
full release suite. The owner's game install and characters were not used.

### Automated and static results

- Clean Windows x64 DLL build passed, followed by `tools/build.ps1 -Plugin death-recap -Test`: **4 passed,
  0 failed, 0 skipped**. Installed compiler: VS2019/MSVC 19.29, SDK 10.0.19041; VS2022 is unavailable.
- Added presentation tests cover element sorting/subtotals, graph gaps, healing, smaller graph widths, UTF-8-safe
  shortening, wrapping, expansion and scroll bounds. Persistence tests cover format-2 context round trips and
  reading a reconstructed format-1 report. Native adapter tests verify captured level/area and the 16-stat snapshot.
- Dump checker passed **20/20** unique capture/UI/context witnesses, including image clip offset `0xac`.
- Slot save-path harness check: **1 passed, 0 failed, 0 skipped**.

The machine's unconfigured `py.exe` launcher makes the build script's Python discovery fail after compilation/tests.
For this run only, an ignored `build/tool-bin/py.cmd` delegates to the installed `python`; prepending that directory
to PATH lets the unchanged repository build script finish normally.

### Visual passes

Repeated build/install/restart/screenshot cycles compared the real native panel directly with the supplied mockup.
The passes corrected panel proportions and typography; the loader's 256-widget-handle limit; stock texture/frame
composition; missing graph/section updates; empty-value overlap; default expansion and navigation; long-label
ellipsis and detail wrapping; sub-pixel graph disappearance; fractional-scale fill striping; and section-border
reflow at scroll boundaries. Final graph fills combine only adjacent columns with identical observed display
heights. They do not smooth or synthesize resource values. Healing markers require confirmed gross restoration.

| Live check | Observed result |
|---|---|
| Default layout | Killing blow above side-by-side damage/life panels, three collapsed rows, one centered Close button |
| Native rendering | Stock modal texture, nine-slice section borders, D2R fonts and native modal button; no overlay drawing |
| Damage bars | Synthetic four-element report sorted lightning/fire/physical/poison, proportional bars and percentages |
| Graph | Synthetic step trace, subtle red fill, two green healing markers, window timestamps and resource scale |
| 1920x1080, 1280x720 and 1280x960 | Collapsed and expanded layouts fit; graph remains visible and uniform after resizing |
| Long labels | Native ellipsis contains area/section titles; shortened attacker heading; full detail text wraps across pages |
| Expansion | Mouse and Enter toggles work; W/S changes selection; one, two and three expanded sections remain within the body |
| Scrolling | Mouse-wheel window message and Home/End scroll the viewport; partial section frames reflow at the boundary |
| Paging | Posted native Page Down advances the active detail page; section changes reset paging |
| Close / Escape / F8 | Mouse Close and Escape dismiss; F8 reopens the latest real report with sections collapsed |
| Real deaths | Three controlled native killme deaths and one Diablo combat death produced four retained reports, no duplicates |
| Combat resource evidence | Diablo report measured 14080 raw life lost (55), 55880 raw post-mitigation damage (218.28125), zero dropped events |
| Incomplete attribution | Combat report retained localized Diablo attribution in details; no explicit fatal link or element evidence was promoted into a killing blow |
| Saved context | Real reports retained level 99 and localized Harrogath, Blood Moor and Rogue Encampment names |
| Final native snapshot | Export confirmed level 99, Rogue Encampment and 16 defense stat entries |
| Death / resurrection | Stock death prompt remains unobscured; after its normal Continue action, recap opens automatically |
| Diagnostics | All eight capture parts enabled, panel ready, no Death Recap warnings/faults in final startup and UI run |
| Cleanup | Game stopped; plugins/config/mod/test saves restored byte-for-byte, original local history restored, temporary launcher removed, slot unlocked |

The test harness's first Escape was sometimes posted before the death animation/confirmation became actionable;
the normal stock Continue button was then used. This did not require replacing, suppressing or replaying death
handling. Screenshots taken during loading or before confirmation are not counted as recap validation.

The existing test save still produces the four previously documented item assertions when used without its
Cartographer data. Those are fixture baseline errors, not a clean-game claim. Broader coexistence remains covered
only by the earlier nine-peer smoke check above.

### Evidence and visual differences

Screenshots and raw evidence are outside git in the `death-recap-native-ui` run under the machine's runs directory.
Final screenshots include `final-collapsed-1920.png`, `final-expanded-1920.png`, the `1280-720` / `1280-960`
variants, `final-long-fit.png`, `final-three-expanded.png`, `final-actual-collapsed.png` and
`final-actual-defenses.png`. The multi-element/healing screenshots are clearly labeled synthetic fixtures rendered
in the actual game. Real-death screenshots intentionally show unavailable fields where the capture lacks evidence.

Native stock assets differ from the mockup's bespoke art: the outer frame is gray stone rather than its exact
bronze ornament, the title plate/section edges are simpler, and the stock Close button has native inlays. The
life trace follows observed steps and gaps rather than reproducing the mockup's illustrative curve. Values display
rounded whole resource units; raw 1/256 values remain in reports/exports. Advanced sections use a viewport and
paged wrapped rows to contain arbitrarily long reports. The stock death confirmation precedes automatic opening.

### Remaining validation / capture limits

A physical controller was unavailable. XInput mappings and the optional native reopen route are implemented but
have not been exercised with controller hardware. D2R 3.3 assets were unavailable in the owned slots; compatibility
uses the shared engine image and independent byte guards, not a separate live 3.3 qualification. A live non-English
locale, custom UI profiles, TCP/IP host/join, complete mechanic coverage and the full capture matrix remain untested.

The existing capture still cannot prove all fatal links, elemental stages, projectile ability names, modifiers,
state categories or effective resistance calculations. Unavailable values are explicit. Defense percentages are
raw stat totals before difficulty penalties/caps, not a claim of effective resistances. These checks establish the
native presentation and narrow real-death/resurrection behavior, not full telemetry coverage or release readiness.
