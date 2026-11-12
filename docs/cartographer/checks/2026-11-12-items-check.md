# Cartographer: map item check (2026-11-12)

Live in game: the plugin built from `main` at `9bdc3f8` (before #209), slot 6, Hell, single player, test save `cartoh`, D2R 3.3
build 93847, D2RLoader 1.3.1-beta. The three maps were staged with `stage_check.ps1 -MapItems`, which puts each map's own item in
the inventory: t20 'Hollow Cave Map' (Cave, 'Hollow Cave'), t21 'Overgrown Map' (Jungle, 'Overgrown') and t22 'Frostline Map'
(Snow, 'Frostline'). Each map was opened from its item in Harrogath, one map per game. The earlier run is
[Hell checks 4 and 5](2026-11-12-hell-check-4-5.md). The logs and screenshots are not kept in the repository.

## Passed

For all three maps (Cave, Jungle, Snow):

- **The item opens the map**: using the item in town opened the portal.
- **The map's name** showed on the portal, the entry banner and the automap.
- **Arrival**: the character arrived in the entrance cell.
- **Map Boss**: 'Test Warden' spawned in the boss room, as a superunique:

      map Boss 'Test Warden': spawned in the boss room (30640,640) 40 x 40 (as a superunique)
      map Boss 'Test Warden': spawned in the boss room (30520,15520) 40 x 40 (as a superunique)
      map Boss 'Test Warden': spawned in the boss room (30000,18240) 40 x 40 (as a superunique)

  (Cave, Jungle and Snow, in that order.)
- **Map stats** showed when hovering over the portal.
- **Horde in the Cave maze**: 6 of 6 spawned in the event room:

      event 'The Horde' (Horde): spawned 6 of 6 in the event room (30640,40) 40 x 40 (0 as a superunique, 0 refused by the game)

- **One map per game**: using a second map item in the same game was refused and the item was kept, in the maze as well as
  outdoors:

      Cartographer: Overgrown cannot be opened: a map has already been opened in this game.

- 0 `ERROR` or `FAULT` lines in the plugin log of the three games.

## Found

- **#218**: the item's tooltip says "Scroll of Town Portal", not the map's own name.
- **#219**: a `core-ui` `automap.txt` seed that has only a header row crashes the startup (a logged `FAULT 0xc0000005` in the
  plugin log of the extra start-up game, not in the three map games).

## Not settled

- **Horde in Jungle and Snow**: the Horde was planned for both (`event for 'Overgrown' ... Horde 'The Horde' (zombie1) x6`), but no
  `spawned ... in the event room` line was logged and none was seen. The character probably never reached the event room (Snow
  check 1 had 6 of 6 when the room was visited). This needs a recheck; it is not called a bug.

## Not tested

- The Jungle and Snow Horde (see above).
