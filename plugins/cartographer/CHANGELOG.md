# Changelog

## Unreleased

- Reads `maps.txt` and `cartographer.txt`, and writes `cartographer-report.txt` with a plain-English line for every row it skipped.
- Each map is opened only by its own item (required, unique `Item` in `maps.txt`, item spell 16); no generic map item, random map or built-in `Preset` map.
- A map's item opens a red portal to that map from Harrogath in Hell, once per game.
- `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` opens a named map without an item (it refuses with a message in a game you joined over TCP/IP, or if the game part is missing).
- `tools\install.ps1` adds the map level to your mod's tables (no items), checks every table first and keeps a `.bak` copy (safe to run twice).
- Without a mod, no tables are read; only the built-in map and the default settings apply.
- A `signatures.txt` entry that cannot be checked (wildcards where the game is patched, or not at the start of a function) is refused and logged.
- Finds the engine parts the portal needs by byte signature (checked against game build 93847 only, not yet in game); the portal stays off if any part is not found exactly once.
- `cartographer check` looks again for the engine parts the game calls and turns the portal on if all are now found; a part the plugin hooks has to be found at load, or the game must be restarted.
