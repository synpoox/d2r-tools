# Cartographer: map item check (2026-11-12)

Live in game: the plugin built from `main` at `9bdc3f8` (before #209; the build is from the run notes, the logs show only plugin version 0.1.0), slot 6, Hell, single player, test save `cartoh`, D2R 3.3
build 93847, D2RLoader 1.3.1-beta. The three maps were staged with `stage_check.ps1 -MapItems`, which puts each map's own item in
the inventory: t20 'Hollow Cave Map' (Cave, 'Hollow Cave'), t21 'Overgrown Map' (Jungle, 'Overgrown') and t22 'Frostline Map'
(Snow, 'Frostline'). Each map was opened from its item in Harrogath, one map per game. The earlier run is
[Hell checks 4 and 5](2026-11-12-hell-check-4-5.md). The logs and screenshots are not kept in the repository.

## Passed

For all three maps (Cave, Jungle, Snow):

- **The item opens the map**: using the item in town opened the portal.
- **The map's name** showed on the portal hover and in the top-right area label.
- **Arrival**: the character arrived in the entrance cell.
- **Map Boss**: 'Test Warden' spawned in the boss room, as a superunique:

      map Boss 'Test Warden': spawned in the boss room (30640,640) 40 x 40 (as a superunique)
      map Boss 'Test Warden': spawned in the boss room (30520,15520) 40 x 40 (as a superunique)
      map Boss 'Test Warden': spawned in the boss room (30000,18240) 40 x 40 (as a superunique)

  (Cave, Jungle and Snow, in that order.)
- **Map stats**: the rolled monster fire resist showed on hovered monsters (debug overlay) in Cave ('Test Warden', 34%, logged
  `fireresist(39)=+34`) and Snow (a zombie, 21%, logged `+21`). Jungle only in the log: `fireresist(39)=+31`, then
  `statprobe: left the game: 34 monster(s) ... were given the map's stats`. The portal hover showed only the name.
- **Entry banner** ("Entering Hollow Cave") seen for Cave only.
- **Horde in the Cave maze**: 6 of 6 spawned in the event room:

      event 'The Horde' (Horde): spawned 6 of 6 in the event room (30640,40) 40 x 40 (0 as a superunique, 0 refused by the game)

- **One map per game**: using a second map item in the same game was refused and the item was kept, seen in the Cave (maze) game only:

      Cartographer: Overgrown cannot be opened: a map has already been opened in this game.

- 0 `ERROR` or `FAULT` lines in the plugin log of the three games.

## Found

- **#218**: the item's tooltip says "Scroll of Town Portal", not the map's own name.
- **#219**: a `core-ui` `automap.txt` seed that has only a header row crashes the startup (a logged `FAULT 0xc0000005` in the
  plugin log of the extra start-up game, not in the three map games). The FAULT line itself says the plugin is not on the stack
  ("probably not the cause"), so it is not a Cartographer crash.

## Not settled

- **Horde in Jungle and Snow**: the Horde was planned for both (`event for 'Overgrown' ... Horde 'The Horde' (zombie1) x6`), but no
  `spawned ... in the event room` line was logged and none was seen. The character probably never reached the event room (Snow
  check 1 had 6 of 6 when the room was visited). This needs a recheck; it is not called a bug.

## Not tested

- The entry banner in Jungle and Snow (only the portal hover and the area label were seen).
- The one-map-per-game refusal in an outdoor game (the only outdoor refusal in the logs is from an earlier run, 12:26, with
  different wording).
