# Your first map in 5 minutes

You will add one map to your mod: an item you own opens a red portal in Harrogath, and the portal leads to the map.
Every file used here is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).
Every column is explained in [tables.md](tables.md).

> **Not tried in a game yet.** The steps match what the plugin checks today (unit tested), but nobody has opened a
> map in game. Today the `Cave` and `Crypt` tilesets are built, and `Jungle` when `OutdoorBuild = 1`; the other tilesets are known names
> that are refused with a message until they are ready.

## What works today

A map opens a portal to a generated maze on the Maze level the installer adds. These columns take effect in game: `Name`
(the message "Cartographer: <Name> is open." and `cartographer open <Name>`), `Item` and `MinCharLevel` (who may open it,
and when), `Tileset` (Cave or Crypt), `Shape` and `Size` (the maze layout), `Boss` (placed in the far room), `Properties`
and `EventChance`. `Level`, `Density`, `Uniques` and `Monsters` are written into the map's level when it opens
(experimental, not verified in game). `Champions` is checked and reported but **not applied yet**.

## Prerequisites

- D2RLoader and Diablo II: Resurrected, and a mod. Cartographer reads nothing without one.
- The mod ships `levels.txt`, `lvlmaze.txt` and `misc.txt` as text tables in its own `data\global\excel` folder. The
  installer stops if `levels.txt` or `lvlmaze.txt` is missing, and without `misc.txt` the item spell check is skipped.
- A way for the player to get the map item (step 2). Cartographer adds no items, drops or recipes. Use your mod's own drop,
  shop or cube recipe, or, to test only, add the item with whatever tool you already use for that.

## 1. Install the plugin

1. Close the game.
2. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"` from the plugin folder. It adds the Maze level to the mod's
   `levels.txt` and `lvlmaze.txt` (backing each up once as `.bak`) and writes `slots.txt`. It builds a Cave level; add
   `-Tileset Crypt` for a Crypt (every map then uses that tileset). It is safe to run twice.
   If your mod ships `.bin` table files, rebuild them from the edited `.txt` tables or remove them (this also applies to
   the `misc.txt` edit in step 2).

## 2. Make an item that opens the map

Each map has an item of its own. In your mod's `misc.txt`, copy an ordinary item (a scroll is a good start) to a new row
and change two things:

- `code`: up to 4 characters that no other item uses. This guide uses `t20`.
- `pSpell`: `16`.

The example pack's `maps.txt` also has two more maps, one that uses item `t21` and a Jungle map that uses item `t22`, so
if you copy the whole pack, `misc.txt` needs `t20`, `t21` and `t22`. The Jungle map also needs `OutdoorBuild = 1` in
`cartographer.txt`; until then it is refused when you use its item.

Give it a name in your string tables so players can tell it apart. Cartographer adds no items of its own, so also decide
how players get it (a drop, a shop or a cube recipe in your mod).

## 3. Add the map

Create `maps.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`. It is a tab-separated text file (a
spreadsheet saved as "Text (Tab delimited)" works). Four columns are enough:

```
Name	Tileset	Shape	Item
Shorelines	Cave	Corridor	t20
```

Of the defaults a row is checked against (size Medium, monster level 85, 6 random properties), size and properties apply in game; the monster level is written into the map's level (experimental, not verified in game).
What does act: Hell difficulty, any character level, opened from Harrogath.

Optional columns go to the right and a blank cell keeps the default. A fuller row:

```
Name	Tileset	Shape	Item	Size	Level	Boss	Monsters	MinCharLevel
Shorelines	Cave	Corridor	t20	Large	90	Bishibosh	quillrat1,zombie1	80
```

In game today `Size`, `Boss` and the other applied columns change the map; `Level` and `Monsters` are accepted and
checked, then ignored.

Upper and lower case do not matter for Cartographer's own words (`Cave`, `Large`) or for item codes. Names
from the game's tables, like `Bishibosh` or `quillrat1`, must match exactly.

## 4. Start the game and check

1. Start the game through D2RLoader, then press the console key and type `cartographer check`.
2. It says how many maps loaded and lists the problems (the console shows the first 10), for example "maps.txt row 3:
   tileset 'Jungel' is not known. Did you mean 'Jungle'?". A bad row is skipped; the others still load. All of them
   are also in `cartographer-report.txt` in the plugin's log folder.
3. Fix what it names and run `cartographer check` again; it re-reads your tables.

The check also reports whether the portal is on. If a game part could not be found, the tables still load but the portal stays
off, and `check` says which part.

## 5. Open the map

The quickest test is `cartographer open Shorelines` in the console: it opens the portal with no item, but only in a game you
host (not on someone else's server), once the game is running, in Harrogath, with the map's gates met.

To test the item: create a Hell character of level 80 or more (or leave `MinCharLevel` blank), get the
`t20` item by whatever way you set up in step 2, go to Harrogath and use it. A red portal appears beside you; step in.

Only one map can be opened per game. A refused map is meant to stay in your inventory with a message that says why (not Hell,
not in town, character level too low, a map already open this game, no free spot for the portal).

## Where to go next

- Add more maps: one row and one item each (all use the installed tileset, Cave or Crypt).
- Plan a boss, a shape or a size: the optional columns in [tables.md](tables.md#mapstxt-required) apply now;
  `Level`, `Monsters`, `Density` and `Uniques` are experimental, and `Champions` is checked only.
- Random properties, events and your own shapes: `mapstats.txt`, `mapevents.txt` and `shapes.txt` (examples in the same
  folder) are applied; `cartographer check` reports bad rows.
- The defaults for every map: `cartographer.txt`. `Properties` and `PlayerStats` are used; the other settings are checked and reported but nothing uses them yet (see [tables.md](tables.md#cartographertxt-optional)).

## When something does not work

| Message or symptom | Likely cause |
|---|---|
| "Item ... cannot open a map: it needs item spell 16" | `pSpell` in your `misc.txt` row is not 16 |
| "item ... is not in the item tables" | the code is not in `misc.txt`, `weapons.txt` or `armor.txt` of the mod; the text suggests the closest code |
| "tileset ... is planned but Cartographer cannot build it yet" | use `Cave` or `Crypt` for now |
| "slots.txt ... is missing" | run `tools\install.ps1` and restart the game |
| The item acts like an ordinary item | no map names that item in its `Item` cell |
| Nothing is read at all | the game is running without a mod, or the tables are not in the folder named in step 3 |
