# Better XP Bar

A D2RLoader plugin for Diablo II: Resurrected that shows your XP progress on the HUD.

```
Level 84
XP 1.35M/1.50M  73.31%
Session +38,250,112  +27.05%
XP/h 21.4M  15.12%  (avg 18.9M)
Next level 23m
```

## Features

- **XP at a glance**: level, progress through the level, session gain, XP per hour and time to the next level.
- **Session tracking**: a new session starts with every game, or keep one per character.
- **Easy to hide**: hovering the stock XP bar shows a hidden display, and clicking it toggles the display.

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

## Installation

1. Close the game.
2. Copy `better-xp-bar.dll` to `<game>\d2rloader\plugins\`.
3. If an older `better-xp-bar.dll` is in a mod's `mods\<Mod>\d2rloader\plugins\` folder, delete it.
4. Start the game through D2RLoader.

The first start writes the settings file `<game>\d2rloader\config\better-xp-bar.toml`.

## Hotkeys

| Action | Default |
|---|---|
| Toggle the display | Ctrl+X |
| Reset the session | Ctrl+R |

Rebind them in Options > Controls > Better XP Bar.

## Configuration

Edit `<game>\d2rloader\config\better-xp-bar.toml`, then restart the game. D2RLoader never overwrites your copy.

```toml
[display]
visible = true            # shown when a game starts
anchor = "bottom-center"  # top-left, top-center, top-right, bottom-left, bottom-center, bottom-right
text_align = "left"       # left, center or right
offset_x = -764           # distance from the anchor, + = right
offset_y = -345           # distance from the anchor, + = down
text_size = 0             # 0 = the default size
scale = 1.0               # size of the whole box (0.25 to 4)
background = 0.7          # opacity of the box behind the lines (0 = none)
text_color = [1.0, 0.86, 0.55]
lines = ["level", "xp", "session", "rate", "eta"]  # which lines, in order; "time" adds the session clock
xp_format = "XP {current}/{needed}  {percent}"     # also {remaining}, {total}, and {current_full} for full numbers

[session]
scope = "game"            # "game": a new session every game. "character": one session per character
window_seconds = 600      # how far back the live XP per hour looks (60 to 86400)
eta_from = "window"       # "window": time to level from the live rate. "session": from the session average
```

XP lost on death lowers XP per hour. Rates show `--` for the first 60 seconds, and `Next level` shows `--` while the rate
is zero or negative.

Diablo II: Resurrected is (c) Blizzard Entertainment. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
