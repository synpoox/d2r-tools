# Cartographer: event banner drawn as a panel (2026-11-12)

Issue #357 (part of #111). Live in game: the plugin built from this branch (on `origin/main` `6e1ae44`), D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7,
Hell, single player, test save `cartos` (a level 1 Barbarian; `debug_mode` and `cheat god 1` were sent right after the game started, before any map).
The logs and screenshots are not kept in the repository.

## Finding

The plugin banner never drew because it was drawn through D2RLoader's Overlay service, whose `drawText` returns `Success` and puts nothing on screen
(already found for `better-xp-bar`, `player-bars` and the map panel: `docs/reverse-engineering/better-xp-bar.md` section (e), `cartographer/22-map-panel.md`).
The old log lines ("entry message shown on the overlay", the late banner call) only said the text was handed over. The "ENTERING <name>" text seen in earlier
checks is the game's own level-entry text. The overlay callback and service were obtained and `OnFrame` ran, so no "could not draw" warning was ever possible.

## Change

The banner is now a second plugin panel (`cartographer/entry-banner`, three centred text boxes: map name, event name, event message), opened for the
banner's time (6.1 s) and closed after, set through the same game UI parts the map panel uses. No fade (the overlay's alpha is gone). Without the panel the
message goes to the console as before. New log lines: `entry banner: panel registered`, `panel opened`, `text set (N line(s))`, `closed`.

## Setup

`cartographer.txt` with `EventChance 100`; a Crypt map `BannerMap` (Corridor, Small, item `t20`); one `mapevents.txt` row, `Boss`, `zombie1`, Count 1,
Name `The Drowned King`, Message `The water churns...`. `cartographer open BannerMap` in Harrogath, entered by the portal, the event room reached with
`cheat goto 138,17700,40`. Screenshots as a burst of about 6 per second (`PrintWindow`, one process), taken from the click on the portal and from the `goto`.

## Seen on screen

- **Entry banner.** After the portal: "BannerMap" in gold, centred at about 17% of the height, next to the game's own red "Entering BannerMap" (so both
  are on screen and distinguishable). Log: `entry message handed to the banner panel (1 line(s))`, `panel opened`, `text set (1 line(s))`, `closed` 6.1 s later.
- **Late banner.** After the `goto`, the Boss spawned (`spawned 2 of 2`) and the banner showed "The Drowned King" in gold over "The water churns..." in red
  (the Name and the Message), top centre, about 1 s into the burst after the room was reached. Log: `handed to the banner panel (2 line(s))`, `panel opened`,
  `text set (2 line(s))`, `closed`.
- 0 `ERROR` or `FAULT` lines in the plugin log.

## Not settled

- The late banner's colours: its two lines use the first two colours (gold, red) so the Name is gold, not red as on a three-line banner. Cosmetic.
- The banner does not fade; it appears and disappears.
- Longer text than one line per text box, other resolutions, other languages, TCP/IP, and a banner during a loading screen were not checked.

## Verified at each level

Unit tests: `bannerVisible` (`eventplan_tests`, 19 of 19 cartographer test executables pass). Clean build: `tools\build.ps1 -Plugin cartographer -Test`. Install into a
game: staged with `stage_check.ps1` (slot 7). Live in game: as above. Slot restored with `stage_check.ps1 -Restore`, the staged `cartos.*` saves removed, slot unlocked.
