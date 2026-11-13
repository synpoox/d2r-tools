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
| Coexistence | Needs no mod file. The game's purple terror zone text is left alone (it cannot be hidden or written into, see below). The panel starts under it: the plugin reads the rect of `HUDWarnings` > `TerrorZoneInfoTextWrapper` (SDK Widget service, every 15 UI tasks) and puts blank lines in front of the panel text to reach its bottom; no Widget service means no padding |

### What was tried first, and why it was dropped

1. **The game's terror zone text** (`HUDWarnings` > `TerrorZoneInfoTextWrapper` > `TerrorZoneInfoText`, `hudwarningshd.json`). The three
   calls resolved the widgets and `SetText` returned, and the SDK `setWidgetVisible` succeeded on the wrapper and the text, but nothing showed
   on screen, even with the text set on every frame. The game's panel presumably rewrites the text and visibility each frame; the owner
   would be a hook on that panel's update, which was not searched for. (Live, 2026-11-12.) The code keeps the widget
   handle once found (looked up again after a failed call), marks the text restored only when the restore call succeeds (retried every 30 tasks),
   and shows it again if the panel open is refused. Live, 2026-11-12 (slot 5, test save, Hell, single player): the panel
   showed in a Cave map, and after the portal back to Harrogath (no zone active) it was gone and no empty or stale
   terror zone line was on screen. Still to check: with a terror-zone character, whether the game re-shows the text each
   frame.
2. **The Overlay service `drawText`**: the frame callback ran and every call returned `Success`, but nothing reached the screen, as
   `better-xp-bar.md` section (e) found.
3. **A plugin panel** (this): shows, under the game's info text. The same route `player-bars` uses.

### Under the terror zone text (issue #371)

The owner asked for the map name and level in place of the purple terror zone text (or appended into its field). Both were tried live (2026-11-12, slot 11, test save, Hell, single player, `cheat desecrate 139` for a zone, build 93847): `setWidgetVisible` on the wrapper and on the text returns Success and changes nothing, and `SetText` on `TerrorZoneInfoText` returns and shows nothing. The game rewrites that text and its visibility every frame before the plugin's AfterGameUi callback, so only a native hook on the game's HUDWarnings update could replace it (not searched for). Instead the panel starts under it. The wrapper rect (layout units, 2 per 1080p pixel) is 0 high with no zone and about 114 high in a two line zone; the panel text gets `padLines` blank lines (44 units each at 30 pt, at most 8) in front. Live after the change: with no zone the panel starts where it did before (y 147 at 1080p); with a zone ("Terrorized: 96", "Extra Fast") the name and level sit below the purple lines with no overlap (screenshots `f1-inmap.png` and `f1-zone2.png`, run carto-hudtext). Not checked: a zone with more than two lines, other resolutions, the text returning outside maps (it is never touched now).

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

## The entry banner (issue #357)

The Overlay service route above is also why the event banner never showed: `drawText` returned Success and drew nothing. The banner is now a second panel in the same
file (`cartographer/entry-banner`, two centred text boxes (event name and message; the map name is the game's own red Entering text, issue #370), open 6.1 s), set through the same FindPanel, FindChild and SetText parts on the UI thread, driven by the same
frame callback. It has its own on and fault state. Seen on screen at entry and after the event spawned: [banner check](../../cartographer/checks/2026-11-12-banner-check.md).
