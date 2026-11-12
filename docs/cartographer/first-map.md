# Your first map

You will add one map to your mod: an item you own opens a red portal in Harrogath, and the portal leads to the map.
Plan on about an hour the first time (most of it is the item in step 2); a second map takes a few minutes.
Every file used here is in [`plugins/cartographer/examples/first-map/`](../../plugins/cartographer/examples/first-map/).
Every column is explained in [tables.md](tables.md). Unfamiliar words (maze level, tileset, slot, `pSpell`, superunique)
are in the [glossary](glossary.md).

> **Not tried in a game yet.** The steps match what the plugin checks today (unit tested), but nobody has opened a
> map in game by following this guide. The `Cave`, `Crypt`, `Jungle` and `Snow` tilesets are built, and the installer adds
> the level each one needs; the other tileset names are refused with a message until they are ready.

## What works today

A map opens a portal to a generated level that the installer adds (a maze for `Cave` and `Crypt`, an outdoor level for
`Jungle` and `Snow`). These columns take effect in game: `Name` (the message "Cartographer: <Name> is open." and
`cartographer open <Name>`), `Item` and `MinCharLevel` (who may open it, and when), `Tileset`, `Shape` and `Size` (the layout),
`Boss` (placed in the far room), `Properties` and `EventChance`. `Level`, `Density`, `Uniques` and `Monsters` are written into
the map's level when it opens (experimental, not verified in game). `Champions` is checked and reported but **not applied yet**.

## Prerequisites

