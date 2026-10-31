# Quick NPC

A D2RLoader plugin for Diablo II: Resurrected that skips the NPC dialog menu. Hold **Left Ctrl** and left-click an
NPC: you go to them, and the option you came for is chosen for you: trade, identify, resurrect your mercenary, or
travel. The game's own menu option is pressed, so prices, range, quest and server checks work exactly as if you had
clicked it yourself. MIT licensed.

## Features

- **Vendors**: opens the trade screen.
- **Deckard Cain**: identifies your items at his usual price.
- **Mercenary and travel NPCs**: resurrects your dead mercenary, or travels to the next act.

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

## Installation

1. Close the game.
2. Copy `quick-npc.dll` to `<game>\d2rloader\plugins\`.
3. If an older `quick-npc.dll` is in a mod's `mods\<Mod>\d2rloader\plugins\` folder, delete it.
4. Start the game through D2RLoader.

The first start writes the settings file `<game>\d2rloader\config\quick-npc.toml`.

## Configuration

Edit `<game>\d2rloader\config\quick-npc.toml`, then restart the game.
D2RLoader never overwrites your copy. A missing setting uses its default, and a bad value is reported in the
plugin's log and its default kept.

```toml
[quick-npc]
enabled = true          # false: switched off
hotkey = "LeftCtrl"     # hold this, then left-click an NPC
timeout_seconds = 60    # how long a click waits for the NPC's dialog (5 to 600)

trade = true            # vendors: open the trade screen
cain_identify = true    # Deckard Cain: identify your items
mercenary = true        # mercenary NPCs: resurrect a dead mercenary
travel = true           # Warriv, Meshif: travel to the next act
```

`hotkey` accepts:
- **Modifier keys:** `LeftCtrl`, `RightCtrl`, `Ctrl`, `LeftAlt`, `RightAlt`, `Alt`.
- **Keyboard keys:** `A`–`Z`, `0`–`9`, `F1`–`F24`, `Numpad0`–`Numpad9`, `Space`, `Tab`, `CapsLock`, `Backquote`.
- **Mouse buttons:** `Mouse3` (middle), `Mouse4` and `Mouse5` (side).
- **A virtual-key code**, such as `"0xA2"`.

Shift doesn't work: it is D2R's Stand Still key, which stops NPC interaction. Alt is D2R's Show Items key, so a click
on an item label picks the item up instead.
