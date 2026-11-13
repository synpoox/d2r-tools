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
