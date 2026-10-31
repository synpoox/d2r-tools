# MultiSpec

Dual spec for Diablo II: Resurrected (offline, with D2RLoader; tested on D2R 3.3 and D2RLoader 1.3.1). Each character keeps 3 saved skill allocations
(up to 99 with `spec_count`) and switches between them in town, without a respec token.

**Status: unreleased. Tested in game on D2R 3.3 and D2RLoader 1.3.1.**

## Use

Open the skill tree: a panel beside it shows your active spec. In town, click its arrows or scroll the mouse wheel to go
through your specs. Outside town the arrows are greyed out.

Or use D2RLoader's console:

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
`<game slot>\d2rloader\plugins\` with the game closed (or use `install.ps1 -Dll <downloaded dll>`). MultiSpec is a global
plugin: it works with any mod, or none; don't put it in a mod's `d2rloader\plugins` folder.
Local builds and other commands: [CLAUDE.md](CLAUDE.md#commands). Built on the plugin template of d2rAI-toolkit (MIT).
