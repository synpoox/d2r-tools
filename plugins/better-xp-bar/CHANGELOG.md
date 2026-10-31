# Changelog

## 1.0.1 - 2026-10-31

- Attempts to run on any game or D2RLoader version.
- Crashes are written to the plugin's log.

## 1.0.0 - 2026-10-30

First public release, for Diablo II: Resurrected 3.3 under D2RLoader 1.3.1.

- Lines are drawn as D2R's own text, with a black box behind them.
- The XP line shows progress through the level.
- `XP/h` and `Next level` no longer flicker.
- The session clock is off by default; add `"time"` to `lines` to show it.
- Hovering the stock XP bar shows a hidden display, and clicking it toggles the display.
- New settings: `display.text_align`, `display.scale` and `display.xp_format`.
- New default position: update `offset_x` and `offset_y`, or delete your config, to use it.
