# Agent guide

Read this file, then the affected plugin's README and `CHANGELOG.md`, and search that plugin's `src/` and `tests/`
before changing anything. Build, test and review steps are in [CONTRIBUTING.md](CONTRIBUTING.md); releases are in
[docs/RELEASING.md](docs/RELEASING.md).

## Ownership and safety

- `plugins/<name>/` owns its code, config defaults, docs and changelog. Plugins are independent: no code shared or
  included across plugins, no plugin building another. Shared tooling lives in `tools/`, `docs/` and `.github/`.
- `third_party/PluginSDK` is a submodule pinned once for every plugin. Never vendor or edit it here. Moving the pin
  needs every plugin rebuilt first (`docs/RELEASING.md`).
- Native addresses, offsets, signatures and byte guards are specific to one D2R build. Keep them in the owning plugin,
  keep what is proven to work, and record every change with its build, evidence and guard in
  `docs/reverse-engineering/` (registry and workflow there; read it before searching for an address).
  Where a guard does not match, the plugin must fail open: log it and leave that part off.
- Never commit game binaries or extracted game assets, dumps, saves, personal config, logs, build output (`build/`,
  `dist/`) or credentials. Tests that need a game slot use a test save, never the owner's own install or characters.
- `d2rAI-toolkit` is a separate repository (templates, game-slot testing). Do not copy it in; reference it.
- Outward-facing steps (pushing, tagging, releases, issues, comments) happen only when the owner asks.

## Working

- Every plugin's `PluginInfo` (`plugin.cpp`) has `.author = "Synpoo"` and a readable `.name` ("Quick NPC", not the
  id `quick-npc`); `.description` says what the plugin does in one sentence.
- Change only the plugin you were asked about; keep fixes minimal and mechanical unless asked for more. Match the
  surrounding code's style, naming and comment density.
- Build and test with `tools\build.ps1 -Plugin <name> -Test`; build the others too after touching shared files.
- Keep build output and scratch work in `build/`, `dist/` or outside the repo; delete superseded scratch builds.
- Work on a branch and keep unrelated local changes intact.

## Finishing

- Say what was verified and how, and keep the levels apart: unit tests, a clean build, an install into a game, and
  live in-game testing. Do not call something tested in game unless it was.
- Add player-visible and compatibility changes to the plugin's `CHANGELOG.md` under `Unreleased`. Update the plugin
  README (and the table in the root `README.md`) when behavior, config or supported builds change.
- Releases: tag each plugin `<name>/vX.Y.Z` (dated changelog heading, version bump), then publish players a suite release
  from `release/suite.json` (`docs/RELEASING.md`); never publish one unless the owner asks.
- Commit messages and PR descriptions state the real scope; no claims the validation does not support.
