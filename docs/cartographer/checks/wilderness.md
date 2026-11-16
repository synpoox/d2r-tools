# Wilderness tileset game check (issue #506)

Slot 4, Hell, level 99 test character, pd2r from `origin/main`, this branch's `cartographer.dll`.

## Map

- Slot-only row: the Frostbitten Fields map row was switched to Tileset Wilderness (only m01 to m05 are map items). Opened with `cheat goto 109`, `cartographer open`, then the red portal.
- Level 219 at (6000,4340), 96 x 80 tiles, seed 6802975708997440084.
- Log: `outdoor slot level 219: built as planned (72 stamps, 0 void cells; grid 120 cells: 72 pieces, 120 covered, 0 void, 0 neither), 120 rooms, plan 0395d6381606cd1b`
- Log: `entrance cell at tile (6016,4356) 16 x 16`, entry room 103 marked, 0 other rooms lost the mark.
- Looked at the north-west and south-east ring edges in HD: stone wall and trees close the edge, no black cells, no gaps.
- Legacy view (`g`) shows the HD pieces as red noise, as for the other HD-only tilesets; not a planner fault.

## Arenas

Both slot-only arenas (one per fill set) built as planned with 0 void cells and a closed ring. The NW corner of the swamp arena has a gate-like wall opening that comes from the piece design.

## Note

The default offset moved to (6000,4340) because (6000,4400) overlapped pd2r level 210 (Rathma levels sit at x 6000..6240, y 4500..5148).
