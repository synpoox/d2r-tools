# Kurast tileset game check (issue #507)

Slot 3, Hell, new level-99 test character, pd2r from a fresh `origin/main` worktree, this branch's `cartographer.dll`, installer run with no offset overrides (OutdoorKurast is level 221, offset 6000,3660).

## Map

- Slot-only rows: the Frostbitten Fields map row switched to Tileset Kurast (only m01 to m05 are map items) with the Boss `Shattered Soul`, plus a mapevents row (`Burbs Horde`, 6 zombies) at EventChance 100. Opened from Harrogath with `cartographer open`, then the red portal; the arrival is in a Kurast hut street.
- `outdoor slot level 221: built as planned (74 stamps, 0 void cells; grid 120 cells: 74 pieces, 120 covered, 0 void, 0 neither), 120 rooms, plan bdfc9ce11c9d9409`
- `outdoor arrival: room 42 of 120 (at 6016,3708) marked as the level entry, in the entrance cell (6016,3708); 0 other room(s) lost the mark`
- `map Boss 'Shattered Soul': spawned in the room (30320,18380) 40 x 40 (as a superunique)`
- `event 'Burbs Horde' (Horde): spawned 6 of 6 in the event room (30120,18380) 40 x 40 (0 as a superunique, 0 refused by the game)`
- d2rloader.log: 4 startup assertions (before the map opened); no `LvlTbls.cpp:817` or warpDefs assert after the map opened. One later assert, `SkilPal.cpp:675`, comes from a skill parameter and is not a level assert.

## Not checked

Arenas (the unit tests cover them), the legacy view, a walk of the whole ring and map sizes other than Medium.

## Note

The default offset is (6000,3660), below Wilderness (3530, 112 tall).
