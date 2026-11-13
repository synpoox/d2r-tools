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
| Location | `HUDWarnings` panel update `0x1476290` `(this, ...)`, called once per frame. On `this`: widgets at `+0x170` InfoText, `+0x178` BatteryInfoText, `+0x180` TerrorZoneInfoText, `+0x188` HostileWarning, `+0x190` DurabilityWarning; the info string at `+0x198` (pointer) / `+0x1a0` (length); the terror zone string at `+0x1e8` / `+0x1f0`. Found from the layout widget names, then the one function that reads `+0x180` and `+0x1e8`. Widget vtable `+0x50` is `SetVisible(bool)`; the text call is `UiSetText` `0x86dd50` (see 22) |
| Guard | Byte signature `HudWarningsUpdate` in `part_signatures.h`, found once by `ResolvePart`; the inline hook cuts the first 18 bytes (whole instructions). A missing or mismatching part is logged and the plugin keeps the #378 behaviour (own panel below the game's zone text) |
| Contract | In a map with map text: with no zone (string length `+0x1f0` is 0) the hook points the zone string (`+0x1e8` / `+0x1f0`) at a copy of the map text for the original call, so the game shows the widget and lays it out, and puts its own string back after. It then sets the widget at `+0x180` to the map text and shows it. Outside a map nothing is touched. A fault (SEH) turns the writes off and the own panel takes over; no widget at `+0x180` also uses the own panel |
| Failure mode | Fail open, as above |
| Evidence | See "In game" |

### Why after the update, and the string swap

- The update runs every frame, so it rewrites the widget text each time: a write from the frame callback is overwritten, which is why 22 saw "nothing changes". The text is written after the original.
- The game's update sets the widget from the zone string and shows it only when the length is not 0; a layout loop over the panel's children then stacks only the visible ones. A widget shown after the update with no zone is drawn above the game info and clipped, so with no zone the string is swapped for the one call to get the widget shown and sized.
- The zone string is built elsewhere (`0x14759c0` builds the info string, not this one), so changing it only for the call leaves the game's own state alone.

### Known limit

In a game that has not yet shown a terror zone since launch (a fresh game), the no-zone text did not appear in the widget although the write is logged and the widget fields match a shown one when read from memory (not found why; the zone event handler may set up the widget on the first zone). After one zone has been shown in the session it works. A fresh game with a zone on also works.

### Not checked

Other resolutions or layouts, a zone with the info panel hidden (the Tab key toggles the info panel and its state persists), multiplayer, other builds, the no-zone text in a fresh game (above).

### In game

Slot 7, build 93847, test save, god mode on, map Shorelines (screenshots kept outside the repo): no zone (after a zone had been shown once): the purple lines read the map name and monster level; zone on (`cheat desecrate 138`): the same text replaces "Terrorized" and the modifiers; in Harrogath with the zone on, the normal terror zone lines show; after leaving the map the normal lines return. Fresh game with no zone: blank (limit above).
