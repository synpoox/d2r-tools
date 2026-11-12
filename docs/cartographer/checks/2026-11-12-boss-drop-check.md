# Cartographer: Boss Drop check (2026-11-12)

Live in game: the plugin built from `main` at `b6bbfdf` (DLL SHA-256 `D5335B77...D0923244`, copied into the slot, game started
after the copy), D2R 3.3 build 93847, D2RLoader 1.3.1-beta, slot 7, Hell, single player, test save `cartoh` (a level 1
Barbarian). `core-maps` was loaded next to Cartographer, so the log warned about it; the maps opened with `cartographer open`
were not affected. The automap file was a one-row seed passed with `-CoreUiAutomap`. Part of issue #111 (the Boss `Drop` roll).
The logs and screenshots are not kept in the repository.

Setup: `cartographer.txt` with `EventChance 100`, a Cave map `BossDrop` (Corridor, `zombie1` only, no map Boss), and one
`mapevents.txt` row, a Boss event with a `Drop`. The character used `cheat god 1` and `cheat easykill 1` and went to the event
room with `cheat goto`. Four games were played.

## Passed

- **The `Drop` roll fires when the Boss dies**: 3 of 3 kills, in three separate games (games 2, 3 and 4), with a plain `zombie1`
  as the event Boss and Drop `Act 5 (H) Super C` (games 2 and 3) or `Runes 10` (game 4):

      event 'Dropper' (Boss): spawned 2 of 2 in the event room (30640,40) 40 x 40 (0 as a superunique, 0 refused by the game)
      boss drop: Drop 'Act 5 (H) Super C': ... the game has 1519 treasure classes, lookup gave 1227
      boss drop: tracking unit ... (type 1, id 21)
      boss drop: rolling 'Act 5 (H) Super C' (treasure class 1227) for unit id 21 after the game's own drop: ...
      boss drop: the roll returned
      boss drop: the tracked boss ... died: already rolled for this boss

  Each kill logged one `rolling` line, one `the roll returned` and the "already rolled" line, so one roll per Boss.
- **The roll's own drop is visible (game 4)**: with Drop `Runes 10` a Thul Rune and gold were on the ground after the kill
  (`rolling 'Runes 10' (treasure class 195) for unit id 15`).
- 0 `ERROR` or `FAULT` lines in the plugin log.

## Not settled

- **A superunique Boss with a horde**: game 1 used Bishibosh as the Boss. His Fallen pack and shamans (who raise more Fallen)
  blocked the way; many kills with `easykill` never produced a `rolling` line, and the live Boss was never confirmed. This is why
  the later games used `zombie1`. Whether the roll fires for a superunique Boss is not settled.
- **What the Super C roll dropped (games 2 and 3)**: the ground held the zombie's own drop and the roll's together (potions,
  a magic Jagged Star and Battle Axe, white Gauntlets in game 3), so the roll's own items could not be told apart there. Game 4
  with a distinctive class is the clean sighting, and it is one kill.
- Item quality from the roll (it was not compared with the game's own drop).

## Not tested

- A Boss event with `Waves` or a Count above 1 on a real Boss, a Boss killed by something other than `easykill`, a second player.
