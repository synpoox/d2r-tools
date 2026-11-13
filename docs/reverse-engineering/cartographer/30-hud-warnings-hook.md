## 30. HUDWarnings update: map text in the terror zone field (issue #371)

Inside any map, with a terror zone on or off, Cartographer shows the map name and monster level in the game's own terror zone text, so it replaces the
zone lines (`src/engine/map_panel.cpp`). Outside a map a zone displays normally. A missing or mismatching part keeps the plugin's own panel
([22-map-panel.md](22-map-panel.md)).

| Field | Record |
|---|---|
| Date | 2026-11-13 |
| Consumer | Cartographer (issue #371) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Location | `HUDWarnings` panel update `0x1476290` `(this, ...)`, called once per frame. On `this`: widgets at `+0x170` InfoText, `+0x178` BatteryInfoText, `+0x180` TerrorZoneInfoText (a child of `TerrorZoneInfoTextWrapper`, which is the one in the children list), `+0x188` HostileWarning, `+0x190` DurabilityWarning; the children array at `+0x58` with its count at `+0x60`; the info string at `+0x198` and the terror zone string at `+0x1e8` / `+0x1f0` (never written by the plugin). Widget vtable `+0x50` is `SetVisible(bool)` (flag at widget `+0x51`), parent at widget `+0x30`, scale float at `+0x80`, offset at `+0x70` / `+0x74`. The text call is `UiSetText` `0x86dd50` (see 22). The layout loop is `0x1476615`-`0x1476711`; it calls `0x8562a0` (widget size) and `0x856fb0` (widget position) |
| Guard | Byte signatures `HudWarningsUpdate` (the hook, 18-byte cut of seven whole instructions), `WidgetSize` (`0x8562a0`) and `WidgetMove` (`0x856fb0`) in `part_signatures.h`, each found once by `ResolvePart` (each matched one place in the 93847 image). A missing or mismatching part (any of the three, or `UiSetText`) is logged and the plugin keeps the #378 behaviour (own panel below the game's zone text) |
| Contract | In a map with map text, after the original returns: set the widget at `+0x180` to the map text, show it and its wrapper, then replay the game's layout loop over the children (a visible child, the hostile warning while InfoText is hidden, the durability warning always; `y` starts at 0; each child at `x = -(int)(width * scale)`, `y`, then `y += (int)(height * scale)`; the wrapper is sized from the text widget, as its own size stays 0 with no zone). The plugin's own panel stays closed only while that placed the text with a height above 0 and a visible flag set; otherwise the own panel shows. Outside a map nothing is touched. A fault (SEH) turns the writes off and the own panel takes over |
| Failure mode | Fail open, as above |
| Evidence | See "In game" |

### Why after the update, and why the layout is replayed

- The update runs every frame and sets the widget text each time, so a write from the frame callback is overwritten; the text is written after the original.
- With no zone the game hides the wrapper (flag `+0x51` is 0 and its size stays 0,0 at position 0,0), and the layout loop places only visible children. Showing the widget afterwards leaves it unplaced, so in a fresh game it drew nowhere useful (blank). After a zone had shown once the stale position from that zone's layout made it look right, which is why it appeared to depend on the session.
- An earlier version swapped the game's zone string for the map text during the original call. It is gone: the swap left the game's capacity field (`+0x1f8`) with the plugin's buffer (unsafe) and did not fix the position. A log right after the original showed the swapped string intact (length 51) but the wrapper hidden and at 0,0, so the string was never the cause.
- `0x14759c0` builds the info string, not the zone string.

### Not checked

Other resolutions or layouts, multiplayer, other builds, a zone with more than two lines, the controller HUD, the info panel hidden for the whole check with a zone (the no-zone map was seen with the info panel hidden and shown).

### In game

Slot 5, build 93847, test save, god mode, map Shorelines, the final DLL, game started fresh (no zone shown before), 1920x1080 (screenshots kept outside the repo):
- Map with no zone: the purple field reads the map name lines ("Monster level 90", "Monsters have", "+50% Monster Density") at the top right, the plugin's own gold and blue panel is closed.
- Map with a zone (`cheat desecrate 138`) and again after re-entering with the info panel on: the same text in the field under the game info, where "Terrorized" would be.
- Harrogath with a zone (`cheat desecrate 109`, info panel on): "Terrorized: 96 / Extra Fast" shows as normal. With the info panel off no zone lines show at all (the game's behaviour).
- After leaving the map (back in Harrogath with the zone on): the normal zone lines are back.
