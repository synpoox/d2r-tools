## 42. The Barracks builder case (issue #272)

Build 93847 (D2R 3.3, image stamp 0x6ab3782c). Evidence: **static** (disassembly of the memory dump with `plugins/cartographer/tools/dump_dis.py`); in-game behaviour is **needs game**.

The maze builder `0x3e69e0` dispatches on `LevelType - 3` through the jump table at `0x3e76d8` (entries are the case RVAs). LevelType 7 (Act 1 Barracks) goes to `0x3e6c24`: `mov edx,2; call 0x3e9820` (the ring grower), `call 0x3e96d0` (the walk), `jmp 0x3e75bc`. There is no special-room call (`0x3eb850`), so the case places no Forge or Next piece and the steered walk needs no special handling: `callsRingGrower` is true, `ownGrower` false, no Prev family.

The common tail `0x3e75bc` branches on the level id: `0x1c` (28, vanilla Barracks) goes to `0x3ec2c0`, `0x6b` to `0x3ec820`, anything else to the generic `0x327880`. A slot level never has id 28, so it takes the generic path.

Data: LvlMaze level 28 is `SizeX 10, SizeY 14, Merge 500`; LvlPrest 167 is Court Connect (mask 0), 168..182 the 15 mask pieces. Barracks is the first non-square room, so the planner, arrival and event-room matching carry a separate room height (`Tileset::roomTilesY`).

Game check (slot 10, 2026-11-17, Hell): the built rooms give a box equal to the plan's 10 x 14 cells on Small, Medium, Large and ExtraLarge ("built as planned", 0 refits), the arrival, boss and event rooms are found by the plan cell, and no filler ring was needed.
