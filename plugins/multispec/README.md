# MultiSpec

A D2RLoader plugin for Diablo II: Resurrected that gives each character several saved skill allocations, called specs.
Switch between them in town from a panel beside the skill tree, without a respec token.

## Features

- **Spec panel**: opens with the skill tree and shows your active spec. In town, click its arrows or scroll the mouse
  wheel to go through your specs. Outside town the arrows are greyed out.
- **3 specs per character** by default, up to 99 with `spec_count`. A new spec starts empty, with all your points unspent.
- Spending points, Akara's respec and the Token of Absolution change the active spec, as usual.
- Points you earn from levels and quests count for every spec.
- Save files are never edited. Your active spec is in your character's save, and the others are in `<character>.multispec`
  next to it. If you remove the plugin, you keep the active spec and the others wait in that file.
- A global plugin: works with every mod and with the unmodded game.
- Console command `spec` lists your specs, `spec 2` loads spec 2 in town, and `multispec` shows the plugin's status.

## Requirements

- Diablo II: Resurrected 3.3, offline or TCP/IP. Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

On any other game or D2RLoader version the plugin still attempts to load and run. It may crash there; game functions
that don't match are left alone, and any crash is written to the plugin's log with its cause.

## Installation

1. Close the game.
2. Copy `multispec.dll` to `<game>\d2rloader\plugins\`.
3. If an older `multispec.dll` is in a mod's `mods\<Mod>\d2rloader\plugins\` folder, delete it.
4. Start the game through D2RLoader.

The first start writes the settings file `<game>\d2rloader\config\multispec.toml`.

## Configuration

Edit `<game>\d2rloader\config\multispec.toml`, then restart the game. D2RLoader never overwrites your copy.

```toml
[multispec]
spec_count = 3    # specs per character, 1 to 99
```

Lowering `spec_count` keeps the extra specs in each character's file, and raising it brings them back. If your active
spec is above the new count, the panel still shows it, and its arrows step back into range.

In TCP/IP games, the host and clients must use the same `spec_count`.
