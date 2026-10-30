# MultiSpec

Dual spec for Diablo II: Resurrected 3.3 (offline, D2RLoader 1.3.1). Each character keeps 2 saved skill allocations
(up to 8 with `spec_count`) and switches between them in town, without a respec token.

**Status: Phase 2 (MVP), not yet tested in game.**

## Use

Open D2RLoader's console in game and type:

| Command | Does |
|---|---|
| `spec` | Lists your specs: points used, unspent, which is active |
| `spec 2` | Loads spec 2 (in town, alive). A new spec starts empty: all your points unspent |
| `multispec` | Plugin status (for bug reports) |

Spending points, Akara's respec and the Token of Absolution change the active spec, as usual. Points you earn (levels,
quests) count for every spec.

## How it stores specs

Your active spec is in your character's save as always; MultiSpec never edits save files. The other specs are in
`<character>.multispec` next to the save. Removing the plugin keeps the active spec; the others wait in that file.

## Build and install

Every push is built by the monorepo's GitHub Actions `ci` workflow: open the run in the Actions tab and download the `multispec`
artifact (`multispec.dll`, its sha256, the default `multispec.toml`). Copy the DLL to
`<game slot>\mods\<Mod>\d2rloader\plugins\` with the game closed (or use `install.ps1 -Dll <downloaded dll>`).
Local builds and other commands: [CLAUDE.md](CLAUDE.md#commands). Built on the plugin template of d2rAI-toolkit (MIT).
