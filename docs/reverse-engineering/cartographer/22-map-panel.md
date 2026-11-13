## 22. Map panel on the right of the screen (issue #308)

Inside a map, a plugin panel under the game's area and game info shows the map's name, monster level and rolled properties
(`src/engine/map_panel.cpp`, text in `src/mappanel`). Outside a map the panel is closed and the game's HUD is untouched.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #308) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Location | Three UI calls, the same ones `player-bars` and `dps-counter` use (`better-xp-bar.md`, `dps-counter.md`, `player-bars.md`): `FindPanel(name)` `0x846170`, `FindChild(parent, name)` `0x856220` (direct children only), `TextBoxWidget::SetText(box, {ptr, len})` `0x86dd50` |
| Guard | Byte signatures in `part_signatures.h` (`UiFindPanel`, `UiFindChild`, `UiSetText`), each found once by `ResolvePart`; a part that is not found leaves the panel off. Only called, never hooked |
| Contract | The panel is a layout the plugin registers (`data/global/ui/layouts/cartographer/map-panel` through the Resource service, `cartographer/map-panel` through the Panel service): a `Panel` anchored top right with one `TextBoxWidget` named `Text`, right aligned. It is opened and closed with `openPanel`/`closePanel` and its text set on the UI thread (`runOnUiThread` from an Overlay frame callback) |
| Failure mode | Fail open: a missing part or service, a refused panel, or a fault (SEH around the three calls) turns the panel off with a log line; nothing else changes |
| Coexistence | Needs no mod file. The panel stands in place of the terror zone text: while the panel shows, the plugin sets `TerrorZoneInfoText` invisible (SDK Widget service, every UI task in a map) and visible again when the panel closes; no Widget service means the text is left alone. The rest of `HUDWarnings` is not touched |

### What was tried first, and why it was dropped

1. **The game's terror zone text** (`HUDWarnings` > `TerrorZoneInfoTextWrapper` > `TerrorZoneInfoText`, `hudwarningshd.json`). The three
   calls resolved the widgets and `SetText` returned, and the SDK `setWidgetVisible` succeeded on the wrapper and the text, but nothing showed
   on screen, even with the text set on every frame. The game's panel presumably rewrites the text and visibility each frame; the owner
   would be a hook on that panel's update, which was not searched for. (Live, 2026-11-12.) The code keeps the widget handle once found (looked up again after a failed call), marks the text restored only when the restore call succeeds, and shows it again if the panel open is refused. Live, 2026-11-12 (slot 5, test save, Hell, single player): the panel showed in a Cave map, and after the portal back to Harrogath (no zone active) it was gone and no empty or stale terror zone line was on screen. Still to check: with a terror-zone character, whether the game re-shows the text each frame.
2. **The Overlay service `drawText`**: the frame callback ran and every call returned `Success`, but nothing reached the screen, as
   `better-xp-bar.md` section (e) found.
3. **A plugin panel** (this): shows, under the game's info text. The same route `player-bars` uses.

### In place of the terror zone text

The owner asked for the panel in place of the purple text. Hiding it is done with the SDK `setWidgetVisible` on `HUDWarnings` > `TerrorZoneInfoTextWrapper` > `TerrorZoneInfoText`
(the call returns Success live). Whether the hide changes anything was not seen: there is no zone API (d2rAI-toolkit `knowledge/tables.md`, "Terror Zones"), the map's
slot level is a level the schedule never lists, and the game's text never drew in the earlier attempt, so no zone text could be put on screen to hide. The hide is
a safeguard (the README says only that the panel shows in a map, not that it replaces the text); the panel sits at the top right (`PanelTop` 270) just under the game's info text, where the zone text would be. (Live, 2026-11-12.)

### Validation

- Unit tests: `tests/mappanel_tests.cpp` (text only).
- Live in game, 2026-11-12, one test slot (11), test mod and test save, Hell, single player, build 93847: in a Tunneling Depths map the
  panel showed the map name, "Monster level 85" and the four rolled properties ("Monsters: +13% Life", "Monsters: +18% Fire Resistance",
  "You: -19% Life Regeneration", "You: -22% Fire Resistance") at the right, under the game's own info text with the automap open; after the
  portal back to Harrogath it was gone. Second pass, same day, after the fix round: in a map with no rolled properties (the test
  tables have no Properties column) the panel showed the name and "Monster level 90" under the five info lines (area, difficulty,
  game version, mod) with the automap open, and was gone after leaving. Not checked: a map with properties after the fix, a terror
  zone, a TCP/IP client (it does not know the properties), a second map in a game, other
  resolutions, other languages, a Steam build.
