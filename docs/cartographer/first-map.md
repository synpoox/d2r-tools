# Your first map

One map: an item opens a red portal in Harrogath (Hell only) to a generated level. The example tables are in
[`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/); every column is in
[tables.md](tables.md), Cartographer's own terms in the [glossary](glossary.md). What was checked in game is in
[checks/](checks/). More maps are one row and one item each; `mapevents.txt`, `shapes.txt` and
`cartographer.txt` are in tables.md.

You need D2RLoader, a mod, and its tables as `.txt` in `<Mod>.mpq\data\global\excel`: `levels`, `lvlmaze`, `itemtypes`, `misc`, `weapons` and `armor` (`check` reads the
item codes from the last three),
plus `superuniques` and `monstats` for bosses (without them a boss spawns as a plain monster).

## 1. Install

1. Copy `cartographer.dll` to `<game>\d2rloader\plugins\` (from a release zip; to build it see
   [CONTRIBUTING.md](../../CONTRIBUTING.md)).
2. Copy `examples\first-map\bosses.txt` from the zip (also
   [in the repo](../../plugins/cartographer/examples/first-map/bosses.txt)) to
   `<Mod>.mpq\data\global\excel\d2rloader\cartographer\` (create the folders if they are missing). The installer reads it,
   so do this first.
3. Run the installer from the unzipped plugin folder:
   `powershell -ExecutionPolicy Bypass -File tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`
4. It adds three levels to `levels.txt`, a `lvlmaze.txt` row, the `cmap` item type to `itemtypes.txt` and a
   `superuniques.txt` row per boss (each table backed up once as `.bak`), and writes `slots.txt`. It is safe to run
   twice. Expect lines like:

   ```
   levels.txt: added 'Cartographer Maze' as level N (Cave, copied from level N).
   slots.txt: Maze is level N, MazeCrypt is level N, MazeIce is level N, Outdoor is level N, OutdoorSnow is level N (other rows kept).
   ```

   Cave, Crypt and Ice each get a level of their own. If the mod ships `.bin` tables, delete or rebuild the `.bin` of each
   edited table, or the game ignores the `.txt`.

## 2. Make the map item

Each map is a row in the mod's `misc.txt` of type `cmap` (the item type `install.ps1` adds to `itemtypes.txt`, so the
tooltip shows the map's name), `pSpell` 16, `belt` 0, `useable` 1, `spellicon` -1, and a free
3-character `code`. Copy the `isc` row and set
the columns that [`map-items.txt`](../../plugins/cartographer/examples/first-map/map-items.txt) lists for the three example
maps (`name` is the row's `name` column, `namestr` the string key); blank the vendor `Min`/`Max` columns so no NPC sells it.
Then add two files to the mod, each starting from the game's own copy (the mod's file replaces the game's whole file):

- `<Mod>.mpq\data\hd\items\items.json`, one entry per code, using the picture of the `isc` scroll (without it the item has no
  icon): `{ "t20": { "asset": "scroll/identify_scroll" } }`
- `<Mod>.mpq\data\local\lng\strings\item-names.json` (and `strings-legacy\item-names.json`), one record per code, with an
  `id` no string file uses yet (below 65536, unique in its low 16 bits) and the `namestr` as `Key`:
  `{ "id": 28106, "Key": "t20", "enUS": "Shorelines Map", ...every other language the file has... }`

Cartographer ships no items: give players the item through your own drop, shop or recipe. To try one now, in the game
console run `debug_mode`, then `cheat spawn t20,inv 0` (the item lands in the inventory). Developers can stage all of this
with `tools\stage_check.ps1 -MapItems` ([testing.md](testing.md#map-items-in-a-test-slot--mapitems)).

## 3. Add the map

Put `maps.txt` next to `bosses.txt`. Four columns are enough; the rest default (Medium, level 85, 6 properties,
any character level):

```
Name	Tileset	Shape	Item	Size	Boss	MinCharLevel
Shorelines	Cave	Corridor	t20	Large	Shoreline Warden	80
```

`Boss` must be a `Name` in `bosses.txt`. `Cave`, `Crypt`, `Ice`, `Jungle` and `Snow` are built. The example `maps.txt` has
three maps (`t20`, `t21`, `t22`); `check` skips a row whose item is not in your tables.

## 4. Check

Start the game through D2RLoader and run `cartographer check` in the console. It re-reads your tables (so you can edit
`maps.txt` and run it again without a restart) and prints:

```
Cartographer <version>: 3 map(s), 0 problem(s). The portal is on.
```

If the mod has no `weapons.txt` or `armor.txt` you also get `some of the game's tables could not be read, so Monsters, Boss and
Item names are not checked`: the maps still load, but a wrong item code or boss name is not caught. Problems are listed first, for example `maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?`; a bad
row is skipped and the rest load. The full list is `cartographer-report.txt`, next to `cartographer.log`, in the
plugin's log folder (`<game>\d2rloader\logs\`, or `mods\<Mod>\d2rloader\logs\`). If the portal is off, `check` says why.

## 5. Open it

`cartographer open Shorelines` opens the portal without the item (single player or a game you host, in Harrogath, in
Hell). With the item: a Hell character of at least `MinCharLevel` (lower it in `maps.txt` for a low-level test character, then run `check`) uses it in Harrogath and steps into the red portal.
The console shows `Cartographer: Shorelines is open.` A refused map stays in the inventory with a message saying why
(not Hell, not in town, level too low, a map already open this game, no free spot).

A map with a `Boss` logs this when the player first comes near the boss room, which is at the far end of the maze (walk
there; `cheat goto <level>,<x>,<y>` warps), not when the portal is entered:

```
map Boss 'Shoreline Warden': spawned in the boss room (x,y) 40 x 40 (as a superunique)
```

`refused by the game` in place of `spawned` means the boss did not appear.

## When something does not work

| Message | Cause |
|---|---|
| `Item ... cannot open a map: it needs item spell 16` | `pSpell` in the `misc.txt` row is not 16 |
| `item code ... is not in the item tables` | the code is not in the mod's `misc.txt`, `weapons.txt` or `armor.txt` |
| `tileset ... is planned but Cartographer cannot build it yet` | use `Cave`, `Crypt`, `Ice`, `Jungle` or `Snow` |
| `Check that the Maze row in slots.txt names a level that exists in levels.txt` | run `tools\install.ps1` and restart the game |
