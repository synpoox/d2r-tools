## 3. Phase 1 parts: the map item and the red portal (`plugins/cartographer`, issue #20)

**Date:** 2026-11-11 (validated 2026-11-12). **Status:** validated in game on build 93847, image stamp `0x6ab3782c`
(D2R 3.3.0, D2RLoader 1.3.1-beta, test slot with the D2RToolsTest mod and a test save, Normal difficulty): all thirteen
parts found by signature at load, the three hooks installed, the item opened its own map's portal, the map loaded and
returned, a second open was refused, the Hell gate refused, the console `open` worked, and the tick's unit id matched
`LocalPlayerReady.playerId`. A second pass on 2026-11-12 (Hell game, level-1 then level-25 Barbarian) settled more; the
per-check levels are in "Game checks of 2026-11-12" at the end of this section. Still not validated in game: the free-spot
refusal (tried, not reproduced) and the TCP/IP client refusal (what would settle each is in that table); the spell-16 use
with no matching check is not reachable through its one direct caller and stays unit tested only.

Cartographer finds every part below by byte signature at load. A part needs exactly one match in the game's executable
sections; otherwise it stays off, the reason goes to the plugin log and `cartographer check`. The two item hooks are
written at load, and only then, whenever their own two parts resolve; until every required part is on they pass straight
through to the game (fail open: the item does what D2R does with an unknown spell). The portal is live only when all
required parts are on, and `cartographer check` can turn it on later by re-finding the called parts. The use hook goes
in first and acts only while the portal is live, so no portal can open ungated. A signature for any part can be supplied without a rebuild in `signatures.txt` (columns `Part`, `Signature`),
beside `maps.txt`; it replaces the built-in one.

