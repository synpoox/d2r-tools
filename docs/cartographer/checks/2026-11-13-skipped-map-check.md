# Cartographer: skipped map row message check (2026-11-13)

Live in game for #393: using a map item whose `maps.txt` row was skipped. Plugin built from `39c0b1b` (`tools\build.ps1
-Plugin cartographer -Test`, 21 of 21 tests pass; DLL SHA-256 `E5D83394...`), staged with `stage_check.ps1 -Slot 5 -MapItems`,
D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 5, Hell, single player, test save `cartoh` (a level 99 test Barbarian;
the save folder was backed up first and put back byte-identical afterwards). `debug_mode` and `cheat god 1` were on. The
logs and screenshots are kept outside the repository (`C:\D2RTestSlots\runs\skipmsg\`).

Setup: the example `maps.txt` with row 3 (`Tunneling Depths`, item `t21`) changed to tileset `Snow`, shape `Open`, which
`cartographer check` refuses (Snow takes no Shape). `cheat spawn t21,inv 0` put the item in the inventory.

| Check | Result |
|---|---|
| Right-click the item of the skipped row in Harrogath | PASS: the console (ctrl + `) shows `Map Tunneling Depths is not set up: maps.txt row 3 was skipped (see cartographer-report.txt).` once per use (two uses, two lines); the game says "I can't use this yet". |
| The item is kept | PASS: the item stays in the inventory after each use (screenshot `s6.png`, `s11.png`). |
| The reason is logged | PASS: `cartographer.log` has the row, name, item and the first problem (`the tileset 'Snow' cannot build the Open shape ...`). |

Log lines:

```
maps.txt row 3: the tileset 'Snow' cannot build the Open shape: Snow is lines of fence with no rooms to join, so it takes no Shape. Use Jungle, Cave or Crypt for an Open map. This map is skipped.
the item 't21' opens nothing: its maps.txt row 3 ('Tunneling Depths') was skipped at load: the tileset 'Snow' cannot build the Open shape: ...
Map Tunneling Depths is not set up: maps.txt row 3 was skipped (see cartographer-report.txt).
```

Screenshot `s11.png` (console open, two message lines, the item still in the inventory). The message goes to the loader
console, not to the on-screen text; the only text drawn over the game is the game's own "I can't use this yet".

Not checked: a row with an empty Name, a row skipped for a duplicate Item (both unit tested), a client in TCP/IP.
