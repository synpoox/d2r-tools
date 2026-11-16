## 38. A red portal beside a unit, and a second map in a game (issue #485)

Build 93847 (D2R 3.3, image stamp 0x6ab3782c). Evidence: **static** (bytes and decompile of the memory dump, Part 0 of #485). Game check: **Pending** (issue #500; not run yet).

### 1. What OpenRedPortal does

`OpenRedPortal` (`0x5da800`) opens only in town: it checks `IsTownRoom` (`0x2f0750`, `IsTownLevel` `0x327850`: levels 1, 40, 75, 103, 109) before anything else, and refuses with `ClientMessage(unit, 0x15, unit)`. After the town and counter checks it finds a spot (`FindFreeSpot`, `0x517f90`, with `UnitCoords` `0x34a040` and `UnitRoom` `0x34b440`), creates the portal (`CreatePortal`, `0x432ce0`, found as the derived call at `OpenRedPortal + 0xf8`) and sets its destination (`SetPortalDest`, `0x34e9d0`, derived at `+0x12a`).

- `CreatePortal(game, unit, room, x, y, level, &object, 0x3c, 0, 1)`: 10 arguments. It refuses a destination in another act. It runs its own town message only when the 10th argument is 0. It creates object 0x3c, then calls `0x435dd0` for the return portal.
- `0x435dd0` (the return portal) never reads its `unit` argument. It builds the destination level on the server inside the call (`0x2ef8b0`), so the slot job must be set first. The return portal leads to the level the red portal stands in: the source, not town.

### 2. Derived calls, not fixed RVAs

`CreatePortal` and `SetPortalDest` are not stored as RVAs. `plugins/cartographer/src/engine/portal.cpp` `CallTarget` reads the `E8 rel32` at `OpenRedPortal + 0xf8` and `+ 0x12a`, takes the target, and accepts it only if its first bytes equal the recorded entry bytes (19 and 16 bytes). Otherwise both are off and the extra open refuses.

### 3. Who may stand beside it

`OpenRedPortal` passes its player to `ClientMessage` on a refusal, so a monster must not reach it. The extra open does the calls behind it itself, with no town check: `UnitRoom(nearUnit)`, `UnitCoords(nearUnit)`, `FindFreeSpot`, `CreatePortal(game, player, found, x, y, dest, &obj, 0x3c, 0, 1)`, `SetPortalDest(obj, dest)`. `UnitRoom` and `UnitCoords` take any unit type.

### 4. The counter

The extra open never calls `OpenRedPortal`, so no counter is checked on that path. `'MAPS'` (`0x4d415053`) is untouched, so the item and console gates (`openings > 0`) are unchanged. The extra opens bump `'MAPX'` (`0x4d415058`) by hand, mirroring `OpenRedPortal`'s own `inc`. `GameCounter` (`0x5d88b0`) is `unordered_map::operator[]`: a missing key is inserted as 0 and it never returns null. There is no `max`: the slots and the one-destination-per-source rule limit the count.

### 5. Game check

Pending: the game check of `cartographer open-extra` (see the PR for #485).
