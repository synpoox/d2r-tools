# Cartographer: Shrine by name, Boss equipment Drop over many rolls, event banner (2026-11-12, second pass)

Follow-up to [2026-11-12-shrine-check.md](2026-11-12-shrine-check.md), part of issue #111. Live in game: the plugin built from `main` at `d656658`, D2R 3.3.0
build 93847, D2RLoader 1.3.1-beta, slot 7, Hell, single player, test save `cartos` (a level 1 Barbarian, god mode and `easykill` on; `debug_mode` and
`cheat god 1` were sent right after each game started, before any map). The logs and screenshots are not kept in the repository.

Setup as in the first record: `cartographer.txt` with `EventChance 100`, a Crypt map `BossDrop` (Corridor, Small, `zombie1` only, `Item` `t20`), opened
with `cartographer open BossDrop` from Harrogath, entered by the portal, the event room reached with `cheat goto 138,<x>,<y>` from the log's
`maze event room` line. Each Boss game used one `mapevents.txt` row: `Boss`, `Spawn` `zombie1`, `Count` 1, `Drop` `Act 5 (H) Equip C`, a `Message`.

## Passed

- **Shrine by name.** `ShrineHealth` (resolved to objects row 172) was created in the event room. Hovering it showed **Health Shrine**; a click that
  landed on it showed **"You feel healthy."** So a shrine spawned by name works like the by-row one; the first record's missed click was the harness, not
  the object. (`objects.txt` was copied into the slot's test mod by hand so the name could resolve.)
- **Boss equipment Drop over many rolls.** 9 Boss kills produced a roll (`the tracked boss ... died: rolled`, `rolling 'Act 5 (H) Equip C' (treasure
  class 360)`, `the roll returned`; one `rolling` line per kill, never a second roll). 7 of them left items on the ground, 9 items in all:

| Roll | Items seen (name colour) | Quality |
|---|---|---|
| 1 | Boneweave [+4] (grey) | socketed |
| 2 | Gothic Plate (blue), Bolts (white) | magic, normal |
| 3 | Succubus Skull (gold-tan) | colour class not confirmed |
| 4 | Cutlass [+1] (grey) | socketed |
| 5 | Hydra Edge (white), Crude Ghost Spear (white) | normal, low quality |
| 6 | Superior Broad Axe (white) | superior |
| 7 | none visible | |
| 8 | Plated Belt (blue) | magic |
| 9 | none visible | |

  Qualities seen: normal, low, superior, magic and socketed; nothing in rare yellow. The ground is dark and crowded, so an item under a body or off the
  shot can be missed, which may explain the two rolls with nothing visible; a roll that really gave nothing was not ruled out.
- **Items from the roll, not from minions.** The log does not list items. Method (data only, no code): the slot's `monstats.txt` had the `zombie1` Hell
  treasure class columns blanked, so the Boss and its minions (all `zombie1`) drop nothing by themselves and every item on the ground comes from the
  roll. Items above are from those games. The record has no control run showing the natural drops, only the first record's observation (a Shako that was
  on the ground before the Boss died).
- 0 `ERROR` or `FAULT` lines in the plugin log in these games.

## Event banner (not confirmed on screen)

With `EventChance 100` and a non-empty `Message`, the log showed the late banner call (Name and Message; the event spawned after entry). Across 21
screenshots in two triggers, the plugin's banner (red Name, grey Message under the map name) never appeared; only the game's own "Entering BossDrop"
banner showed, and the entry banner had only the map name line (the event had not spawned yet). So the banner is logged but **not seen on screen**.
Screenshots at the harness's pace may miss a 6 s fade; this is not a pass.

## Not settled

- **Banner**: not seen on screen, as above. A faster screenshot burst or a screen recording is needed.
- **Roll 3's item quality**: its name colour is the gold-tan the game uses for unique items, but the name shown is a base name, so it was not confirmed.
- **Rolls with nothing visible** (7 and 9 in the table): see above.
- **Multiplayer**: not checked.
- The shrine's effect on health was not measured.

## Other

- Two games (4 and 10) never rolled: the harness sent a malformed `cheat goto` (a stray carriage return in the coordinates), so the Boss was not reached.
  They are not counted.
- A second lane's script sent `debug_mode`, `cheat god 1`, `cheat easykill 1` and clicks into the slot 7 game during the run. `easykill` was already on
  for every kill here (it was sent after each `goto` to kill the Boss quickly), so the stray input changed nothing about the kills; the Boss's roll is
  made by the plugin on the Boss's death and does not depend on how the Boss died. The stray input may have added a few clicks in game 6.
- Slot restored with `stage_check.ps1 -Restore`; the kept (hand-edited) tables, the hand-placed `objects.txt`, the edited `monstats.txt` and the stray
  `cartos.*` saves were removed or restored by hand; slot 7 unlocked.
