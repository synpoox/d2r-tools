# Your first map

One map: an item opens a red portal in Harrogath (Hell only) to a generated level. The example tables are in
[`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/); every column is in
[tables.md](tables.md), Cartographer's own terms in the [glossary](glossary.md). What was checked in game is in
[checks/](checks/). More maps are one row and one item each; `mapstats.txt`, `mapevents.txt`, `shapes.txt` and
`cartographer.txt` are in tables.md.

You need D2RLoader, a mod, and its tables as `.txt` in `<Mod>.mpq\data\global\excel`: `levels`, `lvlmaze` and `misc`,
plus `superuniques` and `monstats` for bosses (without them a boss spawns as a plain monster).

## 1. Install

1. Copy `cartographer.dll` to `<game>\d2rloader\plugins\` (from a release zip; to build it see
   [CONTRIBUTING.md](../../CONTRIBUTING.md)).
2. Copy [`bosses.txt`](../../plugins/cartographer/examples/first-map/bosses.txt) to
   `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`. The installer reads it, so do this first.
3. Run the installer from the repository's `plugins\cartographer` folder (it is not in the release zip):
   `powershell -ExecutionPolicy Bypass -File tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`
4. It adds three levels to `levels.txt`, a `lvlmaze.txt` row and a `superuniques.txt` row per boss (each table backed
   up once as `.bak`), and writes `slots.txt`. It is safe to run twice. Expect lines like:

   ```
   levels.txt: added 'Cartographer Maze' as level N (Cave, copied from level N).
   slots.txt: Maze is level N, Outdoor is level N, OutdoorSnow is level N (other rows kept).
   ```

   `-Tileset Crypt` makes the Maze level a Crypt. If the mod ships `.bin` tables, delete or rebuild the `.bin` of each
   edited table, or the game ignores the `.txt`.

## 2. Make the map item

Each map is a row in the mod's `misc.txt` of type `cmap` (the item type `install.ps1` adds to `itemtypes.txt`, so the
tooltip shows the map's name), `pSpell` 16, `belt` 0, `useable` 1, and a free 3-character `code`. Copy the `isc` row and set
the values that [`map-items.txt`](../../plugins/cartographer/examples/first-map/map-items.txt) shows for the three example
maps. Add each name to `item-names.json` and the code to `items.json`. Cartographer ships no items: give players the item
through your own drop, shop or recipe. To test, `tools\stage_check.ps1 -MapItems` sets the rows up ([testing.md](testing.md#map-items-in-a-test-slot--mapitems)).

## 3. Add the map

Put `maps.txt` next to `bosses.txt`. Four columns are enough; the rest default (Medium, level 85, 6 properties,
any character level):

```
Name	Tileset	Shape	Item	Size	Boss	MinCharLevel
Shorelines	Cave	Corridor	t20	Large	Shoreline Warden	80
```

`Boss` must be a `Name` in `bosses.txt`. `Cave`, `Crypt`, `Jungle` and `Snow` are built. The example `maps.txt` has
three maps (`t20`, `t21`, `t22`); `check` skips a row whose item is not in your tables.

## 4. Check

Start the game through D2RLoader and run `cartographer check` in the console. It re-reads your tables and ends with:

```
Cartographer <version>: 3 map(s), 0 problem(s). The portal is on.
```

Problems are listed first, for example `maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?`; a bad
row is skipped and the rest load. The full list is `cartographer-report.txt`, next to `cartographer.log`, in the
plugin's log folder (`<game>\d2rloader\logs\`, or `mods\<Mod>\d2rloader\logs\`). If the portal is off, `check` says why.

## 5. Open it

`cartographer open Shorelines` opens the portal without the item (single player or a game you host, in Harrogath, in
Hell). With the item: a Hell character of at least `MinCharLevel` uses it in Harrogath and steps into the red portal.
The console shows `Cartographer: Shorelines is open.` A refused map stays in the inventory with a message saying why
(not Hell, not in town, level too low, a map already open this game, no free spot).

A map with a `Boss` logs this when its room is built:

```
map Boss 'Shoreline Warden': spawned in the boss room (x,y) 40 x 40 (as a superunique)
```

`refused by the game` in place of `spawned` means the boss did not appear.

## When something does not work

| Message | Cause |
|---|---|
| `Item ... cannot open a map: it needs item spell 16` | `pSpell` in the `misc.txt` row is not 16 |
| `item code ... is not in the item tables` | the code is not in the mod's `misc.txt`, `weapons.txt` or `armor.txt` |
| `tileset ... is planned but Cartographer cannot build it yet` | use `Cave`, `Crypt`, `Jungle` or `Snow` |
| `Check that the Maze row in slots.txt names a level that exists in levels.txt` | run `tools\install.ps1` and restart the game |