- D2RLoader and Diablo II: Resurrected, and a mod. Cartographer reads nothing without one.
- The mod's tables as text files (not only `.bin`) in `<game>\mods\<Mod>\<Mod>.mpq\data\global\excel` (the folder inside the
  mod folder is named after the mod). The installer needs these there:
  - `levels.txt` and `lvlmaze.txt`: it stops if either is missing.
  - `superuniques.txt`, `monstats.txt` and `monumod.txt`: needed for `bosses.txt` (a map's `Boss`). Without
    `superuniques.txt` or `monstats.txt` the boss rows are not written (the installer says so) and a boss spawns as a plain monster.
  - `misc.txt`: where you add the map item (step 2). Without it the item check is skipped.

  A table your mod does not carry is not taken from the base game. For the vanilla 3.3.0 text tables, copy the file from
  `<game>\d2rloader\data\compiler\3.3.0\data\global\excel` into the mod's excel folder (copy only; never edit that folder).
- A way for the player to get the map item (step 2). Cartographer adds no items, drops or recipes. Use your mod's own drop,
  shop or cube recipe, or, to test only, add the item with whatever tool you already use for that.

## 1. Install the plugin

1. Close the game.
2. Get `cartographer.dll`. From a release zip, take it out of the zip. Or build it: install Visual Studio 2022 (C++
   workload) and CMake, clone the repository with `git clone --recurse-submodules https://github.com/synpoox/d2r-tools` (in
   an existing clone: `git submodule update --init --recursive`), then run
   `powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin cartographer` in the repository folder. The DLL is
   `dist\cartographer\cartographer.dll`.
3. Copy `cartographer.dll` to `<game>\d2rloader\plugins\`.
4. Create the folder `<Mod>.mpq\data\global\excel\d2rloader\cartographer\` (inside the mod's excel folder) and copy
   [`bosses.txt`](../../plugins/cartographer/examples/first-map/bosses.txt) from the example pack into it. Do this before
   the installer: it writes a `superuniques.txt` row for each boss in `bosses.txt`. If you add or change a boss later, run
   the installer again.
5. Find `install.ps1`. It is not in the release zip, which carries only the DLL and the docs. Download the repository (the
   green Code button on GitHub, Download ZIP, or the clone from step 2): the installer is
   `plugins\cartographer\tools\install.ps1` and the example pack is in `plugins\cartographer\examples\first-map\`.
6. Run it in PowerShell from the `plugins\cartographer` folder:
   `powershell -ExecutionPolicy Bypass -File tools\install.ps1 -ModDirectory "<game>\mods\<Mod>"`
   (`-ExecutionPolicy Bypass` is for this one run; without it Windows may say "running scripts is disabled on this system").
   It adds three levels to the mod's `levels.txt` (Maze, Outdoor and Snow), the maze row to `lvlmaze.txt` and one row per
   `bosses.txt` entry to `superuniques.txt` (backing up each table once as `.bak`), and writes `slots.txt`. The Maze level is
   a Cave; add `-Tileset Crypt` for a Crypt (every Cave or Crypt map then uses that one). It is safe to run twice. If it says
   a table is read-only, clear Read-only in the file's Properties and run it again; nothing was written.
7. If your mod ships `.bin` table files, the game reads those and ignores the edited `.txt`. For every table that was
   changed (`levels`, `lvlmaze`, `superuniques`, and `misc` after step 2), delete the `.bin` with the same name in the mod's
   excel folder (for example `levels.bin`); D2RLoader compiles the `.txt` itself when the game starts. Or rebuild the
   `.bin` files with D2RLoader.

## 2. Make an item that opens the map

Each map has an item of its own. In your mod's `misc.txt`, copy the row of an ordinary item (the Scroll of Identify, `isc`,
is a good base: not a potion, which carries stats and belt settings) to a new row and change these columns:

- `code`: 3 characters that no row of `misc.txt`, `weapons.txt` or `armor.txt` uses (the vanilla charms `cm1` to `cm3`
  and runes `r01` and up are taken). This guide uses `t20`. Use 3 characters: a 4-character code gets no picture (not yet
  confirmed in game).
- `name`: any text, for example `Shorelines Map`.
- `pSpell`: `16`.
- `namestr`: the key of the item's name in your `item-names.json` strings (add the record there).
- `rarity`, `spawnable`, `PermStoreItem` and `multibuy`: `0`, and clear every vendor `Min` and `Max` column (`CharsiMin`,
  `GheedMagicMax` and so on), so no NPC sells it and it never drops by chance. Set them differently if the item should be sold.

The new code also needs an entry in the mod's `data\hd\items\items.json` (`{ "t20": { "asset": "scroll/identify_scroll" } }`),
or the item has no icon.

The example pack's `maps.txt` has three maps, using items `t20`, `t21` and a Jungle map that uses `t22`; make a row for
each. [`examples/first-map/map-items.txt`](../../plugins/cartographer/examples/first-map/map-items.txt) lists the values
for those three (the columns above, by name). The Jungle map needs the `Outdoor` level from the
installer (run `tools\install.ps1`); without it the map is refused when you use its item.

Cartographer adds no items of its own, so decide how players get it (a drop, a shop or a cube recipe in your mod). To test
only, spawn it with the console: `debug_mode`, then `cheat spawn t20,inv 0`. In a Cartographer test slot,
`tools\stage_check.ps1 -MapItems` sets up the rows, picture entries and names for you; see
[testing.md](testing.md#map-items-in-a-test-slot--mapitems).

## 3. Add the map

Create `maps.txt` in `<Mod>.mpq\data\global\excel\d2rloader\cartographer\` (the folder from step 1, next to
`bosses.txt`). It is a tab-separated text file (a spreadsheet saved as "Text (Tab delimited)" works). Four columns are enough:

```
Name	Tileset	Shape	Item
Shorelines	Cave	Corridor	t20
```

A cell you leave out takes a default: size Medium, monster level 85, 6 random properties, any character level. Size and
properties apply in game; the monster level is written into the map's level (experimental, not verified in game).
Whatever the row says, a map opens only in Hell, in Harrogath, for a character of at least `MinCharLevel`.

Optional columns go to the right and a blank cell keeps the default. A fuller row (`Boss` must be a `Name` in your
`bosses.txt`; the example pack has `Shoreline Warden`):

```
Name	Tileset	Shape	Item	Size	Level	Boss	Monsters	MinCharLevel
Shorelines	Cave	Corridor	t20	Large	90	Shoreline Warden	quillrat1,zombie1	80
```

In game today `Size`, `Boss` and the other applied columns change the map; `Level` and `Monsters` are written into the
map's level when it opens (experimental, not verified in game).

The example pack's own `maps.txt` has three maps (`t20`, `t21` and a Jungle map with `t22`). Use it whole only when all
three items exist in your `misc.txt`; `check` reports and skips a row whose item is missing. The Jungle row works with the
levels the installer added.

Upper and lower case do not matter for Cartographer's own words (`Cave`, `Large`) or for item codes. Names
from the game's tables, like `quillrat1`, and the boss names in your `bosses.txt` must match exactly.

## 4. Start the game and check

1. Start the game through D2RLoader, open the console (D2RLoader's key; this repository does not record it, so see
   D2RLoader's own readme if the usual `~` key does nothing) and type `cartographer check`.
2. It says how many maps loaded and lists the problems (the console shows the first 10), for example "maps.txt row 3:
   tileset 'Jungel' is not known. Did you mean 'Jungle'?". A bad row is skipped; the others still load. All of them
   are also in `cartographer-report.txt` in `<game>\d2rloader\logs\`, next to `cartographer.log`.
3. Fix what it names and run `cartographer check` again; it re-reads your tables.

The check also reports whether the portal is on. If a game part could not be found, the tables still load but the portal stays
off, and `check` says which part.

## 5. Open the map

The quickest test is `cartographer open Shorelines` in the console: it opens the portal with no item, but only in a game you
host (not on someone else's server), once the game is running, in Harrogath, in a Hell game, with a character of at least the map's `MinCharLevel`.

To test the item: create a Hell character of level 80 or more (or leave `MinCharLevel` blank), get the
`t20` item by whatever way you set up in step 2, go to Harrogath and use it. A red portal appears beside you; step in.

Only one map can be opened per game. A refused map is meant to stay in your inventory with a message that says why (not Hell,
not in town, character level too low, a map already open this game, no free spot for the portal).

## Where to go next

- Add more maps: one row and one item each (Cave and Crypt maps share the one installed Maze level; Jungle and Snow have their own).
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
| "tileset ... is planned but Cartographer cannot build it yet" | use `Cave`, `Crypt`, `Jungle` or `Snow` for now |
| "slots.txt ... is missing" | run `tools\install.ps1` and restart the game |
| The item acts like an ordinary item | no map names that item in its `Item` cell |
| Nothing is read at all | the game is running without a mod, or the tables are not in the folder named in step 3 |
