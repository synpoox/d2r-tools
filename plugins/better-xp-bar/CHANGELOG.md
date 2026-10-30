# Changelog

## 1.0.0 - 2026-10-30

First public release, for Diablo II: Resurrected 3.3 under D2RLoader 1.3.1.

- New part `hud`: the lines are drawn as D2R's own text boxes in a plugin panel (layout served from memory under
  `data/global/ui/layouts/better-xp-bar/XpPanelhd.json`, no mod file replaced). D2RLoader's overlay accepted every
  draw but showed nothing in game, so the overlay no longer draws the lines while `hud` is on. Position, size and
  colour come from the config at load; restart the game after changing them. Tested in game (3440x1440,
  Reimagined): text shows and updates, does not block clicks, stays open when other panels open.
- New setting `display.text_align` (left, center, right; default left). `offset_x` / `offset_y` are now layout units
  to the aligned edge and to the bottom (top) edge; the defaults (-764, -345) put the box left-aligned just above
  the skill bar. Existing config files keep their old offsets: update them or delete the file to get the new defaults.
- Black box behind the lines like the XP bar's hover popup (`background`, default now 0.7, tooltip padding), wide
  enough for the longest lines.
- The session clock (`time`) is no longer in the default lines; add `"time"` to `lines` to show it.
- While the display is hidden, hovering the stock XP bar shows it (within about 50 ms: the game is read 20 times a
  second instead of 4).
- `XP/h` shows only the live rate; `Next level` shows minutes (`<1m`, `13m`, `3h 07m`), no seconds. XP/h refreshes
  every 5 s and Next level every 2 s instead of every read, so they no longer flicker.
- New setting `display.scale` (0.25..4, default 1): sizes the whole box (text, line height, padding).
- The XP line now shows progress through the level, `XP 1.35M/1.50M  73.31%`, formatted by the new setting
  `display.xp_format` (tokens `{current}`, `{needed}`, `{remaining}`, their `_full` forms, `{total}`, `{percent}`).
- Clicking the stock XP bar toggles the display, like the hotkey.
