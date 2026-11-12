# Cartographer: Crypt maze in Hell, Normal and Nightmare refusal, ItemStats, Save and Exit (2026-11-12)

Live in game: the plugin built from `main` at `8874045` (DLL SHA-256 `4024BE9C...C2005`, copied into the slot by
`stage_check.ps1 -Tileset Crypt -MapItems`; the plugin log line `cartographer: 0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`
followed each start), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7, single player, test save `cartoh` (a level 1 Barbarian;
the test save was put back from a copy afterwards). Game processes, in this order: Hell (two games in one process), Normal,
Nightmare, then three Hell processes for `ItemStats=1` (the first with an item that cannot be used, so no map opened; the second and third are the two results below). `debug_mode` was on in every game; `cheat god 1` was on in every game but the first.
Map: a Crypt `Corridor` Small map (`CryptHell`, item `t20`, Boss `Shoreline Warden`, `MinCharLevel` 1, `EventChance` 100) and a Crypt
`Loop` Small map (`CryptItem`, `MinCharLevel` 1). The logs and screenshots are not kept in the repository.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Crypt maze: opens in Hell, arrival at the portal in the entrance cell, Boss in the boss room, one event spawns | PASS (two Hell games; one note below) |
| 2 | Map item in Normal and Nightmare | Refused in both, with the message below; the item stayed in the inventory |
| 3 | `ItemStats=1` | PASS in one game (a map property taken from the item's own stat); one game where the item's stats gave nothing |
| 4 | Save and Exit inside a map, then reload | PASS: no "Character save failed" in the four loader logs kept (Hell, Normal, Nightmare, last `ItemStats` game) |

## Setup notes

- Slot 7 had a Cave `Cartographer Maze` row left from an earlier check; `install.ps1` leaves an existing Maze level as it is, so the
  slot's `levels.txt`, `lvlmaze.txt` and `superuniques.txt` were moved aside (copies kept) and re-seeded from vanilla, and the
  installer then added a Crypt Maze (level 138). They were put back after `-Restore`.
- `stage_check.ps1` parked the PD2 `core-maps`, `core-rules` and `core-ui` DLLs (`-Restore` put them back). No save failure occurred.
- The slot had no `test-harness.dll`, so a copy built from the `d2r-test-harness` repository was put in the slot's plugins folder for
  `game_ctl.py` (removed afterwards). The README says maps open only in Hell, and `MinCharLevel` is the character level needed (1 to 99).
  `MinCharLevel` was 1 here and the character was level 1; a higher value was not tried.

## 1. Crypt maze in Hell

Game 1 (Hell, Harrogath). Using `t20` from the inventory showed the red portal; the plugin log:

    opening 'CryptHell' (Crypt) through level 138
    maze slot level 138: built as planned (8 rooms, entrance placed)
    maze arrival: room 7 of 8 (at 6000,0) marked as the level entry, in plan cell (0,0); 0 other room(s) lost the mark
    Cartographer: CryptHell is open.
    event for 'CryptHell' (seed 1087747748152992844, chance 100%): Treasure 'Sunken Hoard' (quillrat1) at (0,0)

- **Opens in Hell**: PASS.
- **Arrival**: walking into the portal put the player beside the red portal in a room with Crypt walls and pillars (screenshot). The
  log marks the planned entrance cell (0,0) as the level entry. `MazeProbe` was off, so there is no probe line.
- **Boss room**: the plan's boss room was `(6024,32)`; `cheat goto 138,30140,180` took the player there and the log said
  `map Boss 'Shoreline Warden': spawned in the boss room (30120,160) 40 x 40 (as a superunique)`. The player died there to the pack
  (no god mode in that game), so the Boss was not looked at or fought. In game 2 (below) the same line appeared for a different
  layout, `(30080,120)` for a plan boss room `(6016,24)`, and a screenshot shows a pack of Zombies around the player.
- **One event**: the Treasure event's `spawned 1 of 1 in the event room (30000,120) 40 x 40` appeared once, when the player went to the
  event room. Seen once in that game; the Boss event rolled in game 2 was not walked to.

Game 2 (same process, second Hell game, `cheat god 1`): the map opened again with a new layout (10 rooms), the arrival line again
named plan cell (0,0), and the Boss spawned in the boss room.

Notes (not checked further):

- The game's loader log (`d2rloader.log`) had two assertions in game 1 and none in any later game or process: `BC_VERIFY: ptWarp`
  (`D2Common\src\Drlg\RoomTile.cpp:747`) at 14:37:48.987, the moment the map opened, and
  `BC_ASSERT: bPiece >= 0 && bPiece < (1 << nSize)` (`D2Game\src\Game\SCmd.cpp:3877`) at 14:38:23.397, the moment the Treasure event
  spawned. The game continued ("assertion dialogs are disabled"). The cause was not looked into; one game each.
- In a `Loop` Crypt map (game 5) the log said `built as planned (14 rooms, entrance an ordinary room)` and
  `maze arrival: room 5 of 14 (at 6000,8) marked as the level entry, in plan cell (0,1)`; arrival in that layout was not looked at.

## 2. Normal and Nightmare

The same item `t20` was used in Harrogath by the same character with Normal (game 3) and Nightmare (game 4) as the difficulty
(the save's current difficulty was changed with `save_edit.unlock_difficulties` to act 5 of that difficulty). Both times the plugin
log says:

    Cartographer: CryptHell can only be opened in Hell.

and the game showed `cartoh says: I can't use this yet.` (screenshots). No portal appeared, and the item was still in the inventory
afterwards (screenshots). This matches the README ("A refused map stays in your inventory"). No difficulty setting for a map exists in
`maps.txt`; the only rule seen is Hell.

## 3. `ItemStats=1`

README: "`1` uses the map item's own stats as the map's properties (not checked in game)". The test item needed to be usable and carry
affixes: a Small Charm base cloned as a map item (`pSpell` 16, `useable` 1), spawned with `cheat spawn <code>,typ m,inv 0`. A charm
without `useable` did nothing on right-click.

- **Game with no matching rows** (the stock example `mapstats.txt`): `ItemStats: 3 item stat(s) read, 0 used; drawing as usual`,
  after warnings that no `mapstats.txt` row has the stats and that a stat with a layer cannot be a map property (note: each warning
  prints `item stat 0`, even for the layer-9 one). The map drew its normal four properties.
- **Game with Players rows added to `mapstats.txt`** (strength, dexterity, and so on): `ItemStats: 1 item stat(s) read, 1 used` and
  `map stats for 'CryptItem' ... 1 drawn, players: strength(0)=+45`. Inside the map the player's strength went from 30 to 75
  (`statprobe: player 1 entered the map: totals [0=30] -> [0=75]`) and the character sheet showed Strength 75 in blue (screenshot).
  Leaving the map and the game was done by Save and Exit (item 4); strength after leaving was not read in this game.

Not checked: monster stats from an item, an item with 16 or more stats, and leaving the map to see the stat removed.

## 4. Save and Exit inside a map

Game 2 (Hell, inside the Crypt map with the Boss room around the player): Save and Exit from the Esc menu. `cartoh.d2s` was rewritten
at exit (modified 14:41:06, a different hash from before) and the character loaded in game 3 with its inventory. The four loader logs kept contain no `Character save failed` and no `encoding-error`. The `ItemStats` game was also left by Save and Exit inside
the map with the same result.

## What was not checked

A second map in one game, TCP/IP, a Boss event walked to, the Boss fought or seen up close, `MinCharLevel` above 1, the stat removed
on leaving after `ItemStats`, and a hand-walk (not `cheat goto`) to the boss room.
