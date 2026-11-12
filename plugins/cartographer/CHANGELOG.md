# Changelog

## Unreleased

- Reads `maps.txt` and `cartographer.txt`, and writes `cartographer-report.txt` with a plain-English line for every row it skipped.
- A map item opens a red portal to the built-in preset map from Harrogath in Hell, once per game.
- `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` picks the map the next item opens.
- `tools\install.ps1` adds the map item and the map level to your mod's tables (safe to run twice).
- Finds the engine parts the portal needs by byte signature (checked against game build 93847 only, not yet in game); the portal stays off if any part is not found exactly once.
