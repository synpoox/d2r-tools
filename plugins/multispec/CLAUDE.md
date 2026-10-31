# MultiSpec: instructions for Claude

MultiSpec is a global D2RLoader plugin (never mod-scoped) for D2R 3.3 (offline): WoW-style dual spec. A character keeps N saved skill
allocations ("specs", 3 by default) and loads one with the `spec N` console command, in town only.
Design: [docs/design.md](docs/design.md). Recon: [docs/reverse-engineering/multispec-recon.md](../../docs/reverse-engineering/multispec-recon.md).

## The toolkit

The toolkit lives at `../d2rAI-toolkit` (override with `$D2RAI_TOOLKIT`). **Its [CLAUDE.md](../d2rAI-toolkit/CLAUDE.md)
rules apply here**: no game files in git, data before code, ask before anything public,
paid or destructive, never touch the owner's own install (test in game slots). Read its `knowledge/` before engine
work; don't copy it here, link to it.

## Commands

| What | Command |
|---|---|
| Unit tests (any OS, no game) | `tests/run_tests.sh` |
| Windows build check on Linux | `tools/mingw_check.sh` |
| Build the DLL (normal way) | monorepo CI (`.github/workflows/ci.yml` at the repo root, `tools\build.ps1 -Plugin multispec -Test`) on every push: MSVC x64 Release + unit tests; download the `multispec` artifact (DLL, sha256, default config) |
| Build locally (Windows, MSVC; optional) | `powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin multispec -Test` from the repo root (SDK: shared `third_party/PluginSDK`) |
| Install into a game slot (game closed) | `powershell -ExecutionPolicy Bypass -File install.ps1 -Game <slot dir>` (global: `<slot>\d2rloader\plugins`) |
| In-game checks (Phase 3) | toolkit `testing/` game slots: see [TESTING.md](TESTING.md) |
| Entry bytes vs a new dump | `python tools/check_entries.py <dump.exe>` |

## Architecture

| File | Part (off-file word) | What |
|---|---|---|
| `src/plugin.cpp` | | Entry point, parts table, `spec` and `multispec` commands, console -> game thread -> UI thread |
| `src/spec_model.*` | | Portable: data model, file format + CRC, reconciliation (tested in `tests/`) |
| `src/swap.*` | | Portable: swap algorithm over an abstract engine (tested with a fake) |
| `src/spec_engine.*` | `specswap` | The real engine: D2R's respec and spend-a-point functions, player lookup, town/alive checks |
| `src/spec_store.*` | `specstore` | `<name>.multispec` next to the save; atomic write, `.bak`, quarantine of bad files |
| `src/d2r.*` | `specswap` | RVAs, types, entry-byte checks (build 93847) |
| `src/env_guard.*`, `off_switch.*`, `log.*` | `envguard` | From the toolkit's `plugin-template` |
| (command) | `speccmd` | The `spec` console command |
| `src/spec_panel_layout.*` | | Portable: the spec panel's JSON layout, built from stock sprite paths; geometry; arrow parsing and stepping (tested in `tests/`) |
| `src/spec_panel.*` | `specui` | The panel docked to the skill tree: opens and closes with it; arrows and the wheel run `spec N`; shows the active spec |

Off-switch: `multispec-off.txt` next to the DLL, words separated by spaces: `envguard specstore specswap speccmd specui`.
Status: the `multispec` console command (version, image check, every part, counters, last error).

## Rules for this repo

- **Any new engine address goes in `docs/reverse-engineering/multispec.md` (repo root) with the build number and its evidence** (signature, entry bytes, how
  it was verified: static or in game), then into `src/d2r.h` / `src/d2r.cpp`. Run `tools/check_entries.py` on the dump.
- Tested on **D2R 3.3, image stamp `0x6ab3782c`, D2RLoader 1.3.1**. Other versions are attempted, not tested: `CheckEntries` still refuses a function whose entry bytes differ, and `crash_guard.h` logs any fault.
- MultiSpec never writes `.d2s` / `.d2rl` files, never hooks a function, and never writes a skill level or `newskills`
  directly: only D2R's own respec and spend functions change skills.
- Keep the portable core free of Windows and game calls, and add a unit test with every change to it.
- **Privacy scan**: `tools/install_hooks.sh` once per clone; the hooks run the toolkit's `tools/check_public.py` on
  every commit and commit message (private terms in `~/.d2r-modkit/private_terms.txt`, outside the repo). Don't bypass
  it.