Build 93847, image stamp `0x6ab3782c`. The RVAs are evidence from pd2-resurrected `plugin/core-maps/map_open.cpp` and
`plugin/common/pd2_engine.h` (UnitStat's from d2rAI-toolkit `rvas_93847.h`); the plugin finds the parts by signature and
does not use them. The signatures are in `plugins/cartographer/src/core/part_signatures.h`.

| Field | Record (issue #39) |
|---|---|
| Date | 2026-11-11 |
| Consumer | Cartographer portal (`src/engine/portal.cpp`) |
| Module | `D2R.exe` |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, under D2RLoader 1.3.1; memory-layout image of the running D2RLoader process (d2rAI-toolkit `tools/dump_image.py`). SHA-256 not recorded: the image is read from process memory, so it is not reproducible byte for byte (D2RLoader writes runtime pointers into it, such as the `UnitStat` thunk slot); the stamp and the checker output identify it |
| Location | Function entry RVAs in the table; every signature starts at the entry (offset 0), so the match address is the function |
| Guard | The whole signature compared under its mask at the resolved address; for a part that is only called, a unique match at the entry is the whole guard. For the two hooked parts the guard D2RLoader re-checks is the signature's own first bytes (15 and 14, no `??`), not bytes copied from the live entry at load. Max patch length (hooked parts only): the bytes from the entry before the first relative branch, call or RIP-relative instruction, so a patch must not split or overwrite one; both hooks copy fewer (15 of 44, 14 of 38). Called-only parts show `n/a (called)`: they are never patched, and the value would only invite it. The column reads signature length / max patch, as in section 1a |
| Contract | Roles and signatures in the table. Wildcards (`??`) cover relative branch and call targets, RIP-relative displacements, struct-table offsets (`ItemRecord`: the table count offset `+0x15a8`) and the thunk's jump slot (`UnitStat`). One exception: `GameCounter` keeps one call displacement unmasked (below) |
| Evidence | Disassembly of each entry (Capstone, over the dump). Each signature is the shortest whole-instruction prefix of at least 24 bytes that matches once, with masked bytes as wildcards, across all executable sections (`.text`, `.maho`, `.koishi`, `.btext`; each section's VirtualSize, as the plugin scans). Roles checked from the code: `OpenRedPortal` calls `UnitRoom`, a room check, `GameCounter` on `game + 0x1aac8` (read, and incremented after a successful opening), `UnitCoords` and `FindFreeSpot` (`3, 0x400, 5, 100` as recorded), a create call with object 60, and `ClientMessage(player, 0x15, player)` on a refusal; `UnitClassOf` returns `[unit + 4]`; `RoomLevelId` follows `[room + 0x18]` into a function reading `[[level + 0x90] + 0x1f8]`; `ItemRecord` is a bounds-checked `base + row * 0x1c0` over a vector at table `+0x15a0` |
| Validation | Static, then in game: `plugins/cartographer/tools/check_portal_signatures.py` on the 93847 image (with `--stamp 0x6ab3782c`), 13 of 13 (the twelve plus `PlayerTick`) unique and resolving to the recorded RVAs, and `GameCounter` confirmed as the target of two calls in `OpenRedPortal`; unit tests parse every signature and check each is at least 24 bytes (uniqueness is the checker's job). Then run in game on build 93847 (see the status above); not checked on Steam build 94995 |
| Failure mode | Fail open: a signature with no match or more than one match, or a guard mismatch, logs which part and leaves the portal off |
| Coexistence | The dump is of a D2RLoader process, so detours already in place show in it. None of the hooked or called entries starts with `E9`; `UnitStat` starts with D2RLoader's own `FF 25` thunk (below). Another plugin detouring a part later fails that part's guard |

| Part | Role | RVA | Signature (offset 0 = function entry) | Offset | Guard / max patch bytes | Required |
|---|---|---|---|---|---|---|
| ItemSpellCheck | Hooked. `(game, player, item, target) -> u64`; item spell 16 is the map | `0x5817f0` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 0 | 30 / 44 (hook copies 15) | yes |
| ItemSpellUse | Hooked. `(game, player, item, target)`; runs the opening | `0x581680` | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 48 8B EA 48 8B F1` | 0 | 24 / 38 (hook copies 14) | yes |
| ItemRecord | Called. `(dataFlag, row) -> record`; record +0x80 code, +0x94 pSpell | `0x314110` | `40 57 48 83 EC 30 8B FA E8 ?? ?? ?? ?? 3B B8 ?? ?? ?? ?? 72 ?? 33 C0 48 83 C4 30` | 0 | 27 / n/a (called) | yes |
| UnitClassOf | Called. `(unit, file, line) -> row` | `0x349860` | `48 83 EC 28 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC B8 FF FF FF FF 48 83 C4 28 C3 8B 41 04` | 0 | 41 / n/a (called) | yes |
| UnitRoom | Called. `(unit) -> room` | `0x34b440` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9 74 ?? 48 83 C4 20` | 0 | 63 / n/a (called) | yes |
| RoomLevelId | Called. `(room) -> level id` | `0x2efc10` | `48 85 C9 75 ?? 33 C0 C3 48 8B 49 18 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 85 C9` | 0 | 35 / n/a (called) | yes |
| ClientMessage | Called. `(player, 0x15, player)`: "can't do that" | `0x491960` | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 49 8B F8 0F B7 F2 48 8B D9` | 0 | 24 / n/a (called) | yes |
| OpenRedPortal | Called. `(game, player, key, level, max)`: red portal 60 by the player; counts openings per key and refuses at `max` (the one-map-per-game rule) | `0x5da800` | `48 89 5C 24 08 48 89 6C 24 10 44 89 44 24 18 56 57 41 56 48 83 EC 70 48 8B E9` | 0 | 26 / n/a (called) | yes |
| GameCounter | Called. `(game + 0x1aac8, &key) -> int*`; key `'MAPS'` (`0x4d415053`) | `0x5d88b0` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 41 08 4C 8B F2 48 8B D9 4D 85 C0 75 ?? 48 8B 11 E8 A4 3F 00 00` | 0 | 44 / n/a (called) | yes |
| UnitStat | Called. `(unit, stat, layer) -> int`; stat 12 is the level | `0x2f5020` (d2rAI-toolkit `rvas_93847.h`; pd2-resurrected: a jump into D2RCore) | `FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA` | 0 | 26 / n/a (called) | yes |
| UnitCoords | Called. `(unit, int xy[2])` | `0x34a040` | `48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 85 FF 75 ?? 48 8D 4C 24 30 40 88 7C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9` | 0 | 89 / n/a (called) | no (spot check skipped) |
| FindFreeSpot | Called. `(game, room, xy, 3, 0x400, &found, 5, 100)` | `0x517f90` | `48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 90 00 00 00 48 8B 05 ?? ?? ?? ??` | 0 | 30 / n/a (called) | no (spot check skipped) |
| PlayerTick | Hooked (issue #40). The player mana step `(game, player) -> al`; the server runs it once per frame for every live player, so the console command gets a game and a player there. Optional: the map item does not need it | `0x42e350` | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA 4C 8B F9 48 85 D2 0F 84 ?? ?? ?? ?? 45 33 C0 48 8B CF 41 8D 50 08 E8 ?? ?? ?? ?? 48 8B CF 8B F0 E8 ?? ?? ?? ?? 33 DB 48 8B CF 8B E8 8D 53 55 E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ??` (81 bytes, `??` on every rel32; hook cuts the first 14: five whole instructions, no relative operand) | 0 | 81 / 27 (hook copies 14; the first rel32 is the `0F 84` at 27) | no (console command only) |

PlayerTick evidence (static, read-only, against `D2RLoader_6ab3782c_dump.exe`, a memory-layout dump made by
d2rAI-toolkit `tools/dump_image.py`): the 81-byte signature has 1 match (RVA `0x42e350`; its first 14 bytes alone match 15
places, so the whole signature is used). It runs past the prologue into the body (null check of the player, the
`8D 53 55` = state 0x55 test), so another function cannot become the unique match by register allocation alone.
Reproduce: read the dump file, convert the signature to a regex (`??` = `.`, with `re.S`) and `finditer` over all bytes:
one match at `0x42e350`. The call at `0x42e74d` (`E8 FE FB FF FF`) resolves to `0x42e350` too. `rdx` is the player and `rcx`
the game (saved to `rdi`/`r15` at entry). Its only caller is the player regen tick `0x42e600` (better-auras.md section 4),
which reaches it on every path for a live player (`0x42e74d`). `0x42e600` itself is hooked by Better Auras, and a second
hook on the same entry is refused, so the tick part uses `0x42e350` instead. Contract: the hook calls the original
first, then serves a posted console request when the unit is a player (`+0` == 0) whose unit id (`+8`) equals the
`playerId` of the SDK's `LocalPlayerReady` event, using the SDK event as the source of "which player is local". That
`playerId` is the unit id is an assumption (not documented by the SDK): if it is wrong, the request is dropped after 10 s
with a log line naming both ids; the hook also logs the first player unit id it sees next to `LocalPlayerReady.playerId`.
`cartographer open` refuses (no arming) when the part is off, and in a game with no server tick (a TCP/IP client).
The portal opens from inside the unit timer event (`0x42e600`), not the item-use handler: needs the in-game re-entrancy
checks in the plugin README. In game the console `open` worked in a
Normal-difficulty game (section 3, Game checks); the TCP/IP client refusal was not exercised.

Evidence level of every row: static, dump (not game). Eleven of the twelve RVAs are `.pdata` function entries in this build; `RoomLevelId` (`0x2efc10`) is a frameless leaf with no `.pdata` entry (the nearest `.pdata` entry before it starts at `0x2efb70`, a different function). The plugin checks `.pdata` only for hooked parts overridden in `signatures.txt`, so `RoomLevelId` needs only a unique match. None looks wrong. Findings per part:

- **ItemSpellCheck, ItemSpellUse:** the 15 and 14 entry bytes recorded earlier were not unique (3140 and 11 matches in the
  executable sections), so with them both parts would have stayed off. Extended to 30 and 24 bytes, each unique; the hook
  sizes are unchanged and inside the max patch length.
- **UnitStat (load timing):** the parts are looked up once at plugin load, so the thunk must already be in place then; the dump
  shows it only in a running process. If it is not, the part and the portal stay off until `cartographer check` looks again
  (it re-scans all called parts with the load-time overrides and turns the portal on if every required part is found; it never
  writes code: the two item hooks are installed at load when their own parts resolve, and pass through to the game until the
  portal is on, so a missing hooked part needs a restart). Put "UnitStat found at load" on the in-game checklist.
- **UnitStat:** `0x2f5020` is a D2RLoader thunk, `jmp [rip+x]` plus four NOPs, into D2RCore (pd2-resurrected found the same);
  the bytes after it are the dead original body, and calling the thunk is the way to read a stat. The signature covers the
  thunk and the start of the dead body, so it exists only while D2RLoader's thunk is in place; without it the part is off.
  The jump slot is masked. Do not patch it.
- **RoomLevelId:** a 17-byte wrapper (`[room + 0x18]`, then a jump). Four wrappers have the same masked bytes (`0x2efc10`,
  `0x2efc30`, `0x2efd00`, `0x2f0720`; the next one, `0x2efc30`, is also a twin and jumps to `0x360fd0`). `0x2efc10` is
  unique only because it is the one twin followed by padding and another twin, so the signature takes in the 15 bytes of padding
  and the next function's first three bytes. Its uniqueness rests on code layout: a linker reordering, a change to the
  neighbour or to the padding breaks it (fail open). A sturdier way for later is to find it through a caller or its jump
  target `0x360fc0` and follow the rel32.
- **GameCounter:** an `unordered_map<int,int>` lookup, and three copies of it have identical bytes apart from their call
  displacements (`0x5d88b0`, `0xd8590`, `0x8723b0`). No fully masked signature tells them apart, so the first call
  displacement (`E8 A4 3F 00 00`, to the map's rehash helper) is kept unmasked. This is the most build-sensitive signature
  here: a game update that moves the helper turns the part off (fail open), and the signature is then re-derived. `tools/check_portal_signatures.py` also checks that `OpenRedPortal` contains
  two `E8` calls (at `+0x5e` and `+0x13e`) whose target is the matched `GameCounter`, which ties the right copy to a unique part;
  a later step could derive `GameCounter` from there.
- **UnitRoom, UnitCoords:** share an assert-check prologue with many accessors, so the shortest unique prefix is long
  (63 and 89 bytes); `UnitCoords` is optional.

Game layout used: `game + 0x104` difficulty (u8), `game + 0x106` data flag (u8), `game + 0x1aac8` counters; item
record `+0x80` code (4 letters), `+0x94` pSpell. Item spell 16 is free in vanilla D2R. The town is Harrogath, level 109.

Failure mode for every part: fail open, log the reason, leave the portal off. Coexistence: another plugin hooking the
same dispatchers is not handled; D2RLoader refuses a second hook on the same bytes and the part then reports that.

Re-check on a new build: write a memory-layout image with d2rAI-toolkit's `tools/dump_image.py` (game running through
D2RLoader), then `python -I plugins/cartographer/tools/check_portal_signatures.py <image> --stamp <stamp>`. The packed
`D2R.exe` on disk cannot be checked (encrypted code).

### Game checks of 2026-11-12

Hell game on slot 11 with the D2RToolsTest mod, test save `cartos` (progression raised so Hell is offered), vanilla
`levels.txt`, `lvlprest.txt` and `misc.txt` plus the installer's "Cartographer Preset" level 138 and three `misc.txt`
rows with `pSpell` 16 (`t20`, `t21`, `t22`); maps `Shorelines` (Hell, level 1) and `Gated` (Hell, level 20).
Levels: live = seen in the running game; static = read from the dump only; tried = run in game without reaching the result; not done = not tried.

| Check | Level | Result |
|---|---|---|
| Open a map in Hell | live | `t21` in Harrogath opened `Gated` through level 138; a red portal appeared, the scroll was consumed, the portal led to level 138 and back. |
| Level gate refuses | live | Level 1 using `Gated` (needs 20): log "Gated needs character level 20 (you are 1)", item kept. On screen the player sees only "cartos says: I can't use this yet."; the Cartographer wording is in the log and console. |
| Level gate allows | live | After `cheat level 25` the same item opened the portal. This exercises `UnitStat` (`0x2f5020`) on its success path only. |
| Free-spot gate | tried, refusal not reproduced | `cheat mspawn zombie1,100` around the player did not stop the spot search; the portal opened (kept as evidence). A refusal was never seen, so `FindFreeSpot` (`0x517f90`) has only its success path exercised. Static reading is in "Free-spot search" below: the mask tests a collision bit, not a monster crowd, and the search runs out to about 99 subtiles, so a refusal needs every tested window in that area blocked. What would settle it: a debug build that forces `found == nullptr` once, or a spot where collision bit `0x400` is set on every window out to that radius (none known). Until then the refusal is unit tested only (`CheckGates` -> `NoSpot`). |
| Spell-16 item no map names | live | `t22` (spell 16, in no `maps.txt` row): no log line, no message, not consumed, the same as with Cartographer unloaded. |
| Spell-16 use with no matching check | not done | Static: the only direct caller (`0x4f591e`) runs the use only after `ItemSpellCheck` returned nonzero (see "Callers of ItemSpellUse" below), so the case is not reachable through it. It is defensive code, unit tested only, with no in-game test planned; only an indirect caller (none searched) could reach it. |
| Leave game with a portal open, next game | live | Save and Exit from level 138 with the portal open: clean exit, no crash. In the next game `Shorelines` opened (the per-game `'MAPS'` counter was fresh) and the tick logged its first player unit again. Whether that open used the item or `cartographer open` was not recorded, so the console path after a game change is not shown. |
| TCP/IP client refused | not done | Needs a second game instance joined as a TCP/IP client, then `cartographer open <Name>`; not run. The refusal is in the code (no tick seen) and the unit tests. |
| Town portal scroll on the map level | live | `tsc` used in level 138 did nothing: not consumed, no portal, no log line. With Cartographer unloaded the same scroll did the same, so this is vanilla behaviour with a preset-level slot, not Cartographer. |
| Better Auras alongside | live | Both loaded in one game (Better Auras' log: "other plugins: cartographer.dll"); `Shorelines` opened; no new crash report. Cartographer hooks `0x42e350`, Better Auras `0x42e600`, so the two do not meet. |

Gate evidence, as it stands: the order `NotInTown`, `Difficulty`, `CharLevel`, `AlreadyOpen`, `NoSpot` is the code's; in game
the Normal-difficulty refusal, the Hell allow, the level refusal and the level allow were seen in this pass, and the
AlreadyOpen refusal (a second open) and the NotInTown refusal (console `open` outside town) were seen live in #70. `UnitStat`
and `FindFreeSpot` were run only on their success path (a level read, a spot found); neither has been shown to refuse live.
Only `NoSpot` has never been seen refusing.

Free-spot search (static, read-only, `python -I plugins/cartographer/tools/dump_dis.py C:\ghidra-work\D2RLoader_6ab3782c_dump.exe
--stamp 0x6ab3782c dis <rva>`; dump, not game): `FindFreeSpot` (`0x517f90`) takes the `0x400` as its fifth argument and
passes it, with a 7 by 7 window (`3 * 2 + 1`), to `0x364b20` (its seventh argument, `5`, is never read), then `0x365f80`, then `0x3666c0`. That last function ANDs
each 16-bit entry of the room's collision grid (`[room_coll + 0x20]`, one `u16` per subtile) with the mask and ORs the
results; a nonzero result means the window is blocked. So `0x400` is bit `0x400` of the per-subtile collision word, not a
unit count: zombies would matter only if their presence sets that bit, which the 100-zombie run suggests it does not.
The search spirals outward from the player in rings (candidates step by 2 subtiles; windows whose corner is outside the room are skipped) up to the eighth argument (100), moving into neighbouring rooms, and sets
`found` to null only when every tested window up to that radius is blocked or no room exists. What bit `0x400` means (its engine
name) is not established by this dump. The exact in-game check that would reproduce a refusal: stand where bit `0x400` is
set on every 7 by 7 window out to about 99 subtiles (about 20 tiles) of the player. Harrogath has no such spot that we know
of, so a forced `found == nullptr` in a debug build is the practical route. Not run.

Callers of ItemSpellUse (static, dump; `0x581680` and `0x5817f0` found by scanning every `E8` and `E9` in `.text`): each has
exactly one direct caller, both inside one large function (the item-use handler; `0x4f45eb` is its shared tail):
`ItemSpellCheck` at `0x4f52c6`, `ItemSpellUse` at `0x4f591e`. The use block `0x4f590f` is reached only from `0x4f580d` and
`0x4f5816` (both `jne`), in block `0x4f57f4`. That block is reached only from `0x4f55f9` and `0x4f562c`, both in block
`0x4f55f0`, which is reached only from `0x4f5470`, in block `0x4f5438`, which is reached only from `0x4f52cd`: the
`test al, al; jne` right after `call 0x5817f0` at `0x4f52c6`. So the use runs only after `ItemSpellCheck` returned nonzero.
The two jumps at `0x4f580d` (`[record + 4]` of the row from `0x300ae0(dataFlag, ...)` compared with `r13d`) and `0x4f5816`
(`r14b` nonzero) choose between the use and the other branch (`0x4f581c`); they do not skip the check. Through this caller
the "use with no matching check" case is therefore not reachable (static); it is defensive code in Cartographer's hook, and
it stays unit tested only. Indirect callers (vtable or function pointer) were not searched.

Portal label: the red portal's tooltip read "Missing String" when the player stood in Harrogath and "Harrogath" inside level
138. Likely cause: the installer sets level 138's `LevelName` to "Cartographer Preset" and adds no string for that key, so every
installer user would see it; not verified in game. Tracked in #87.

What remains: the free-spot refusal and the TCP/IP client refusal (each row above says what would settle it); the spell-16
use with no matching check stays unit tested only. The hook entries and `UnitStat`'s thunk are checked in the dump and by the game running through them.
