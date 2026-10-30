# Better Automap

A D2RLoader plugin that improves Diablo II: Resurrected's automap: choose which side the mini map sits on, open
the full-screen map over the mini map with one key, and zoom the full-screen map with the mouse wheel.

By Synpoo. For D2R 3.3.0 (build 93847, Battle.net; 94995 on Steam) with D2RLoader 1.3.1.

## Features

- **Mini map position**: the mini map (Options > Gameplay > Automap Size: Mini Map) sits at the top right, or the
  top left if you prefer. It still steps aside to the left while the inventory or stash is open, as in the base game.
- **Full-screen map over the mini map**: while the mini map is shown, the Automap key (Tab) opens the full-screen
  map in its place. Press it again and the mini map comes back exactly as it was. Your saved options are not changed.
- **Mouse wheel zoom**: on the full-screen map, wheel up zooms in and wheel down zooms out, within limits you set.
  The wheel works as usual whenever another panel uses it (inventory, stash, chat, menus) or the full-screen map is
  closed.
- Every feature can be turned off on its own. Turned off, it behaves exactly like the base game.
- Safe on updates: on any other game build the plugin does nothing and says so in the log.

## Requirements

- Diablo II: Resurrected 3.3.0, build 93847 (Battle.net) or 94995 (Steam).
- D2RLoader 1.3.1, starting the game through `D2RLoader.exe`.
- Windows 10 or 11, 64-bit.

## Installation

1. Download `better-automap.dll` from the latest build (the `better-automap` artifact of the monorepo's
   [ci workflow](https://github.com/synpoox/d2r-tools/actions/workflows/ci.yml)).
2. Close the game.
3. Copy `better-automap.dll` into `<D2R folder>\d2rloader\plugins\` (create the folder if it is missing). It then
   works with every mod.
4. Start the game through D2RLoader. The first start creates `<D2R folder>\d2rloader\config\better-automap.toml`.
5. In game, open the console and type `better-automap` to see that all three features are `on`.

To remove it, delete the DLL (and the `.toml` if you like).

## Configuration

Edit `<D2R folder>\d2rloader\config\better-automap.toml`, or put a `better-automap.toml` next to the DLL (that copy
wins). Restart the game after a change.

| Key | Default | What it does |
|---|---|---|
| `[minimap] enabled` | `true` | Mini map position on/off |
| `[minimap] position` | `"right"` | `"right"` (top right) or `"left"` (top left) |
| `[overlay] enabled` | `true` | Tab opens the full-screen map over the mini map and brings the mini map back |
| `[zoom] enabled` | `true` | Mouse wheel zoom on the full-screen map |
| `[zoom] min` | `0.5` | Smallest zoom (above 0, at most 1.0) |
| `[zoom] max` | `2.5` | Largest zoom (1.0 to 8.0) |
| `[zoom] step` | `0.25` | Zoom change per wheel notch |

A wrong value keeps its default and is reported in the log and by the `better-automap` command. For crash hunting
you can also list features to leave out (`minimap`, `overlay`, `zoom`) in `better-automap-off.txt` next to the DLL.

### Known limits

- The mini map position replaces the left/right choice of the Automap Size option and the Toggle Minimap key.
- The Tab change only applies while the mini map is shown; with the Full Screen size, Tab works as in the base game.
- The zoom resets to 1.0 when the game restarts, and does not apply to the mini map.
- Controller mode is not supported (the wheel does not apply there).
- Works only on the build listed above; any other build leaves the plugin inactive until it is updated.

For developers: `docs/hook-sites.md` lists every game address with its evidence, `docs/TESTING.md` the in-game test
checklist. Builds run on GitHub Actions (the monorepo's `.github/workflows/ci.yml`, CMake, `windows-latest`, x64 Release).
