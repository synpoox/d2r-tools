# Cartographer: Lava map edges and the filler ring (#372), 2026-11-13

Live in game for #372, the items PR #377 left unchecked. Plugin built from `origin/main` (`4745cdc`) with `tools\build.ps1 -Plugin
cartographer -Test` (44 of 44 test cases pass), no product code changed. D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, slot 7, Hell,
single player, test save `cartoh` (a level 99 test Barbarian; the slot's save folder was backed up first and put back afterwards).
`debug_mode` and `cheat god 1` were on before every map; `cheat stat item_lightradius,25` made the dark edges readable. Maps were
opened with `cartographer open <Name>` in Harrogath and entered through the portal; a maze slot builds one map per game, so each map was a
new game. Example tables with four Lava rows (level 140): Large Winding, ExtraLarge Corridor, ExtraLarge Winding, ExtraLarge Sprawl.
`cheat goto 144,x,y` takes subtile coordinates (tile * 5 + 2). The logs and screenshots are kept outside the repository
(`C:\D2RTestSlots\runs\lava-edges-check\`).

## Results

| Check | Result |
|---|---|
| Piece id write to the closed piece 836 | PASS by log: every game logged `maze filler ring: N of N rooms made, N set to the closed piece 836` (54 of 54, 50 of 50, 58 of 58, 54 of 54) and `built as planned`. The counter counts writes; the piece was not read back. |
| Black void at the four edges, Large Lava (Winding, seed 791276690275095032, level 360x264) | PASS: north, south, east and west edges of the walkable ground show lava and cliff to the screen edge, no black field (`L_n2`, `L_s2`, `L_e2`, `L_w2`). |
| Black void at the four edges, ExtraLarge Lava (Corridor, seed 6060755728845329565, 360x216; Winding, seed 1866512260374242075, 216x360) | PASS: all four edges of both layouts show lava and cliff, no black field (`sheet_XC`, `sheet_XW`). |
| Clipped layout (ExtraLarge Corridor, seed 16528205731205976003, level 360x360, ring clipped on the east side) | PASS for east, north and south: 54 of 54 fillers made, lava and dark ground at the edge, no black field (`C_e`, `C_e2`, `C_e3`, `C_n`, `C_s`). The west edge was not reached (`C_w`: the goto left the player at the arrival). |
| Filler rooms hold no monsters or uniques | Not counted. Nothing was seen standing past the walkable edges in any shot; the closed piece has no walkable ground. |

## Notes

- The root cell of a real layout is not where `capacityAt` assumes (centered first room). Measured root cells (7,5), (6,4), (4,6), (3,6);
  goto targets had to be worked out from the log's arrival, event and boss rects (an 8x8 rect sits at the cell origin + 8, a cell
  origin is 2500 + cell * 24). A goto onto a tile with no walkable ground leaves the player at the arrival.
- A scratch unit test over the plan showed about 17 to 21 percent of ExtraLarge Corridor and Winding layouts fall back to margin 0 and
  clip the ring; Large clips rarely. Not committed.
- "No black void" was judged by eye on screenshots. The top of some shots is the black past the level itself.

Not checked: a per-room monster or unique count for the fillers, the west edge of the clipped layout, a read-back of the piece id,
Normal and Nightmare, multiplayer.
