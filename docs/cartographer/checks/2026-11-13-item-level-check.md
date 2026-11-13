# Cartographer: item level label check (2026-11-13, issue #384)

Live in game, slot 7, build 93847 / D2RLoader 1.3.1-beta, test save `cartoh` (Barbarian, `cheat level 99`), god mode, single
player, plugin built from origin/main `6cb1994` with `-MapItems` staging. The slot's `display_item_levels` was turned on in
`d2rloader.toml` for the run and put back to `false` afterwards.

## What was seen

- With `display_item_levels = true` the game adds the item level in brackets to the label and the tooltip name of the item
  types it lists (weapons, armor, rings, amulets, charms, jewels): `cheat spawn rin,lvl 91` read `Ring (91)` on the ground, in
  the inventory (`Snake's Ring of Measure (91)`) and inside an open map (Shorelines, monster level 90).
- A map item (`t20`, a scroll base with item type `cmap`) is not one of those types: its label and tooltip read `Shorelines Map`
  and carry no item level text at all, in Harrogath and inside the map, with 20 on the ground at once, spawned at `lvl 91`
  or by `cheat isl 91`.
- The text `Item Level: 1` (the label in the #353 check records) was **not reproduced**: not on a ground label, a tooltip (also
  with Alt and Ctrl held) or the Debug Tools panel. The string is not in the plugin's source or tables, nor in
  `D2R_loader.dll` or `D2RCore.dll`.
- Monster drops inside a map were not seen: `cheat killmonster` did not kill the spawned zombies, so no drop level was read.

## Not settled

Where `Item Level: 1` comes from, and whether a map's real item level is 1 (the risk in #384: affix rows with `level` above 1
would never roll). The level of a dropped map could only be read through the game's own bracket label, which a map item does
not get. A next check needs a way to read the unit's item level on the game thread (SDK item details, or the item level getter
`0x36ccf0` in the toolkit's address table), then the quality hook's `request` level, for a cheat spawn, a monster drop and a
cube output.
