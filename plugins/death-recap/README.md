# Death Recap

An unreleased D2RLoader plugin that records measured health and mana changes before your character dies and presents a native recap with local history. This is an alpha implementation with **partial coverage**, not a release or a claim of complete combat telemetry. It has not been installed or tested in game.

## Features

- Guarded local HP/mana commit and healing observations, separate regeneration endpoints, and one report per observed local death.
- Native recap panel with timeline pages, HP sample graph, source and element subtotals, and a defense/state snapshot. Missing attribution, amounts and killing-blow evidence remain unavailable.
- Reopenable history, bounded local retention, clear-history and JSON export. Storage runs on a worker outside combat callbacks.

## Requirements

Target: D2R 3.3 (93847) and 3.4 (95032) under D2RLoader 1.3.1, engine image `0x6ab3782c`; offline and supported modded TCP/IP only. Runtime installation and compatibility are unverified. Each capture part uses independent signatures, byte guards and SDK mutation ownership; a mismatch leaves that part off. There is no version gate. No Battle.net support.

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
| [ / ] | Previous / next report while open |
| Page Up / Page Down | Previous / next timeline page |
| Escape | Dismiss |
| Controller D-pad left / right | Previous / next report while open |
| Controller D-pad up / down | Previous / next timeline page |
| Controller B | Dismiss; stock gameplay controls remain untouched |

Keyboard actions are rebindable in the loader. Controller reopen uses an optional native UI-switcher route after inventory, paired with inventory. The SDK allows one global route; another plugin's route is preserved if it conflicts. Auto-open works independently of that route. Controller behavior and revive compatibility have not been checked live.

## Configuration

The embedded defaults are consumed at load; restart after edits.

| Setting under `[death-recap]` | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Enable observations and presentation |
| `window_seconds` | `10` | Rolling report window, clamped to 1–60 seconds |
| `auto_open` | `true` | Open the recap after a recorded death |
| `history_limit` | `20` | Maximum retained reports, clamped to 1–100 |

Malformed settings retain their defaults. Reports and `latest-export.json` live under `%LOCALAPPDATA%/D2RLoader/death-recap/`, outside character saves. History uses a versioned numeric schema, atomic replacement and a 32 MiB disk limit; the oldest reports are omitted from disk if necessary. Missing/corrupt/unsupported history starts empty in memory and never changes a character. An export contains raw resource units, field evidence, events, names and defenses; unavailable amounts use JSON null. Defense stats retain native stat units. Timestamps are monotonic process/boot-relative milliseconds, not wall-clock dates.

## Coverage and validation

Always interpret totals as measured subtotals. Regeneration commits can combine healing and persistent damage, so only their endpoints are reported. Other resource writers, poison/Open Wounds attribution, Crushing Blow, Energy Shield, costs, projectile abilities, pet ownership and attacker modifiers are not fully captured. Unknown sources/abilities and missing element stages stay explicit. A last hit alone never establishes a confirmed killing blow. Reconciled accounting does not establish complete capture.

Source names are copied from active compiled tables and SDK localization; unavailable numeric localization uses the active `monstats:<Id>` key. Persisted names are owned by the report and do not change with the active mod. State capture is bounded and shows truncation. A lost feed event leaves a sequence gap; a saturated death marker is retried. Nested mutations are not added twice.

Windows x64 DLL build and all three host test executables pass with the installed VS2019/MSVC 19.29 toolchain. The static dump checker passes all 17 patterns/witnesses. The requested VS2022 toolchain is unavailable on this machine. No installation or game checks were performed, as requested; mechanic coverage, host/join behavior, panel/controller rendering and coexistence remain unverified.

See [native implementation evidence](../../docs/reverse-engineering/death-recap-implementation.md), [static contracts](../../docs/reverse-engineering/death-recap-static.md), and [handoff](HANDOFF.md) for remaining acceptance work.

MIT licensed.
