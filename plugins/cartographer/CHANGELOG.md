# Changelog

## Unreleased

- `MapNames=1` shows a map's own name on its portal, automap and entry banner (off by default; not verified in game).
- Small preset maps now get their event, in the first room far enough from where you arrive, and the banner names an event only after it has spawned (#115, #121; written, not verified in game).
- Shrine events place an object from `objects.txt` and a Boss `Drop` rolls an extra treasure class when the boss dies (written and unit tested; not yet verified in game).
- Ambush events spawn their waves when you reach the event room, and Treasure events spawn a fleeing monster or a chest that rolls `Drop` (written and unit tested; not yet verified in game).
- Boss and Horde events spawn what you name in `Spawn`, taken from the game's own tables (checked in game with a superunique name only).
- When you enter a map, its name is shown on screen with the event's name and message, and Boss and Horde events spawn in the event room (spawns checked in game; the banner text was not seen on screen).
- When a map opens, its random properties from `mapstats.txt` are applied to the map's monsters, event-spawned ones included (checked in game on cheat-spawned and event-spawned monsters).
- Player stats from a map are off by default; `PlayerStats=1` in `cartographer.txt` turns them on, for a test character only; they are sent through an unused game state so the character sheet can show them (written and unit tested; not verified in game), and an autosave inside the map is untested.
- `mapstats.txt` rows for `gold`, `hitpoints`, `level` and similar stats the game saves are refused on players, `hitpoints`, `mana` and `stamina` also on monsters, stats that need a skill, state or monster type number on both, and `cartographer check` names them.
- `cartographer check` reports a `Stat` in `mapstats.txt` that is not in `itemstatcost.txt`, with a suggestion.
- Map portal tooltip shows "Colossal Summit" instead of "Missing String"; run `tools\install.ps1` again to fix an older install (checked in game).
- Cave and Crypt maps can be built from their layout through a manual `Maze` slot (checked in game for Cave maps; Crypt not yet); one Maze slot serves one tileset (Cave or Crypt), and a map of the other tileset is refused with a message. Turn on `MazeProbe` in `cartographer.txt` to log what the game builds.
- `tools\install.ps1` keeps the other rows of `slots.txt` (such as the `Maze` row) when it runs again.
- `cartographer check` and the report now say when `core-maps` is loaded too; use one or the other.
- Reads the optional `shapes.txt`, `mapstats.txt` and `mapevents.txt` and reports problems in them, and lets a map's `Shape` name a `shapes.txt` row (checked only; not applied in game yet).
- New guide "Your first map in 5 minutes", a reference for every table column, and a small example set of tables in `examples/first-map`.
- Reads `maps.txt` and `cartographer.txt`, and writes `cartographer-report.txt` with a plain-English line for every row it skipped.
- Each map is opened by its own item (the `Item` in `maps.txt`); there is no general map item, random map or built-in map.
- A map's item opens a red portal to that map from Harrogath in Hell, once per game.
- `cartographer check` lists what is wrong in your tables; `cartographer open <Name>` opens a map without its item.
- `tools\install.ps1` adds the map level to your mod's tables (no items), checks every table first and keeps a `.bak` copy (safe to run twice).
- Reads the tables from the mod folder that D2RLoader 1.3.1 passes (before, no maps were found with that version).
- Without a mod, no tables are read and there are no maps; only the default settings apply.
- A `signatures.txt` entry that cannot be checked (wildcards where the game is patched, or not at the start of a function) is refused and logged.
- Finds the engine parts the portal needs by byte signature (checked in game on build 93847); the portal stays off if any part is not found exactly once.
- `cartographer check` looks again for the engine parts the game calls and turns the portal on if all are now found; a part the plugin hooks has to be found at load, or the game must be restarted.
