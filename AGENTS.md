# Agent guide

Read this file, then the affected plugin's README and `CHANGELOG.md`, and search that plugin's `src/` and `tests/`
before changing anything. Build, test and review steps are in [CONTRIBUTING.md](CONTRIBUTING.md); releases are in
[docs/RELEASING.md](docs/RELEASING.md). Issue tracker use and triage labels are in [docs/agents/](docs/agents/).

## Ownership and safety

- `plugins/<name>/` owns its code, config defaults, docs and changelog. Plugins are independent: no code shared or
  included across plugins, no plugin building another. Shared tooling lives in `tools/`, `docs/` and `.github/`.
- `third_party/PluginSDK` is a submodule pinned once for every plugin. Never vendor or edit it here. Moving the pin
  needs every plugin rebuilt first (`docs/RELEASING.md`).
- Native addresses, offsets, signatures and byte guards are specific to one D2R build. Keep them in the owning plugin,
  keep what is proven to work, and record every change with its build, evidence and guard in
  `docs/reverse-engineering/` (registry and workflow there; read it before searching for an address).
  Where a per-part byte guard does not match, log it and leave that part off.
- No version gates: a plugin always attempts to load, hook and run, whatever the game build, D2RLoader version or PE stamp
  (the stamp is the loader's). Declare `D2RL_PLUGIN_ABI_VERSION`, read optional context through the SDK getters, and keep
  a `crash_guard.h` installed so any fault is logged to the plugin's log with its cause. New engine code should find addresses by byte
  signature, as `plugins/scrollable-waypoints` does.
- Never commit game binaries or extracted game assets, dumps, saves, personal config, logs, build output (`build/`,
  `dist/`) or credentials. Tests that need a game slot use a test save, never the owner's own install or characters.
- `d2rAI-toolkit` is a separate repository (templates, game-slot testing). Do not copy it in; reference it.
- Outward-facing steps (pushing, tagging, releases, issues, comments) happen only when the owner asks.

## Working

- Read the relevant parts of `d2rAI-toolkit` (`knowledge/`, `plugin-template/`, `third_party/PluginSDK`) before beginning
  any plugin work. Build against this repo's own `third_party/PluginSDK`; the toolkit's copy can be newer.
- New plugins are global by default: `PluginInfo.flags` never includes `ModScopedOnly`, and a plugin does not depend on
  the active mod shipping layouts or other files (UI goes in plugin panels and plugin resources). The one allowed read
  is what the game itself takes from the active mod, such as `savepath` in its `modinfo.json` to find the saves
  (MultiSpec); it must be optional, with a fallback when the mod or the key is missing.
- Every plugin's `PluginInfo` (`plugin.cpp`) has `.author = "Synpoo"` and a readable `.name` ("Quick NPC", not the
  id `quick-npc`); `.description` says what the plugin does in one sentence.
- Change only the plugin you were asked about; keep fixes minimal and mechanical unless asked for more. Match the
  surrounding code's style, naming and comment density.
- Build and test with `tools\build.ps1 -Plugin <name> -Test`; build the others too after touching shared files.
- Keep build output and scratch work in `build/`, `dist/` or outside the repo; delete superseded scratch builds.
- Work on a branch and keep unrelated local changes intact.
- Prefer a git worktree per feature branch (`git worktree add ..\d2r-tools-<topic> -b <plugin>/<topic> origin/main`, then
  `git submodule update --init --recursive`); remove it after the squash-merge. Do not copy `build/` between worktrees or
  install the same plugin DLL to the game from two at once. Details in [CONTRIBUTING.md](CONTRIBUTING.md#worktrees).

## Finishing

- Say what was verified and how, and keep the levels apart: unit tests, a clean build, an install into a game, and
  live in-game testing. Do not call something tested in game unless it was.
- Add player-visible and compatibility changes to the plugin's `CHANGELOG.md` under `Unreleased`. Keep each entry to one short line, without jargon. Update the plugin
  README (and the table in the root `README.md`) when behavior, config or tested versions change. The root `README.md`
  lists released plugins only; never add an unreleased one.
- Releases: tag each plugin `<name>/vX.Y.Z` (dated changelog heading, version bump), then publish players a suite release
  from `release/suite.json` (`docs/RELEASING.md`); never publish one unless the owner asks.
- Commit messages and PR descriptions state the real scope; no claims the validation does not support.
