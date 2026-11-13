## 27. Map item cursor, in-game attempt (issue #307)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Slot 5, test character, `debug_mode` + `cheat god 1`.

### Result

The cause is still not established, and the fix is unverified in game. No code changed.

### What was tried

- Static: the client right-click use (`FUN_140161f20`, InventoryItemManager.cpp) only sends the use packet (0x26) through `FUN_1400ed1e0`; no cursor or identify
  prediction was found in it. The identify handler (`pSpell` 1, `0x57ec30` -> `FUN_14046e8c0`) is a server-side ItemMode change that probably puts the item on the cursor.
  A refused map item never reaches it (`ItemSpellCheck` returns 0 at `0x4f52c6`), so the cursor likely comes from client data (the `isc` base art or `spellicon`), not the server.
- In game: harness mouse hover highlights inventory items, but harness right-clicks
  (`game_ctl.py click fx fy right`) on a Town Portal scroll and on a map item neither used the item nor reached the guard (no portal, no log line, no cursor change).
  The harness cannot reach the client's right-click use handling, so the symptom was not reproduced and no fix could be checked.

### Next

Reproduce with a real mouse and capture which cursor shows; then compare a clone of `isc` with a different base art. Only then change code.

## Static follow-up: where the cursor starts (found 2026-11-12, static only, Ghidra project of the `0x6ab3782c` dump)

The note above guessed the server. It is the client, and it is driven by one `misc.txt` column.

### Result

The client starts the item (identify) cursor itself, on right-click, from the item row's `spellicon` column, before the server is asked. A map row copied from `isc`
has `spellicon` 0, so the cursor starts on every map right-click; the server then refuses a second map (item stays), and nothing ever clears the cursor.
Setting `spellicon` to -1 (as `tsc` and keys have) makes the client skip it. **The owner confirmed with a real mouse that `spellicon` -1 on the map rows in `misc.txt` fixes #307.**

### Evidence (RVAs in the memory-layout dump; call chain)

