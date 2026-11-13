## 21. `ptWarp` and `bPiece` asserts in a Crypt map (issue #263)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. Both messages were seen in game 1 of the
crypt check (the Crypt difficulty check) and reproduced here in game, slot 7, Hell test save, single player, a Crypt map
(`Monsters` = `zombie1`, a Boss and a Treasure event, Chance 100). Statically read from the dump unless marked "in game".

### 1. `BC_VERIFY: ptWarp` (`D2Common\src\Drlg\RoomTile.cpp:747`): harmless, same cause as #254

- The verify is in `0x3f44b0` (called from the tile switch `0x3f32c0` at `0x3f3627` and from `0x3f4320`). It looks up `Warp[k]` through `0x3daad0` in the room's warp list. The slot
  row has `Warp0..7` = -1, so no node is found and the verify fails.
- This is the stairs tile of the entrance room ("Prev" piece, tile orientation 10 or 11) asking for an exit that the slot does not have, the same cause as
  [section 20](20-warpdefs-assert.md) (`lvlwarp` lookup). The tile gets no warp link, which is the intent.
- In game: logged at the moment the map opens, before the portal is entered, in every process started (4 of 4 here; the loader logs an assertion once per process).
  The map built and played normally. No change was made.

### 2. `BC_ASSERT: bPiece >= 0 && bPiece < (1 << nSize)` (`D2Game\src\Game\SCmd.cpp:3877`): caused by Cartographer, fixed

#### Where

- The assert is in the unit-info packet builder `0x47c570(ctx, unit)` (the packet a client gets when a monster comes into view). For each of the 16 components `i` it
  asserts that the unit's piece byte `[[unit+0x10]+8+i]` is below `1 << nSize`. `nSize` comes from `0x3cbea0(unit, i)`, the component's count from the class's
  `monstats2` row (byte at `+0x15+i`): `nSize` = 1 for a count of 0 to 2, else `bsr(count-1)+1`.
- Pieces are made in `0x543120` -> `0x5429d0(group, unit)`. A group of the level's region record that already holds preset members (16 bytes each at `group+4`, the
  number at `group+3`) is **copied to the unit without checking it** against the unit's class; otherwise each piece is `rand % count`, which is always in range.
- The group comes from `0x509190(regions, room, unit)` (region record array at `game+0x1d0`; the record's groups at `+0x14`, stride `0x34`, count at `+0x12`):
  the group of the unit's class is found among the first `count` groups; if the class is not there and `count < 13`, the game **appends** one in slot `count`
  (`0x5093e4`: writes the class word, `inc [record+0x12]`, calls `0x4195e0(group, ..., 3)`).
- `0x4195e0` makes the presets: it reads `edx = [group+3]` (members already there) and creates `min(3, (1 << nSize of the class) - edx)` more, from `group+4+16*edx`. The part read
  (the first 300 instructions, up to the piece loop) never clears `[group+3]` or the old presets (inference for the rest of the function; the in game result below fits it). A fresh record has zeros past its count, so that works in the vanilla game.

#### What Cartographer did

`Monsters` writes the record's counts (`+0x10`, `+0x11`, `+0x12`) to the listed n and the class, weight and members byte (`+3` = 0) of the first n groups only. The slots n..12 keep the
level's original (vanilla Crypt) groups, with their members and presets. The Treasure monster (`quillrat1`) is not a listed class, so the game appended its group in slot n, on top
of a stale old group with a non-zero members byte: nothing new was generated and the old group's presets were copied onto a `quillrat1`.

In game, with a probe on `0x3cbea0` (not kept): the first Treasure spawn logged `BAD unit id 14 class 63 comp 2 piece 2 count 0 (bits 1)` with pieces `1 1 2 1 0 0 0 0 1 0 ...` against
counts `0 1 0 0 ...` (a value of 1 or 2 where the class has no such component), and the loader logged `bPiece` in the same second, as the spawn code returned. The same
spawn in a game with the fix logged no `BAD` line and no `bPiece` assert (below).

#### Fix

`PlanRecord` (`plugins/cartographer/src/levelrow/levelrow.cpp`) also writes zero to the members byte (`+3`) of the group slots n..12. Only the members byte: lookups read the groups below the
count, and the class, weight and presets of a slot past the count are rewritten by the game when it appends. The ledger undoes the writes with the others. Unit test in
`plugins/cartographer/tests/levelrow_tests.cpp`.

#### Verification levels

| Level | Result |
|---|---|
| Unit tests (`tools\build.ps1 -Plugin cartographer -Test`) | the plan writes a zero members byte for every slot n..12 and only that; 18 of 18 test executables pass |
| Build | clean |
| Live in game (slot 7, Hell, `cartographer open CryptHell`, `cheat goto` to the event room) | before the fix: 1 of 1 game logged the `BAD` unit and the `bPiece` assert at the Treasure spawn. With the fix: 3 of 3 games (two with the probe, one with the final DLL) spawned the Treasure monster in the event room with no `BAD` line and no `bPiece` assert; the loader log of each held `ptWarp` only |

Not checked: that the Treasure monster fled or dropped its chest (not part of this issue); Normal and Nightmare (Crypt maps open only in Hell); a Crypt map with the `Monsters` list
at the vanilla group count (nothing stale); the same append path for an Ambush or Horde monster (the fix covers it: every unlisted class takes the same path, but it was not run);
TCP/IP.
