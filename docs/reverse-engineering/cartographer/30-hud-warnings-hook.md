## 30. HUDWarnings update: map text in the terror zone field (issue #371)

While a zone is on and a map is open, Cartographer writes the map name and monster level into the game's own terror zone text, so it replaces the
zone lines (`src/engine/map_panel.cpp`). With no zone the game keeps that widget hidden and laid out empty, so the plugin's own panel
([22-map-panel.md](22-map-panel.md)) shows as before.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #371) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, D2RLoader 1.3.1-beta |
| Location | `HUDWarnings` panel update `0x1476290` `(this, ...)`, called once per frame. On `this`: widgets at `+0x170` InfoText, `+0x178` BatteryInfoText, `+0x180` TerrorZoneInfoText, `+0x188` HostileWarning, `+0x190` DurabilityWarning; the info string at `+0x198` (pointer) / `+0x1a0` (length); the terror zone string at `+0x1e8` / `+0x1f0`. Found from the layout widget names, then the one function that reads `+0x180` and `+0x1e8`. Widget vtable `+0x50` is `SetVisible(bool)`; the text call is `UiSetText` `0x86dd50` (see 22) |
| Guard | Byte signature `HudWarningsUpdate` in `part_signatures.h`, found once by `ResolvePart`; the inline hook cuts the first 18 bytes (whole instructions). A missing or mismatching part is logged and the plugin keeps the #378 behaviour (own panel below the game's zone text) |
| Contract | Hook calls the original, then, when a map is open, the game's zone string length (`+0x1f0`) was and is not 0 and the map text is not empty, sets the widget at `+0x180` to the map text and shows it. Otherwise nothing is written. A fault (SEH) turns the hook's writes off and the own panel takes over |
| Failure mode | Fail open, as above |
| Evidence | See "In game" |

### Why after the update, and only with a zone

- The update runs every frame (about 3240 calls in about 3 minutes in a map, from a temporary log), so it rewrites the widget text each time: a write from the frame callback is overwritten, which is why 22 saw "nothing changes".
- Pointing the string at `+0x1e8` / `+0x1f0` at the map text before the original (tried) does nothing visible: the game builds the zone text elsewhere (`0x14759c0`) and sets the widget from that.
- A widget shown after the update with no zone is drawn above the game info and clipped: the layout pass had run with it hidden. With a zone on, the widget is already laid out, and the written text takes its place.

### Not checked

Other resolutions or layouts, a zone with the info panel hidden, multiplayer, other builds, town (the hook passes through outside a map; not looked at).

### In game

Slot 11, build 93847, test save, `cheat desecrate 139` in the map Shorelines (screenshots `h8-tab`, `h8-zone1`, `h8-zone2`, kept outside the repo): with the zone on, the purple lines read "Shorelines / Monster level 90" where "Terrorized: 96 / Extra Fast" was; with no zone the gold own panel sits under the game info as in 22. The hook's write was logged.
