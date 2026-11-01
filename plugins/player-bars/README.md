# Player Bars

A [D2RLoader](https://github.com/D2RLoader) plugin for Diablo II: Resurrected that shows health and mana bars above
your character, MMO style, at all times in a game. The orbs and the statues beside them are hidden, the frame behind the buttons stays, and you can click to walk where the art was.

## Requirements

- [D2RLoader](https://github.com/D2RLoader)
- Diablo II: Resurrected (Battle.net or Steam) with D2RLoader. Tested on D2R 3.3 with D2RLoader 1.3.1; other versions are attempted.

Works with or without mods, with mouse and keyboard or a controller. A mod that moves the HUD art keeps it. New art in
the stock position is clipped like the stock art; set `hide_orbs = false` to keep it whole.

## Install

1. Close the game.
2. Copy `player-bars.dll` into `<game folder>\d2rloader\plugins\`. Create the `plugins` folder if it doesn't exist.
3. Start the game through D2RLoader.

To uninstall, delete the DLL.

## Settings

Edit `<game folder>\d2rloader\config\player-bars.toml`, then restart the game. D2RLoader creates it on first run.

| Setting | Default | What it does |
|---|---|---|
| `hide_orbs` | `true` | Hide the orbs and statues, and let the ground there take clicks |
| `position` | `"top"` | `"top"` above the character, `"bottom"` below it |
| `show_text` | `false` | Show numbers on the bars |
| `bar_width`, `health_height`, `mana_height` | `90`, `9`, `7` | Bar size in pixels at 1080p |
| `offset_y` | `150` | With `"top"`: height above the middle of the screen |
| `offset_below` | `15` | With `"bottom"`: distance below the middle of the screen |
| `health_color`, `mana_color`, `back_color`, `border_color` | | Colors as `RRGGBB` |

## License

[MIT](LICENSE) © Synpoo.

Diablo® II: Resurrected is a trademark of Blizzard Entertainment, Inc. This project is not affiliated with or
endorsed by Blizzard Entertainment.
