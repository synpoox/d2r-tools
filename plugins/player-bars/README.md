# Player Bars

A D2RLoader plugin for Diablo II: Resurrected that shows health and mana bars above your character, MMO style, the
whole time you're in a game. The orbs and statues are hidden and the frame behind the buttons stays. MIT licensed.

## Features

- **Bars above your character**: they follow you when side panels open, or sit below you if you prefer.
- **Smooth fill**: a lighter section shows what a healing or mana potion is still restoring.
- **Cleaner HUD**: no orbs or statues, and you can click to walk where they were. Works on the controller HUD too.

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

Works with or without mods. A mod that moves the HUD art keeps it. New art in the stock position is clipped like the
stock art; set `hide_orbs = false` to keep it whole.

## Installation

1. Close the game.
2. Copy `player-bars.dll` to `<game>\d2rloader\plugins\`.
3. Start the game through D2RLoader.

The first start writes the settings file `<game>\d2rloader\config\player-bars.toml`.

## Configuration

Edit `<game>\d2rloader\config\player-bars.toml`, then restart the game. D2RLoader never overwrites your copy. A missing
setting uses its default.

```toml
[player-bars]
hide_orbs = true          # false: keep the orbs and statues
show_text = false         # true: numbers on the bars
bar_width = 90            # bar size in pixels at 1080p
health_height = 9
mana_height = 7
position = "top"          # "top" above the character, "bottom" below it
offset_y = 150            # with "top": height above the middle of the screen
offset_below = 15         # with "bottom": distance below the middle of the screen
health_color = "962325"   # colors are RRGGBB
mana_color = "2020A8"
back_color = "0F0A0A"
border_color = "000000"
```

## License

[MIT](LICENSE) © Synpoo.

Diablo® II: Resurrected is a trademark of Blizzard Entertainment, Inc. This project is not affiliated with or
endorsed by Blizzard Entertainment.
