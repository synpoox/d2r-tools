# Changelog

## Unreleased

- Open a map from its own item: a red portal from Harrogath, in Hell, once per game.
- Maps can only be opened in Hell; the old `Difficulty` column is ignored.
- Each map's `Item` in `maps.txt` names its item; there is no generic map item, random map or built-in map.
- Plain-English checking of `maps.txt` and `cartographer.txt`, with a skipped-row report in `cartographer-report.txt`.
- `cartographer check` lists problems in your tables; `cartographer open <Name>` opens a map in town without its item.
- `tools\install.ps1` adds the map level to your mod (Cave, or `-Tileset Crypt`), keeps a `.bak` and is safe to run twice.
- Every map is a generated Cave or Crypt maze; the `Preset` tileset is gone.
- Cartographer never changes the game's own levels; the map level is in no quest or level group.
- Random map properties from `mapstats.txt` apply to monsters; `PlayerStats=1` also gives them to you (experimental).
- `ItemStats=1` uses a map item's own stats as the map's properties (not checked in game yet).
- Map events: Boss, Horde, Ambush, Shrine and Treasure, with a banner on entry; Boss `Drop` rolls extra loot (Shrine and chest placement experimental).
- A `Monsters` list whose weights are all 0 or add up to more than 255 is refused with a log line; monsters the game never puts in a level's groups are left out.
- A map's `Boss` now appears in the far room of Cave and Crypt mazes.
- Each map's `Boss` is now its own boss from the new `bosses.txt`, never a vanilla boss (at most 16); run `tools\install.ps1` again.
- A map's `Boss` shows its `bosses.txt` Name in game (run `tools\install.ps1` again; not yet checked in game).
- Small maps get their event in a room far enough from where you arrive.
- You arrive in the planned entrance room of a maze.
- A map's `Level`, `Density`, `Uniques` and `Monsters` now apply (experimental).
- A map's own name shows on its portal, automap and entry banner.
- `AutoCheck=1` and `AutoCheckMap` run a game check by themselves in a Hell game, for test slots only (off by default).
- Multiplayer: TCP/IP clients learn the host's map, name and maze layout (experimental, not on Battle.net).
- `cartographer check` warns when `core-maps` is also loaded, and names unusable `mapstats.txt` rows.
- Optional `shapes.txt`, `mapstats.txt` and `mapevents.txt`, with problems reported.
- New guide "Your first map in 5 minutes", a column reference and an example set in `examples/first-map`.
- The map Boss and events are placed in the right room of a maze map.
- Hosts and players need the same version of Cartographer to share a map; older versions no longer connect for map sync.
- Multiplayer: TCP/IP clients also get the host's outdoor (Jungle) map layout (needs `OutdoorBuild=1`, not checked in game).
- Outdoor maps (off, not checked in game): you arrive in the planned entrance clearing, and the map Boss and event use their planned clearings.
- Jungle maps open from their item through a new `Outdoor` level (needs `OutdoorBuild=1`; run `tools\install.ps1` again; not checked in game yet).
- A map gives you at most 16 stats, the most the character sheet can show (experimental).
- Snow maps open from their item through a new `OutdoorSnow` level (needs `OutdoorBuild=1`; run `tools\install.ps1` again; not checked in game yet).
- Map names now show by default; set `MapNames=0` to turn them off.
