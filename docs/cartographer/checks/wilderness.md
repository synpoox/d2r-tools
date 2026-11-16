# Wilderness tileset game check (issue #506)

Slot 3, Hell, level 99 test character, pd2r from a fresh `origin/main` worktree, this branch's `cartographer.dll`, installer run with no offset overrides (levels 216 to 220; OutdoorWilderness is level 220).

## Map

- Slot-only rows: the Frostbitten Fields map row switched to Tileset Wilderness (only m01 to m05 are map items) with a Boss (`Moor Warden`) and a mapevents row (`Bog Horde`, 6 zombies, chance 100). Opened with `cheat goto 109`, `cartographer open`, then the red portal.
- Level 220 at (6000,3530), 96 x 80 tiles.
- `outdoor slot level 220: built as planned (72 stamps, 0 void cells; grid 120 cells: 72 pieces, 120 covered, 0 void, 0 neither), 120 rooms, plan e447a39309aeefbb`
- `outdoor arrival: room 37 of 120 (at 6064,3578) marked as the level entry, in the entrance cell (6064,3578); 0 other room(s) lost the mark`
- `map Boss 'Moor Warden': spawned in the room (30080,17730) 40 x 40 (as a superunique)`
- `event 'Bog Horde' (Horde): spawned 6 of 6 in the event room (30280,17730) 40 x 40 (0 as a superunique, 0 refused by the game)`
- d2rloader.log: 4 startup assertions (before the map opened, none from the slot levels); none after the map opened.
- Legacy view (`g`) shows the HD pieces as noise, as for Mesa and Desert; not a planner fault.

## Arenas

Both slot-only arenas (one per fill set), 3 x 3 cells, 40 x 40 tiles:

- `arena 'Wilderness Swamp Arena' (level 221)`: `built as planned (22 stamps, 0 void cells; grid 25 cells: 22 pieces, 25 covered, 0 void, 0 neither), 25 rooms, plan 2330c3910e209dfa (arena)`
- `arena 'Wilderness Tree Arena' (level 222)`: `built as planned (22 stamps, 0 void cells; grid 25 cells: 22 pieces, 25 covered, 0 void, 0 neither), 25 rooms, plan 89ed93b98514ef9b (arena)`

Other sizes are covered by the unit tests. Slot rows were restored byte for byte afterwards.

## Note

The default offset is (6000,3530): the band between Desert (3400) and Kurast (3660) clears the Rathma levels of pd2r (x 6000..6240, y 4500..5148).
