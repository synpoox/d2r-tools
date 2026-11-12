# Your first map in 5 minutes

You will add one map to your mod: an item you own opens a red portal in Harrogath, and the portal leads to the map.
Every file used here is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).
Every column is explained in [tables.md](tables.md).

> **Not tried in a game yet.** The steps match what the plugin checks today (unit tested), but nobody has opened a
> map in game. Today only the `Preset` tileset is built; `Cave`, `Jungle` and the others are known names that are refused
> with a message until they are ready.

You need a mod (Cartographer reads nothing without one), D2RLoader, and Diablo II: Resurrected.

## 1. Install the plugin

1. Close the game.
2. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"` from the plugin folder. It adds one level to the mod's
   `levels.txt` and `lvlprest.txt` (backing each up once as `.bak`) and writes `slots.txt`. It is safe to run twice.
   If your mod ships `.bin` table files, rebuild them from the edited `.txt` tables or remove them.

## 2. Make an item that opens the map

Each map has an item of its own. In your mod's `misc.txt`, copy an ordinary item (a scroll is a good start) to a new row
and change two things:

- `code`: up to 4 characters that no other item uses. This guide uses `t20`.
- `pSpell`: `16`.

Give it a name in your string tables so players can tell it apart. Cartographer adds no items of its own.

## 3. Add the map

Create `maps.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`. It is a tab-separated text file (a
spreadsheet saved as "Text (Tab delimited)" works). Four columns are enough:

```
Name	Tileset	Shape	Item
Shorelines	Preset	Corridor	t20
```

That map gets the defaults: size Medium, monster level 85, 6 random properties, Hell difficulty, any character level,
opened from Harrogath. `Shape` has to be filled in but a `Preset` map does not use it yet.

Optional columns go to the right and a blank cell keeps the default. A tuned version:

```
Name	Tileset	Shape	Item	Size	Level	Boss	Monsters	Difficulty	MinCharLevel
Shorelines	Preset	Corridor	t20	Large	90	Bishibosh	quillrat1,zombie1	Hell	80
```

Upper and lower case do not matter for Cartographer's own words (`Preset`, `Large`, `Hell`) or for item codes. Names
from the game's tables, like `Bishibosh` or `quillrat1`, must match exactly.

## 4. Start the game and check

1. Start the game through D2RLoader, then press the console key and type `cartographer check`.
2. It says how many maps loaded and lists every problem, for example "maps.txt row 3: tileset 'Jungel' is not known.
   Did you mean 'Jungle'?". A bad row is skipped; the others still load. The same text is in `cartographer-report.txt`
   in the plugin's log folder.
3. Fix what it names and run `cartographer check` again; it re-reads your tables.

The check also reports whether the portal is on. If a game part could not be found, the tables still load but the portal stays
off, and `check` says which part.

## 5. Open the map

Create a Hell character of level 80 or more (or leave `Difficulty` and `MinCharLevel` blank), go to Harrogath, and use the `t20`
item. A red portal appears beside you; step in. You can also type `cartographer open Shorelines` in the console to open
the portal with no item.

Only one map can be opened per game. A refused map is meant to stay in your inventory with a message that says why (wrong difficulty,
not in town, character level too low, a map already open this game, no free spot for the portal).

## Where to go next

- Give the map a boss, monsters, a size or a level: the optional columns in [tables.md](tables.md#mapstxt-required).
- Change the random properties or add events: `mapstats.txt` and `mapevents.txt` (examples in the same folder). The plugin does
  not read these two yet.
- Change the defaults for every map: `cartographer.txt`.

## When something does not work

| Message or symptom | Likely cause |
|---|---|
| "Item ... cannot open a map: it needs item spell 16" | `pSpell` in your `misc.txt` row is not 16 |
| "item ... is not in the item tables" | the code is not in `misc.txt`, `weapons.txt` or `armor.txt` of the mod; the text suggests the closest code |
| "tileset ... is planned but Cartographer cannot build it yet" | use `Preset` for now |
| "slots.txt ... is missing" | run `tools\install.ps1` and restart the game |
| The item acts like an ordinary item | no map names that item in its `Item` cell |
| Nothing is read at all | the game is running without a mod, or the tables are not in the folder named in step 3 |
