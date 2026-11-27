# Death Recap

An unreleased D2RLoader plugin that records measured health and mana changes before your character dies and presents a native recap with local history. This is an alpha implementation with **partial coverage**, not a release or a claim of complete combat telemetry. Native UI refinement and narrow real-death/resurrection checks have passed on 3.4; the broader combat coverage matrix remains unverified.

## Features

- Guarded local HP/mana commit and healing observations, separate regeneration endpoints, and one report per observed local death.
- Native recap with a killing-blow summary, sorted elemental bars, a life step graph with healing markers, and three initially collapsed detail sections. Missing attribution and amounts remain explicitly unavailable.
- Reopen the latest report, scroll and page its advanced telemetry, and retain bounded local history. Storage runs on a worker outside combat callbacks.

## Requirements

Target: D2R 3.3 (93847) and 3.4 (95032) under D2RLoader 1.3.1, engine image `0x6ab3782c`; offline and supported modded TCP/IP only. A smoke check passed on 3.4 at 1920x1080 with nine peer plugins; broader compatibility remains unverified. Each capture part uses independent signatures, byte guards and SDK mutation ownership; a mismatch leaves that part off. There is no version gate. No Battle.net support.

Joined clients have no authoritative game thread. They can produce clearly labeled **replicated snapshot** reports from a local living-to-dead transition; they cannot see individual authoritative hits. If native death capture is unavailable, the host can also fall back to these snapshots. Neither path claims full coverage.

## Installation

Development build only: copy the built `death-recap.dll` into an isolated install's `d2rloader/plugins/` directory. The plugin supplies its own panel resources and works globally without mod layouts. Do not distribute this alpha as a validated release.

Build from the repository root against its pinned SDK:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin death-recap -Test
```

## Hotkeys

| Input | Action |
|---|---|
| F8 | Dismiss or reopen latest recap |
| Mouse | Click a section to expand/collapse; click Close to dismiss |
| Mouse wheel / Home / End | Scroll the recap |
| W / S | Select previous / next section |
| Enter | Expand / collapse selected section |
| Page Up / Page Down | Previous / next detail page |
| Escape | Dismiss |
| Controller D-pad up / down | Select previous / next section |
| Controller A | Expand / collapse selected section |
| Controller D-pad left / right | Scroll up / down |
| Controller LB / RB | Previous / next detail page |
| Controller B | Dismiss |

Keyboard actions are rebindable in the loader. Controller reopen uses an optional native UI-switcher route after inventory. The SDK allows one global route; another plugin's route is preserved if it conflicts. Automatic opening waits until the normal death confirmation and resurrection finish; it does not dismiss or replace the stock death prompt. Reopening resets all sections. Keyboard/mouse and resurrection have been checked live; a physical controller has not.

## Configuration

The embedded defaults are consumed at load; restart after edits.

| Setting under `[death-recap]` | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Enable observations and presentation |
| `window_seconds` | `10` | Rolling report window, clamped to 1â€“60 seconds |
| `auto_open` | `true` | Open the recap after the recorded death is confirmed and the character resurrects |
| `history_limit` | `20` | Maximum retained reports, clamped to 1â€“100 |

Malformed settings retain their defaults. Reports and `latest-export.json` live under `%LOCALAPPDATA%/D2RLoader/death-recap/`, outside character saves. History uses a versioned numeric schema, atomic replacement and a 32 MiB disk limit; the oldest reports are omitted from disk if necessary. Missing/corrupt/unsupported history starts empty in memory and never changes a character. History format 2 preserves level and localized area context and reads earlier format-1 reports. An export contains raw resource units, field evidence, events, names and defenses; unavailable amounts use JSON null. Defense stats retain native stat units. Timestamps are monotonic process/boot-relative milliseconds, not wall-clock dates.

## Coverage and validation

Always interpret totals as measured subtotals. Regeneration commits can combine healing and persistent damage, so only their endpoints are reported. Other resource writers, poison/Open Wounds attribution, Crushing Blow, Energy Shield, costs, projectile abilities, pet ownership and attacker modifiers are not fully captured. Unknown sources/abilities and missing element stages stay explicit. A last hit alone never establishes a confirmed killing blow. Reconciled accounting does not establish complete capture.

Source names are copied from active compiled tables and SDK localization; unavailable numeric localization uses the active `monstats:<Id>` key. Persisted names are owned by the report and do not change with the active mod. State capture is bounded and shows truncation. A lost feed event leaves a sequence gap; a saturated death marker is retried. Nested mutations are not added twice.

Windows x64 DLL build and four host test executables pass with the installed VS2019/MSVC 19.29 toolchain. The dump checker passes all 20 capture/UI/context witnesses. VS2022 is unavailable on this machine. Native UI checks include synthetic multi-element/healing reports and actual deaths, captured level/area context, repeated reports, dismissal, reopening, expansion, paging and resurrection. Validation details and remaining platform/input gaps are in [TESTING.md](TESTING.md).

See [native implementation evidence](../../docs/reverse-engineering/death-recap-implementation.md), [static contracts](../../docs/reverse-engineering/death-recap-static.md), and [handoff](HANDOFF.md) for remaining acceptance work.

MIT licensed.
