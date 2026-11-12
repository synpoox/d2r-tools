# Changelog

## Unreleased

- Open a map from its own item: a red portal from Harrogath, in Hell, once per game.
- Maps can only be opened in Hell; the old `Difficulty` column is ignored.
- Each map's `Item` in `maps.txt` names its item; there is no generic map item, random map or built-in map.
- Plain-English checking of `maps.txt` and `cartographer.txt`, with a skipped-row report in `cartographer-report.txt`.
- `cartographer check` lists problems in your tables; `cartographer open <Name>` opens a map in town without its item.
- `tools\install.ps1` adds the Maze level to your mod's tables (no items; `-Tileset Crypt` for a Crypt), keeps a `.bak` copy and is safe to run twice.
- Every map is now generated; the Preset tileset is gone. Cave and Crypt maps are built from their layout; one Maze level serves one tileset.
- A slot that names a level of the original game or a town is ignored and reported, so it can never change those levels.
- Random map properties from `mapstats.txt` apply to the map's monsters; `PlayerStats=1` adds player properties (off by default, test characters only, experimental).
- Map events: Boss, Horde, Ambush, Shrine and Treasure, with a banner on entry; Boss `Drop` rolls extra loot (Shrine and chest placement experimental).
- A map's `Boss` now appears in the far room of Cave and Crypt mazes.
- Small maps get their event in a room far enough from where you arrive.
- You arrive in the planned entrance room of a maze.
- `MapNames=1` shows a map's own name on its portal, automap and entry banner (off by default).
- Multiplayer: TCP/IP clients learn the host's map, name and maze layout (experimental, not on Battle.net).
- The map portal tooltip reads "Colossal Summit" instead of "Missing String"; run `tools\install.ps1` again on an older install.
- `cartographer check` warns when `core-maps` is also loaded, and names unusable `mapstats.txt` rows.
- Optional `shapes.txt`, `mapstats.txt` and `mapevents.txt`, with problems reported (`shapes.txt` is checked only for now).
- New guide "Your first map in 5 minutes", a column reference and an example set in `examples/first-map`.
