## 34. Guaranteeing the map Boss (issue #445)

Build 93847 (stamp `0x6ab3782c`), D2RLoader 1.3.1-beta. Static analysis of the dump with `plugins/cartographer/tools/dump_dis.py`. Evidence labels: **static**, **inference**, **needs game**. Builds on [section 9](09-map-events.md#9-map-events-entry-message-decision-boss-and-horde-issue-92) ("Plan rooms and the map Boss spot"). No new signature: the one new use is a flag on a call that already exists.

### 1. Answer

**A room that has no walkable subtile cannot be made to take the Boss at a collision-checked spot; the game gives no way to force it onto floor that is not there.** So the guarantee is a ladder, each rung logging one line (`boss path:` on success):

1. the plan's boss room, centre first (as before);
2. the other rooms of the plan's boss cell, in order;
3. every other room of the level, each the moment the game populates it (the entry room and the rooms farthest from it are listed first, but the game decides when a room populates);
4. the same room again, with the collision test skipped (flag `0x80`, below), **as the real Boss** (superunique, section 5). A room is only tried while it populates: the game frees a server room's record once the player walks on, and a game check crashed reading a pointer kept from earlier.
 The game populates a room once, so a cell room that has already populated is skipped, and when a refusal leaves no room that can still populate, rung 4 runs at once in the room populating now (even a cell room). The picker logs a warning when it is left waiting with nothing pending.

5. when the forced try fails too and no room is left to populate: the player tick (`EventsPlayerTick`, as the Ambush's waves in `event_6b.cpp`) tries once in the room the player stands in, flag `0x80`, as the real Boss (section 5). If even that is refused the log has an Error; there is never a plain monster.

Rungs 1 to 3 are the existing fallbacks plus the wider set of rooms; rungs 4 and 5 are the ones that always have somewhere to put it. The picker and the loop are pure and unit tested (`BossRoomPicker`, `roomsFarthestFirst`, `driveBoss` in `plugins/cartographer/src/eventplan/`).

### 2. What the engine does

| Fact | Evidence | Level |
|---|---|---|
| `SpawnMonster` `0x492cf0` fills a request and calls the spawn core `0x493cc0`; its p7 (`0xa`) is the free-spot radius. The core walks rings 3, 6, ... out from the start inside the room rect | section 9 | static |
| Per candidate subtile the core calls the collision test `0x364d50` (`0x4941e5`). Non-zero means blocked: `je 0x494210` accepts a free spot; for a blocked one `test byte [req+0x30], 0x80` / `jne 0x494210` **accepts it anyway**, otherwise the ring walk goes on (`0x4941ef`..`0x494208`). So request flag `0x80` = "take the spot even if the collision test refuses it" | `dump_dis.py ... dis 0x4941c0 40` | static |
| `SpawnMonster`'s last argument is the request's flags word (`SpawnFlags`, 0 in all other uses). `SpawnFlagNoCollision = 0x80` is passed only by the last rung | `0x492d27` / `0x492d3b` (the flags word is stored into the request) | static |
| The superunique spawn `0x499ba0` can return null before it spawns: the superunique row lookup (`0x397220`) gives null or a negative id (`0x499c38`..`0x499c4d`); the once-per-game bitmap (`game + 0x1d8`, tested at `0x499c76`, with a global at `0x260cded`) blocks a second spawn of the same row (`0x499c87`); the inner `0x492cf0` returns null (`0x499ccd`). (Issue #449 read it as taking no flags; section 5: it does.) | `dump_dis.py ... dis 0x499ba0 110` | static |
| The free-spot finders `0x362f90` / `0x364e90` and `0x402f50` were not used: they are not clipped to the room rect and `SpawnMonster` would search again from the result | section 9 | static |

### 3. Why a void room cannot be forced

A room the plan marks as the boss room can still be an engine room with no floor (a quadrant with only walls, or an outdoor corner clearing). The ring search only ever visits subtiles of that room's rect, and `0x364d50` refuses every one of them. Flag `0x80` would take one anyway, but that puts the monster inside rock where nobody can reach it, so the intended room is not "forced"; the Boss is moved to another room instead (rungs 2 to 4), and only the last rung uses the flag, in the room populating now. Choosing the sub-room by "has floor" needs the same collision read the spawn already does, so the refusal itself is the signal.

### 5. The real Boss on the last rung (issue #463)

Build 93847, static, `dump_dis.py` on `0x499ba0`, `0x49e980`, `0x492cf0`. The superunique spawn **does** take a flags word: its sixth argument (`[rbp + 0x118]`, after the row at `[rbp + 0x110]`, read as 16 bits at `0x499bdd`) is stored at `[rsp + 0x42]` and passed at `0x499d06`..`0x499d30` as the seventh argument (`[rsp + 0x38]`) of the row-resolving spawn `0x49e980`. That function ors `0x140` into the word (`0x49ea05`..`0x49ea10`) and passes it to `SpawnMonster` `0x492cf0` (`0x49ea33`..`0x49ea64`; or `0x492d60` with a chosen position), which stores it as the request's flags word (`0x492d27`/`0x492d3b`), the word the spawn core tests with `test byte [req+0x30], 0x80` (section 2). So flag `0x80` reaches the core through the superunique path, and the row's name, mods, minion group and drops are kept. The plugin's `SpawnSuperFn` already had the sixth argument and always passed 0. No new address: `sig::SpawnSuperUnique` is the existing signature, found by bytes at runtime.

Route chosen: pass `0x80` to the superunique spawn (`ep::bossCall`), not "give a forced monster the superunique row afterwards". The row is applied by the code that builds any superunique (name, mods, minions, TC), so nothing is re-implemented and the once-per-game bit is set as in a normal spawn.

Once-per-game bit (`game + 0x1d8`, tested at `0x499c6d`..`0x499c87`): a second spawn of the same row in one game returns null, and a plain monster is not an acceptable stand-in, so `SpawnMapBoss` clears the bit of the map Boss's row before it spawns it (the bit table at `0x1d996d0` holds `1 << n`, bytes read in the dump). The base-class fallback is removed for a Boss that has a superuniques row; a Boss that is a plain monstats id is that monster on every rung, as before. Events (Boss, Horde, Ambush) keep their own behaviour.

No-room path: `OnPopulated` sets `bossStranded` when the picker is out of rooms; `EventsPlayerTick` then calls `PlaceStrandedBoss` once with the room the player stands in (a live room on the map's level, so its record is not stale), flag `0x80`.

Needs game: that flag `0x80` takes effect on the superunique path (name shown, minions, drops at a spot the collision test refuses).

### 4. Not verified

The ladder and flag `0x80` ran in game in the checks of the PR for #445 (results in that PR); how often each rung is reached in play is not known. The test knob `CARTOGRAPHER_TEST_BOSS_REFUSE=N` (environment variable, a game check only) refuses the first N ordinary tries so rungs 2 to 4 can be reached; it does nothing when unset.
