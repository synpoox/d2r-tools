# Cartographer: cube map level check (2026-11-12)

Live in game: a cube recipe raises a map's level (#330). D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, test mod `D2RToolsTest`,
Hell, single player, test save, god mode (`debug_mode`, `cheat god 1`). Two runs:

- Run 1 (slot 6): a build with extra logging and the earlier logic.
- Run 2 (slot 5, the final DLL of this head, `levelKnown = ok && (inputLevel >= 0 || !CubeExec hooked)`): the recipe `t21` +
  `tsc` -> `usetype,nor,mod` `map-level` 10, on a fresh Tunneling Depths Map (no stat). All three checks passed, below.
  The slot's tables and plugin were restored and the slot unlocked afterwards. The slot's `maps.txt` was edited for the runs
  (three Cave maps) and put back. Logs are kept outside the repo.

| Check | Result |
|---|---|
| 1. A fresh map gets the roll (0 to level) | PASS (run 2): `stat before 0 after 10, cube input 0, target 10, now 10`. |
| 2. A second transmute adds again | PASS (run 1 and run 2): run 2 `stat before 10 after 20, cube input 10, target 20, now 20`. |
| 3. The map opens at it | PASS (run 1 and run 2): run 2, used in Harrogath (CaveB opens only there), `map level 20 (from the item's stat)`, the map monsters took Level 20, a portal opened (CaveB). |
| 4. The tooltip shows it | Not seen on a map: map items show no property lines in this game. The line shows on non-map outputs (see the #310 check in `docs/reverse-engineering/cartographer/14-item-stats.md`). |
| Stability | No `[FAULT]` lines from the plugin during the transmutes. |

Two notes on run 2: in Act 1 the map said "Not in town", and outside town "I can't use this yet" (CaveB opens only in
Harrogath), so the character was warped with `cheat goto` (Blood Moor, then Harrogath) before the map was used. The test
character `cartoh` was created for the run in the test save folder (outside the repo).

Test mistakes, already reported: a first boot script targeted slot 7, which another lane had locked and was running. A few
console commands (`debug_mode`, `cheat god 1`, `cheat easykill 1`) and clicks reached that game before it was noticed; the slot was
not touched again. A recipe row was also added to slot 6's `cubemain.txt` by mistake (a script pointed at the wrong slot; slot 6 was
free and not running), and removed again.

Also found and fixed during run 2: `tools\install.ps1` lost the `map-level` property row (and, on a fresh mod, the map-level
stat row) when a table was read a second time while its first edit was still queued, so the first game start logged
`property id -1`. A table already queued is now reused.
