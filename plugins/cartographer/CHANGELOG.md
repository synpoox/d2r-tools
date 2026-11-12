# Changelog

## Unreleased

- Open a map from its own item: a red portal from Harrogath, in Hell, once per game.
- Maps can only be opened in Hell; the old `Difficulty` column is ignored.
- Each map's `Item` in `maps.txt` names its item; there is no generic map item, random map or built-in map.
- Plain-English checking of `maps.txt` and `cartographer.txt`, with a skipped-row report in `cartographer-report.txt`.
- `cartographer check` lists problems in your tables; `cartographer open <Name>` opens a map in town without its item.
- `tools\install.ps1` adds the map level to your mod (Cave, or `-Tileset Crypt`), keeps a `.bak` and is safe to run twice.
- Every map is a generated Cave or Crypt maze; the `Preset` tileset is gone.
- A slot that names a level of the original game or a town is ignored and reported, so it can never change those levels; the map level is in no quest or level group.
- Random map properties from `mapstats.txt` apply to monsters; `PlayerStats=1` also gives them to you (experimental).
- `ItemStats=1` uses a map item's own stats as the map's properties (not checked in game yet).
- Map events (Boss, Horde, Ambush, Shrine, Treasure) with an entry banner.
- A map's `Boss` now appears in the far room of Cave and Crypt mazes.
- Small maps get their event in a room far enough from where you arrive.
- You arrive in the planned entrance room of a maze.
- `MapNames=1` shows a map's own name on its portal, automap and entry banner (off by default).
- Multiplayer: TCP/IP clients learn the host's map, name and maze layout (experimental, not on Battle.net).
- `cartographer check` warns when `core-maps` is also loaded, and names unusable `mapstats.txt` rows.
- Optional `shapes.txt`, `mapstats.txt` and `mapevents.txt`, with problems reported.
- New guide "Your first map in 5 minutes", a column reference and an example set in `examples/first-map`.