| Step | Address | What it shows |
|---|---|---|
| Item record layout | `0x315fd0` (misc column loader) | column table entry `{name "spellicon", type 6, offset 0x144}`; `pSpell` is `{type 2, offset 0x94}`. Record lookup `0x314110(dataFlag, row)` (record size `0x1c0`, array at manager `+0x15a0`, count at manager `+0x15a8`; null once `row` is past the count). Its first argument picks the data manager: `0x300a90` returns `*(0x2a9a580 + 16 * flag)` and asserts for a flag of 4 or more, so there are up to four record tables (the first draft of this note wrongly called the argument unused; Ghidra's decompile hides it). The game passes the client item's `unit+0x1bd` (`0x34a0e0`) or, on the server, `byte [game+0x106]` (note 11 saw flag 3 live) |
| Vanilla values (a mod's `misc.txt`) | | `isc` spellicon 0, `ibk` (Tome of Identify) 0, `tsc` -1, `key` -1 |
| Right-click use | `0x161f20` (InventoryItemManager.cpp), called from `0x2aa7e0` (inventory click, after `0x15a780` "can use") and `0x229440` (belt) | after its checks it calls `0x1c26b0(item)`; if that returns true it plays the use sound (`0x1a0780`) and carries on; the 0x26 use packet (`0x1400ed1e0`) is sent afterwards regardless |
| Cursor start | `0x1c26b0` | `cmp byte [rec+0x144], 0xa; jnc ret0` (`0x1c2706`), else opens the inventory panel if needed, calls `0x14f440(spellicon, item)` (`0x1c2ae0`) and returns 1 |
| Cursor state | `0x14f440` | sets the client globals `DAT_142a88708 = 5` (targeting mode, 6 with no item), `DAT_142a88700 = item`, `DAT_142a8870c = spellicon`, then `0x14fc20` redraws the cursor |
| Server side | `0x5817f0` check | `(rec[0x144] != -1) == (target != 0)` must hold or the check fails: an item with spellicon 0 and no target never passes the game's own check. Cartographer's `ItemSpellCheck` hook replaces this check for map rows, so the hook decides for them |

So the first map in a game works only because the server uses (consumes) the item, which removes the cursor target; a refused map leaves the item and the client stays in
targeting mode. The hotkey use (`0x1c7a30`) also calls `0x1c26b0`. The item interaction listener of PR #318 is not part of this fix and was dropped from PR #374.

### Fix

The real fix is data: `spellicon` -1 on the map rows in `misc.txt` (documented in `docs/cartographer/tables.md`; `cartographer check` notes a map item row with 0 to 9).
As a safety net for rows a modder has not fixed, `ClearMapItemCursor` (`portal.cpp`) does the same in memory. It runs from the player tick (the server side of the game, so single
player and the host; a joined client never runs it), once per data bank. It reads the data flag from `game+0x106` and walks that bank's records (`ItemRecord` part, rows from 0
until the first null; the flag must be below 4), and writes `0xff` to `+0x144` only for a row with `pSpell` 16, a spellicon byte below 10, and an item code that a row of the
Maps table names. A pSpell 16 row that no map names is left to the game's own check. The write goes through `WriteGame` (VirtualQuery and protection lift). Guards: the
`ItemRecord` part signature matched, `g_live`, flag below 4, the walk bounded at 16384 rows. The log line names the data bank and the row counts. The write is process-wide
and is never restored (not on leaving the game, not on unload), because the record table belongs to the process. A client item whose `unit+0x1bd` differs from the game's
flag would read another bank; that is not handled. No code bytes are written. With the part off nothing changes and the old behaviour stays.

### Blank spellicon cell (settled: reads as 0)

`0x315fd0` only builds the column table (`spellicon`, type 6, offset `0x144`). The byte the game reads is whatever D2RLoader's compiler writes for the cell (notes 07 and 11: the game loads the compiled table, so the empty value depends on the column; D2RCore's parser is not involved). Vanilla `misc.txt` cannot show it: the only blank `spellicon` is the `Expansion` marker row, which the compiler drops (`misc.bin` has 169 records).

Tried in game (D2R 3.3 build 93847, D2RLoader 1.3.1-beta, slot 11, test mod; the loader compiled the mod's `misc.txt` in memory at startup, "Compiled 188 tables from TXT", and wrote no `misc.bin`). Three map item rows with `pSpell` 16 and `spellicon` blank (`t20`), `0` (`t21`) and `-1` (`t22`). The record bytes were read from process memory (ReadProcessMemory, record found by its code at `+0x80`, `pSpell` `+0x94`) at the main menu, before any player tick, with no "spellicon set to -1" line in `cartographer.log` (so `ClearMapItemCursor` had not run):

| Row | `spellicon` cell | Byte at `+0x144` |
|---|---|---|
| `t20` | blank | `0x00` |
| `t21` | `0` | `0x00` |
| `t22` | `-1` | `0xff` |

A blank cell is therefore the same as 0 and shows the item cursor like it. `cartographer check` still notes only a literal 0 to 9; flagging blank cells too is a follow-up (the in-memory safety net already covers them: 0 is below 10).

### Verification levels

- Static: the chain above, read in Ghidra and Capstone from the dump. Not run.
- Unit tests and a clean build: see the PR.
- In game (memory read, no play): a blank `spellicon` cell compiles to byte 0 (table above).
- In game: the data fix (`spellicon` -1 in `misc.txt`) was confirmed by the owner with a real mouse. The runtime write is static only: not run in game.

### Reference

An open-source D2RLoader plugin, `kainabel666/Kains_D2RLoader_Plugins` (`pspell-ext`), hooks the same pSpell paths (`GetPSpellDescriptor 0x300AE0`, `PrepareHandler 0x5817F0`,
`DoHandler 0x581680`, `IsPSpellUsableNoTarget 0x308E80`). Read as a reference for the addresses only; no code was copied. It does not address the client cursor.
