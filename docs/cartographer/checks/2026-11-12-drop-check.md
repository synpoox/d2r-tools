# Cartographer: Boss Drop and Treasure chest loot check (2026-11-12)

Live in game: the plugin built from `main` at `d36ee90` (DLL SHA-256 `7FC89F188C0B5FD5...`, copied into the slot by `stage_check.ps1`; the
log's first line is `0.1.0 loaded (mod D2RToolsTest, D2R 3.3.0, image 0x6ab3782c)`), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7, Hell,
single player, test save `cartos` (a level 1 Barbarian, not the level 99 test character: god mode and `easykill` were on). The PD2 plugins were
parked by the stage script. `debug_mode` and `cheat god 1` were sent right after each game started, before any map was opened. Parts of
issues #111 and #220. The logs and screenshots are not kept in the repository.

Setup: `cartographer.txt` with `EventChance 100`, a Crypt map `BossDrop` (Corridor, Small, `zombie1` only, no map Boss), opened with
`cartographer open BossDrop` and entered by the portal; the event room was reached with `cheat goto`. One `mapevents.txt` row per game, so one event type per game:

| Game | Event row | Drop |
|---|---|---|
| 1 | Boss `zombie1`, Count 1 | `Runes 12` |
| 2 | Treasure, `Spawn` 146 (an objects row number; this slot's test mod has no `objects.txt`) | `Runes 17` |

(One earlier attempt of game 2 used `Chest2` as `Spawn` and the log said the name could not be turned into a row without `objects.txt`: the event was
skipped, as designed. A first attempt with a Cave map failed because the slot's Maze level is a Crypt level.)

Ground items are not drawn without a label in this slot (they were invisible in plain screenshots, which first looked like "nothing dropped").
Holding Alt (the harness sets the modifier and the key down) shows the item labels; the pictures below are Alt shots.

## Passed

- **Boss `Drop` dropped an item (game 1, one kill).** The Boss was tracked (`boss drop: tracking unit ... (type 1, id 13)`), spawned with one minion
  (`event 'Rune Test' (Boss): spawned 2 of 2 in the event room ... (0 as a superunique, 0 refused by the game)`). After the kills:

      boss drop: the tracked boss (unit ..., id 13, type 1) died: rolled
      boss drop: rolling 'Runes 12' (treasure class 197) for unit id 13 after the game's own drop: killer ..., level value 85, arg3 0
      boss drop: the roll returned

  The Alt shot at the bodies showed a **Ral Rune** and an **Elder Staff** (white) (`loot_alt.png`). The staff is not from `Runes 12`; a rune is the roll's
  kind of item. One roll line, no second roll, a level value of 85 (the map's monster level). The minion died too, so the Ral Rune is attributed to the
  Boss's roll by its class, not by a control.
- **Treasure chest loot dropped (game 2, one chest).** The chest (objects row 146) was placed in the event room, was not locked
  (`treasure chest: unit ... was not locked`) and was tracked (`id 34, mode 0`). A click on it (it opened, lid up) logged:

      treasure chest: the operate call on the tracked chest (unit ..., activator ...) returned 1, mode 0 -> 1: rolled
      treasure chest: Drop 'Runes 17' (treasure class 202): data flag 3, level value 85, compiled class ...
      treasure chest: the roll returned 9223372036854775808 (tier 0, forced quality 0, at most 6 items)

  The Alt shot beside the chest showed one **Ith Rune** (`chest_alt.png`). No monster was killed in that game, so nothing else could drop there.
  This is the loot the first #220 report (Hell check 5) missed; its cause was the hook on a function a click never reaches, and the hook on the chest
  handler (sections 18 and 19) has now dropped loot in this game as well.
- 0 `ERROR` or `FAULT` lines in the plugin log (both games).

## Not settled

- **The roll's item quality and amounts**: both drops were runes, which have no quality. A `Drop` that gives gear was not looked at here.
- **A control for the Boss roll**: the minion's own drop was not separated from the Boss's (a zombie's own drop is not a rune class, so this is only inferred).
- **Why the first #220 game showed no loot**: not reproduced; the explanation in section 18 (the old hook on `0x592c40`) was already in the tree.
- A superunique Boss roll was settled earlier ([names-drop-check](2026-11-12-names-drop-check.md)); this check used a plain `zombie1`.
- The loot filter of the slot's character hides item labels without Alt; not a Cartographer matter.

## Other

- One assert line in the loader log at the map entry, as in earlier checks (`BC_VERIFY: ptWarp ... RoomTile.cpp:747`, see [#254](https://github.com/synpoox/d2r-tools/issues/254)); the game went on.
- The `verifyitems` cheat writes nothing the harness or plugin log shows, so it was not used for evidence.
- Slot 7 was held by another lane (carto-337) for about 14 minutes after the start of this task; the check waited for it to be freed, took it with `stage_check.ps1` and restored and unlocked it afterwards (the staged DLL, tables and the staged `cartos` files were removed by the restore).
