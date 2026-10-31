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

## Worktrees

Use a [git worktree](https://git-scm.com/docs/git-worktree) per active feature branch so parallel work does not share a
checkout, `build/` or `dist/`. Stay in the main checkout for `main`, releases (`tools\release.ps1`) and quick edits, and
keep the number of long-lived worktrees low.

```powershell
git fetch origin
git worktree add ..\d2r-tools-<topic> -b <plugin>/<topic> origin/main
cd ..\d2r-tools-<topic>; git submodule update --init --recursive

# after the squash-merge
git worktree remove ..\d2r-tools-<topic>; git branch -D <plugin>/<topic>; git worktree prune
```

- A new worktree starts with `third_party/PluginSDK` empty, so initialize the submodule (`tools\build.ps1` stops if it
  is missing). Each worktree configures its own `build/`; never copy one between worktrees.
- Worktrees share the game install: two that install the same plugin DLL overwrite each other, so test in game one at a
  time. Keep worktree paths short (MSBuild/CMake can hit the 260-character limit).
- Name sibling folders `d2r-tools-<topic>`, check `git worktree list` now and then, and prune stale ones.

## Native findings

New or changed native addresses, offsets, signatures, byte guards or ABI details go in the owning plugin's source and
in [docs/reverse-engineering/](docs/reverse-engineering/README.md): one feature document per plugin, plus a row in the
registry's index. Record the game build or module they apply to, the evidence, the guard, what happens when the guard
fails (fail open), and what has not been validated, as its recording format and
[workflow](docs/reverse-engineering/RVA-AND-INTERCEPTION-WORKFLOW.md) describe.

## Changelog and docs

Add player-visible or compatibility changes to the plugin's `CHANGELOG.md` under `## Unreleased`. Keep each entry to one short line in plain player wording. At release, move them
under `## X.Y.Z - YYYY-MM-DD` (release date), as in [docs/RELEASING.md](docs/RELEASING.md). The release notes quote that
section, so write it for players. Update the root `README.md` plugin table (version, description) when either changes; it lists released plugins only.

## Review

Keep the commit or PR description to the real scope and honest about validation. Do not commit game files, saves,
logs, personal settings, build output or credentials.
