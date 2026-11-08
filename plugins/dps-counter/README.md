# DPS Counter

A D2RLoader plugin for Diablo II: Resurrected that shows the damage per second you deal to monsters, as text just
above the life globe. MIT licensed.

## Features

- **Real damage**: counts the damage your hits deal to monsters, after their resistances (a killing blow counts in full).
- **Every attack**: melee, missiles, spells and auras that hit.
- **Out of the way**: it shows only while you fight, and disappears a few seconds after the last hit.

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

Works with or without mods. Your minions' and your hireling's damage is not counted.

Limits: it counts only in games you host (single player, or a TCP/IP game you created); in a game you join it reads 0.
In a hosted game with other players it counts the whole party's damage. Poison and other damage over time are not
counted.

## Installation

1. Close the game.
2. Copy `dps-counter.dll` to `<game>\d2rloader\plugins\`.
3. Start the game through D2RLoader.

The first start writes the settings file `<game>\d2rloader\config\dps-counter.toml`.

## Configuration

Edit `<game>\d2rloader\config\dps-counter.toml`, then restart the game. D2RLoader never overwrites your copy. A missing
setting uses its default.

```toml
[dps-counter]
window_seconds = 5   # damage over this many seconds, divided by them (1 to 60)
enabled = true       # false turns the counter off: no label, no hook
offset_x = -495      # the label's centre from the screen's centre, in pixels at 1080p
offset_y = 830       # the label's top from the top of the screen (the default sits just above the life globe)
text_size = 22
```

## License

[MIT](LICENSE) © Synpoo.

Diablo® II: Resurrected is a trademark of Blizzard Entertainment, Inc. This project is not affiliated with or
endorsed by Blizzard Entertainment.
