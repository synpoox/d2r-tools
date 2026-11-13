## 27. Map item cursor, in-game attempt (issue #307, follow-up to note 22)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Slot 5, test character, `debug_mode` + `cheat god 1`.

### Result

The cause is still not established, and the fix is unverified in game. No code changed.

### What was tried

- Static: the client right-click use (`FUN_140161f20`, InventoryItemManager.cpp) only sends the use packet (0x26) through `FUN_1400ed1e0`; no cursor or identify
  prediction was found in it. The identify handler (`pSpell` 1, `0x57ec30` -> `FUN_14046e8c0`) is a server-side ItemMode change that probably puts the item on the cursor.
  A refused map item never reaches it (`ItemSpellCheck` returns 0 at `0x4f52c6`), so the cursor likely comes from client data (the `isc` base art or `spellicon`), not the server.
- In game: the item guard installed (`cartographer.log`: "item guard on"). Harness mouse hover highlights inventory items, but harness right-clicks
  (`game_ctl.py click fx fy right`) on a Town Portal scroll and on a map item neither used the item nor reached the guard (no portal, no log line, no cursor change).
  The harness cannot reach the client's right-click use handling, so the symptom was not reproduced and no fix could be checked.

### Next

Reproduce with a real mouse and capture which cursor shows; then compare a clone of `isc` with a different base art. Only then change code.

## Static follow-up: where the cursor starts (found 2026-11-12, static only, Ghidra project of the `0x6ab3782c` dump)

The note above guessed the server. It is the client, and it is driven by one `misc.txt` column.

### Result

The client starts the item (identify) cursor itself, on right-click, from the item row's `spellicon` column, before the server is asked. A map row copied from `isc`
has `spellicon` 0, so the cursor starts on every map right-click; the server then refuses a second map (item stays), and nothing ever clears the cursor.
Setting `spellicon` to -1 (as `tsc` and keys have) makes the client skip it.

### Evidence (RVAs in the memory-layout dump; call chain)

| Step | Address | What it shows |
|---|---|---|
| Item record layout | `0x315fd0` (misc column loader) | column table entry `{name "spellicon", type 6, offset 0x144}`; `pSpell` is `{type 2, offset 0x94}`. Record lookup `0x314110` (record size `0x1c0`, array at table `+0x15a0`, count at `+0x15a8`; its first argument is unused, one table for the process) |
| Vanilla values (a mod's `misc.txt`) | | `isc` spellicon 0, `ibk` (Tome of Identify) 0, `tsc` -1, `key` -1 |
| Right-click use | `0x161f20` (InventoryItemManager.cpp), called from `0x2aa7e0` (inventory click, after `0x15a780` "can use") and `0x229440` (belt) | after its checks it calls `0x1c26b0(item)`; if that returns true it plays the use sound (`0x1a0780`) and carries on; the 0x26 use packet (`0x1400ed1e0`) is sent afterwards regardless |
| Cursor start | `0x1c26b0` | `cmp byte [rec+0x144], 0xa; jnc ret0` (`0x1c2706`), else opens the inventory panel if needed, calls `0x14f440(spellicon, item)` (`0x1c2ae0`) and returns 1 |
| Cursor state | `0x14f440` | sets the client globals `DAT_142a88708 = 5` (targeting mode, 6 with no item), `DAT_142a88700 = item`, `DAT_142a8870c = spellicon`, then `0x14fc20` redraws the cursor |
| Server side | `0x5817f0` check | `(rec[0x144] != -1) == (target != 0)` must hold or the check fails: an item with spellicon 0 and no target never passes the game's own check. Cartographer's `ItemSpellCheck` hook replaces this check for map rows, so the hook decides for them |

So the first map in a game works only because the server uses (consumes) the item, which removes the cursor target; a refused map leaves the item and the client stays in
targeting mode. The item listener of PR #318 does not change this: whatever it consumes, the owner saw the cursor, so either it does not see the real right-click or the cursor start
is reached from another caller (`0x1c7a30` hotkey use also calls `0x1c26b0`). The data fix below does not depend on it.

### Fix

`ClearMapItemCursor` (`portal.cpp`) walks the item record table (`ItemRecord` part, rows from 0 until the first null) and writes `0xff` to `+0x144` of every row with `pSpell` 16 whose
spellicon byte is below 10. Guards: the `ItemRecord` part signature matched and `g_live`; only rows with `pSpell == 16`; only a byte below 10 is changed; the walk is bounded (16384
rows). It runs once per game from the player tick and also from the item listener, before the game's right-click handling. No code bytes are written. Failure mode: with the part off
nothing changes and the old behaviour stays. A map row already at -1 is left alone (a modder may set it in `misc.txt`; that is the better source of truth).

### Verification levels

- Static: the chain above, read in Ghidra and Capstone from the dump. Not run.
- Unit tests and a clean build: see the PR.
- In game: not done. No real mouse is available to agents. The owner's check is in the PR body.

### Reference

An open-source D2RLoader plugin, `kainabel666/Kains_D2RLoader_Plugins` (`pspell-ext`), hooks the same pSpell paths (`GetPSpellDescriptor 0x300AE0`, `PrepareHandler 0x5817F0`,
`DoHandler 0x581680`, `IsPSpellUsableNoTarget 0x308E80`). Read as a reference for the addresses only; no code was copied. It does not address the client cursor.
