# Changelog

## Unreleased

- Reads `maps.txt` and `cartographer.txt`, and writes `cartographer-report.txt` with a plain-English line for every row it skipped.
- A map item opens a red portal to the built-in preset map from Harrogath in Hell, once per game.
- `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` picks the map the next item opens.
- `tools\install.ps1` adds the map item and the map level to your mod's tables, checks every table first and keeps a `.bak` copy (safe to run twice).
- Without a mod, no tables are read; only the built-in map and the default settings apply.
- A `signatures.txt` entry that cannot be checked (wildcards where the game is patched, or not at the start of a function) is refused and logged.
- The map item's name is Cartographer's own text, so the installer no longer edits your `item-names.json`.
- Finds the engine parts the portal needs by byte signature (checked against game build 93847 only, not yet in game); the portal stays off if any part is not found exactly once.
- `cartographer check` looks again for engine parts that were missing at load and turns the portal on if all are now found.
