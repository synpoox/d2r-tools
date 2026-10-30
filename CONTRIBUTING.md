# Contributing

Read [AGENTS.md](AGENTS.md) (ownership and safety rules) and the affected plugin's README first. Work on a branch;
`main` is what CI builds and releases come from.

## Build and validate

```powershell
git clone --recurse-submodules https://github.com/synpoox/d2r-tools    # or: git submodule update --init
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin <name> -Test
```

Windows x64, Visual Studio 2022 (C++ workload), CMake 3.29+. `-Test` runs a plugin's host unit tests (ctest or its test
executable); `plugins/<name>/tests/*.py` are d2rAI-toolkit in-game check modules and are only syntax-checked. CI
(`.github/workflows/ci.yml`) builds every plugin on `windows-latest`.

Report results in separate levels: automated tests, a clean build, installed into a game, and live in-game testing
(for example a plugin's `TESTING.md`). For documentation-only changes, check links and paths instead.

## Native findings

New or changed native addresses, offsets, signatures, byte guards or ABI details go in the owning plugin's source and
docs with: the game build or module they apply to, the evidence, the guard, what happens when the guard fails (fail
open), and what has not been validated.

## Changelog and docs

Add player-visible or compatibility changes to the plugin's `CHANGELOG.md` under `## Unreleased`. At release, move them
under `## X.Y.Z - YYYY-MM-DD` (release date), as in [docs/RELEASING.md](docs/RELEASING.md). Update the root `README.md`
plugin table (version, description) when either changes.

## Review

Keep the commit or PR description to the real scope and honest about validation. Do not commit game files, saves,
logs, personal settings, build output or credentials.
