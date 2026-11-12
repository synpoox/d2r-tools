# Cartographer: map item type check (2026-11-12)

Live in game: the map item type from PR #228 (`origin/cartographer/item-name`; map items are `misc.txt` rows of their own item type `cmap`, "Map"),
slot 6, Hell, single player, test save `cartoh` (level 40 Barbarian), D2R 3.3, D2RLoader 1.3.1-beta. Staged with that branch's `stage_check.ps1 -MapItems`.
The `core-*.dll` plugins were parked for the run (they cause "Character save failed") and put back afterwards; slot 6 was unlocked afterwards. Nothing was
committed to #228. Refs #218. The logs and screenshots are not kept in the repository.

Builds: the Overgrown Reach check ran on `cdad137` (#228 after merging `origin/main`), with the example tables; the installed `cartographer.dll` matched
`dist\cartographer\cartographer.dll` (SHA-256 starts `03E09883`) and the log showed `cartographer: 0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`.
The other checks ran on `91c892d` (SHA-256 starts `15EB6A8F`). An earlier Overgrown Reach attempt on `91c892d` was a stale build from before #246 and is superseded.

## Passed

- **Tunneling Depths item (t21)** (`91c892d`): right-clicked from the inventory in its own new game; the item was consumed and its portal opened:

      Cartographer: Tunneling Depths is open.
      map sync: Tunneling Depths is open (row 1 'Tunneling Depths', slot level 138, opening 1, maze seed 15202109654749281282); 0 client(s) to tell

- **Overgrown Reach Map (t22), a Jungle map** (`cdad137`): right-clicked from the inventory in Harrogath in a new game. The item was consumed and a red portal opened:

      Cartographer: Overgrown Reach is open.
      map sync: Overgrown Reach is open (row 2 'Overgrown Reach', slot level 139, opening 1, outdoor seed 15463937561057475297); 0 client(s) to tell

  The log showed `outdoor slot level 139: built as planned (18 stamps, 432 void cells; ... 288 rooms)`, and the character stepped through the portal into a
  jungle level (arrival room marked as the level entry). There is no `OutdoorBuild` line in `cartographer.log` or `d2rloader.log`. Hovering the item before use
  showed "Overgrown Reach Map".
- **Belt** (`91c892d`): a map item could not be placed in the belt (a potion slot was emptied and the item was refused there). The base 4-slot belt was tested;
  the Sash in the test save was unequipped as a level requirement was unmet, so a belt with more slots was not.
- **Vendor (Malah)** (`91c892d`): none of his three tabs (Weapons, Weapons, Misc) sold a map item.
- **Sell** (`91c892d`): a Shorelines Map (t20) showed "Sell value: 40"; Ctrl-click sold it at Malah, gold went from 0 to 40, no errors in the plugin log.
- **Save and Exit with a map item in the inventory** (`91c892d`): an Overgrown Reach Map (t22) was spawned (`cheat spawn t22,inv 0`), then Save and Exit. No
  "Character save failed" or `BC_ASSERT` in `d2rloader.log`. After reloading the character the item was still in the inventory and its tooltip read
  "Overgrown Reach Map".
- **Inventory icon**: the map items show the scroll art (the Scroll of Identify picture), not a map icon. Recorded only.

## Not checked

- Larzuk, Anya and Qual-Kehk: their vendor tabs were not opened.
- Sell price at the other vendors.
- A belt with more than 4 slots.
- Other difficulties and more than one map item of the same kind in a stack or in the stash.
- Walking the Jungle level, fighting, and leaving by a portal; Save and Exit from inside the map.
