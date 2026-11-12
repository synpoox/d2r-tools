# Your first map in 5 minutes

You will add one map to your mod: an item you own opens a red portal in Harrogath, and the portal leads to the map.
Every file used here is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).
Every column is explained in [tables.md](tables.md).

> **Not tried in a game yet.** The steps match what the plugin checks today (unit tested), but nobody has opened a
> map in game. Today only the `Preset` tileset is built; `Cave`, `Jungle` and the others are known names that are refused
> with a message until they are ready.

## What works today

Today a map only **opens a portal**. These columns take effect in game: `Name` (the message "Cartographer: <Name> is open."
and `cartographer open <Name>`), `Item` and `MinCharLevel` (who may open it, and when), and `Tileset` (written
to the log). `Level`, `Density`, `Uniques` and `Monsters` are written into the map's level when it opens (experimental, not
verified in game). Everything else (`Size`, `Boss`, `Properties`, `EventChance`, `Champions`, and the settings in
`cartographer.txt`) is checked and reported but **not applied yet**: every map opens the same
level, the one preset level the installer adds. Generated layouts, bosses and properties come in later phases.

## Prerequisites

- D2RLoader and Diablo II: Resurrected, and a mod. Cartographer reads nothing without one.
- The mod ships `levels.txt`, `lvlprest.txt` and `misc.txt` as text tables in its own `data\global\excel` folder. The
  installer stops if `levels.txt` or `lvlprest.txt` is missing, and without `misc.txt` the item spell check is skipped.
- A way for the player to get the map item (step 2). Cartographer adds no items, drops or recipes. Use your mod's own drop,
  shop or cube recipe, or, to test only, add the item with whatever tool you already use for that.

## 1. Install the plugin

1. Close the game.
2. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
3. Run `tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"` from the plugin folder. It adds one level to the mod's
   `levels.txt` and `lvlprest.txt` (backing each up once as `.bak`) and writes `slots.txt`. It is safe to run twice.
   If your mod ships `.bin` table files, rebuild them from the edited `.txt` tables or remove them (this also applies to
   the `misc.txt` edit in step 2).

## 2. Make an item that opens the map

Each map has an item of its own. In your mod's `misc.txt`, copy an ordinary item (a scroll is a good start) to a new row
and change two things:

- `code`: up to 4 characters that no other item uses. This guide uses `t20`.
- `pSpell`: `16`.

The example pack's `maps.txt` also has a second map that uses item `t21`, so if you copy the whole pack,
`misc.txt` needs both `t20` and `t21`.

Give it a name in your string tables so players can tell it apart. Cartographer adds no items of its own, so also decide
how players get it (a drop, a shop or a cube recipe in your mod).

## 3. Add the map

Create `maps.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\`. It is a tab-separated text file (a
spreadsheet saved as "Text (Tab delimited)" works). Four columns are enough:

```
Name	Tileset	Shape	Item
Shorelines	Preset	Corridor	t20
```

Of the defaults a row is checked against (size Medium, monster level 85, 6 random properties), none is applied in game yet.
What does act: Hell difficulty, any character level, opened from Harrogath. `Shape` has to be filled in but a `Preset` map
does not use it yet.

Optional columns go to the right and a blank cell keeps the default. A fuller row:

```
Name	Tileset	Shape	Item	Size	Level	Boss	Monsters	MinCharLevel
Shorelines	Preset	Corridor	t20	Large	90	Bishibosh	quillrat1,zombie1	80
```

In game today only `Item` and `MinCharLevel` change what happens here; `Size`, `Level`, `Boss` and
`Monsters` are accepted and checked, then ignored, so this map opens the same level as the plain row above.

Upper and lower case do not matter for Cartographer's own words (`Preset`, `Large`, `Hell`) or for item codes. Names
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

- Add more maps: one row and one item each. Each row's `Item` and `MinCharLevel` work now.
- Plan a boss, monsters, a size or a level: the optional columns in [tables.md](tables.md#mapstxt-required) are checked now
  and will apply once layouts are generated (later phases).
- Random properties, events and your own shapes: `mapstats.txt`, `mapevents.txt` and `shapes.txt` (examples in the same
  folder). The plugin reads and checks them now, and `cartographer check` reports bad rows, but nothing applies them yet.
- The defaults for every map: `cartographer.txt`. Its settings are checked, not applied yet.

## When something does not work

| Message or symptom | Likely cause |
|---|---|
| "Item ... cannot open a map: it needs item spell 16" | `pSpell` in your `misc.txt` row is not 16 |
| "item ... is not in the item tables" | the code is not in `misc.txt`, `weapons.txt` or `armor.txt` of the mod; the text suggests the closest code |
| "tileset ... is planned but Cartographer cannot build it yet" | use `Preset` for now |
| "slots.txt ... is missing" | run `tools\install.ps1` and restart the game |
| The item acts like an ordinary item | no map names that item in its `Item` cell |
| Nothing is read at all | the game is running without a mod, or the tables are not in the folder named in step 3 |
