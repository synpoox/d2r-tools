# Cartographer: map item type check (2026-11-12)

Live in game: the plugin built from the head of PR #228 (`91c892d`, `origin/cartographer/item-name`; map items are `misc.txt`
rows of their own item type `cmap`, "Map"), slot 6, Hell, single player, test save `cartoh` (level 40 Barbarian), D2R 3.3, D2RLoader
1.3.1-beta. Staged with that branch's `stage_check.ps1 -MapItems`. The installed `cartographer.dll` matched `dist\cartographer\cartographer.dll`
(SHA-256 starts `15EB6A8F`) and the log showed `cartographer: 0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`. The
`core-*.dll` plugins were parked for the run (they cause "Character save failed") and put back afterwards. Nothing was committed to #228. Refs #218. The
logs and screenshots are not kept in the repository.

## Passed

- **Tunneling Depths item (t21)**: right-clicked from the inventory in its own new game; the item was consumed and its portal opened:

      Cartographer: Tunneling Depths is open.
      map sync: Tunneling Depths is open (row 1 'Tunneling Depths', slot level 138, opening 1, maze seed 15202109654749281282); 0 client(s) to tell

- **Belt**: a map item could not be placed in the belt (a potion slot was emptied and the item was refused there). The base 4-slot belt was tested; the
  Sash in the test save was unequipped as a level requirement was unmet, so a belt with more slots was not.
- **Vendor (Malah)**: none of his three tabs (Weapons, Weapons, Misc) sold a map item.
- **Sell**: a Shorelines Map (t20) showed "Sell value: 40"; Ctrl-click sold it at Malah, gold went from 0 to 40, no errors in the plugin log.
- **Save and Exit with a map item in the inventory**: an Overgrown Reach Map (t22) was spawned (`cheat spawn t22,inv 0`), then Save and Exit. No
  "Character save failed" or `BC_ASSERT` in `d2rloader.log`. After reloading the character the item was still in the inventory and its tooltip read
  "Overgrown Reach Map". The item showed the scroll art, not a map icon.

## Found

- **Jungle maps are refused while `OutdoorBuild = 0`** (the default in the staged `cartographer.txt`). Right-clicking the Overgrown Reach Map (t22) in a new game
  showed "I can't use this yet" and the item stayed in the inventory:

      Cartographer: Jungle maps need OutdoorBuild = 1 in cartographer.txt (it is 0, so the outdoor build is off). Set it and restart the game.

  So the Overgrown Reach item was not consumed in this run.

## Not settled

- **Overgrown Reach opening a portal**: the above refusal; it needs a run with the outdoor build on, or without that toggle.
- **Larzuk, Anya and Qual-Kehk**: their vendor tabs were not opened. Map items have no vendor columns and are not spawnable, so a map item in a shop is not expected.
- **Sell price at the other vendors.**

## Not tested

- A belt with more than 4 slots.
- Other difficulties and more than one map item of the same kind in a stack or in the stash.
