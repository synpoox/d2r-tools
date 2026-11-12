# Cartographer: outdoor gaps check (2026-11-12)

Live in game: the plugin built from `main` at `8efa0d6` (DLL SHA-256 `4a870f5b...afb2`; the installed DLL hash matched the build and the log
"loaded" line was the new start, `0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`), slot 5, Hell, single player,
test save `cartoh` (a level 1 Barbarian), D2R 3.3 build 93847, D2RLoader 1.3.1-beta. Staged with `stage_check.ps1 -MapItems` and a
scratch pack: `maps.txt` with 'Overgrown' (Jungle, Winding, Medium) and 'Frostline' (Snow, Winding, Medium), `OutdoorBuild=1`,
`OutdoorProbe=1`, `EventChance=0`, no Boss, the default `PlayerStats` (on), and `mapstats.txt` with Players rows `strength +25` and
`coldresist +30`. The default automap seed was used (no `-CoreUiAutomap`). Both maps were opened with `cartographer open <Name>`, not from an
item; the map items' tooltip was not tested. The earlier run is [map item check](2026-11-12-items-check.md). Logs and
screenshots are not kept in the repository.

| Check | Result |
|---|---|
| 1. Jungle entry banner | PASS: "ENTERING OVERGROWN" seen on screen after the portal (`jenter_6.png`, and again in `jungle_charsheet2.png`). |
| 2. Snow entry banner | PASS: "ENTERING FROSTLINE" seen on screen after the portal (`senter_6.png`). |
| 3. One outdoor map per game | PASS (by console): with 'Overgrown' open, `cartographer open Frostline` was refused (`s5_second_open.png`, log below). Not tried from a second map item. |
| 4. Map name on the automap | PASS: the automap title (top right) read "OVERGROWN" in the Jungle map (`jungle_automap.png`). Snow's automap was not captured: the level 1 character died in Frostline before the shot. |
| 5. PlayerStats | PASS in the Jungle map, 3 times in and out: see below. |

Check 5, from the character sheet (screenshots `jungle_charsheet.png`, `town_charsheet.png`, `jungle_charsheet2.png`, `reload_charsheet.png`):

- In town: Strength 30, Cold Resistance -100%.
- In the map: Strength 55 (shown blue) and Cold Resistance -70%, which is the drawn +25 and +30.
- Back in Harrogath through the portal: Strength 30, Cold Resistance -100% again.
- Entered the map a third time (Strength 55, -70% again), then Save and Exit from inside the map. The loader log has no "Character save failed". After
  Play and Hell, in Harrogath: Strength 30, Cold Resistance -100% (base stats kept).

Log lines:

```
Cartographer: Overgrown is open.
Cartographer: opening Frostline...
Cartographer: an outdoor level was already built (or is being built) in this game, so Frostline cannot be opened now. Leave the game and start a new one.
statprobe: player 1 entered the map: totals [43=0 0=30] -> [43=30 0=55], list on chain: yes (3 -> 4 lists), list flags 0x0 state 39 (state given 39)
statprobe: player 1 left the map: list removed (chain 4 -> 3 lists)
statprobe: left the game: 21 monster(s) and 3 player(s) were given the map's stats
Cartographer: Frostline is open.
```

0 `[ERROR]` and 0 FAULT lines in the plugin log (2 games).

## Not settled / limits

- The banner was caught in the Jungle map once the screenshots were taken in a quick burst; the first entry (a single shot after a 2 s wait) missed it.
- The Frostline game ended with the character dead (level 1 in Hell); the Save and Exit click that followed was not confirmed, and no
  stats were checked in Snow.
- The one-map refusal was seen only for a console open, Jungle then Snow, not for a second map item or the same tileset.
- `core-maps.dll` is in the slot's plugins folder, and the plugin logged its "loaded next to Cartographer" warning; the console open does not
  depend on the item hook.
- Player stats were checked on one test character, single player, level 1, only on the Jungle map.
