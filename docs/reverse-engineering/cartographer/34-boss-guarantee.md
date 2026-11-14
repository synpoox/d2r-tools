## 34. Guaranteeing the map Boss (issue #445)

Build 93847 (stamp `0x6ab3782c`), D2RLoader 1.3.1-beta. Static analysis of the dump with `plugins/cartographer/tools/dump_dis.py`. Evidence labels: **static**, **inference**, **needs game**. Builds on [section 9](09-map-events.md#9-map-events-entry-message-decision-boss-and-horde-issue-92) ("Plan rooms and the map Boss spot"). No new signature: the one new use is a flag on a call that already exists.

### 1. Answer

**A room that has no walkable subtile cannot be made to take the Boss at a collision-checked spot; the game gives no way to force it onto floor that is not there.** So the guarantee is a ladder, each rung logging one line (`boss path:` on success):

1. the plan's boss room, centre first (as before);
2. the other rooms of the plan's boss cell, in order;
3. every other room of the level, each the moment the game populates it (the entry room and the rooms farthest from it are listed first, but the game decides when a room populates);
4. the same room again, with the collision test skipped (flag `0x80`, below), as a plain monster. A room is only tried while it populates: the game frees a server room's record once the player walks on, and a game check crashed reading a pointer kept from earlier.

Rungs 1 to 3 are the existing fallbacks plus the wider set of rooms; rung 4 is the only one that always has somewhere to put it. The picker and the loop are pure and unit tested (`BossRoomPicker`, `roomsFarthestFirst`, `driveBoss` in `plugins/cartographer/src/eventplan/`).

### 2. What the engine does

| Fact | Evidence | Level |
|---|---|---|
| `SpawnMonster` `0x492cf0` fills a request and calls the spawn core `0x493cc0`; its p7 (`0xa`) is the free-spot radius. The core walks rings 3, 6, ... out from the start inside the room rect | section 9 | static |
| Per candidate subtile the core calls the collision test `0x364d50` (`0x4941e5`). Non-zero means blocked: `je 0x494210` accepts a free spot; for a blocked one `test byte [req+0x30], 0x80` / `jne 0x494210` **accepts it anyway**, otherwise the ring walk goes on (`0x4941ef`..`0x494208`). So request flag `0x80` = "take the spot even if the collision test refuses it" | `dump_dis.py ... dis 0x4941c0 40` | static |
| `SpawnMonster`'s last argument is the request's flags word (`SpawnFlags`, 0 in all other uses). `SpawnFlagNoCollision = 0x80` is passed only by the last rung | `event_apply.cpp` | static |
| The superunique spawn `0x499ba0` can return null before it spawns: the superunique row lookup (`0x397220`) gives null or a negative id (`0x499c38`..`0x499c4d`); the once-per-game bitmap (`game + 0x1d8`, tested at `0x499c76`, with a global at `0x260cded`) blocks a second spawn of the same row (`0x499c87`); the inner `0x492cf0` returns null (`0x499ccd`). It takes **no flags**, so rung 4 spawns the Boss's base class row as a plain monster | `dump_dis.py ... dis 0x499ba0 110` | static |
| The free-spot finders `0x362f90` / `0x364e90` and `0x402f50` were not used: they are not clipped to the room rect and `SpawnMonster` would search again from the result | section 9 | static |

### 3. Why a void room cannot be forced

A room the plan marks as the boss room can still be an engine room with no floor (a quadrant with only walls, or an outdoor corner clearing). The ring search only ever visits subtiles of that room's rect, and `0x364d50` refuses every one of them. Flag `0x80` would take one anyway, but that puts the monster inside rock where nobody can reach it, so the intended room is not "forced"; the Boss is moved to another room instead (rungs 2 to 4), and only the last rung uses the flag, in the room the player is standing in. Choosing the sub-room by "has floor" needs the same collision read the spawn already does, so the refusal itself is the signal.

### 4. Not verified

Nothing here has run in a game before the in-game checks of the PR for #445. In particular: that flag `0x80` spawns a normal monster that takes the map's stats, that rung 4's spot is reachable on every map type, and how often each rung is reached in play. The test knob `CARTOGRAPHER_TEST_BOSS_REFUSE=N` (environment variable, a game check only) refuses the first N ordinary tries so rungs 2 to 4 can be reached; it does nothing when unset.
