# Cartographer: map panel on a rare map, in game (2026-11-13)

Live in game for issue #391: PR #403 (the panel laid out like item properties), slot 11, Hell, single player, test save `cartos`
(Barbarian, level 99), god mode (`debug_mode`, then `cheat god 1`) before any map. D2R build 93847, D2RLoader 1.3.1-beta.
Staged with `stage_check.ps1 -Slot 11 -MapItems -Tables examples\first-map`; the slot's test mod already had `cmap` affix rows in
`magicprefix`, `magicsuffix`, `rareprefix` and `raresuffix` (mon-life, mon-damage, mon-fire-res, mon-speed, res-all, str,
mon-cold-res), and the map rows have `compactsave` 0. Maps came from `cheat spawn t20,typ r,lvl 90,inv 0`, then `cheat idall`, then a
right-click and the red portal. Screenshots stay outside the repo.

| # | Check | Result |
|---|---|---|
| 1 | Rare map shows a yellow rare name and blue property lines on hover | PASS (`Dread Eagle Shorelines Map`) |
| 2 | The game's stat text is found with the `d2r:` prefix | PASS |
| 3 | `ÿc3` / `ÿc4` colour codes in the panel text | FAIL: printed as `yc3`, no colour |
| 4 | Headers gold, property lines blue (two text boxes) | PASS |

## Findings

- **Colour codes do not work in the panel's text widget.** With the text `ÿc4Monsters have` and `ÿc3+40% Enhanced Damage` (UTF-8
  C3 BF) every line was gold and showed the code as plain text. Nine variants set in one panel in a throwaway build all printed
  literally or vanished: U+E07E plus the digit or `;`, raw byte 0xFF (it also changed the font), a no-break space, `^c3`,
  `<c3>`, `[c3]`. The toolkit notes say the same for tooltip listeners. The panel now has two `TextBoxWidget`s over the same area, one
  gold (headers) and one item-property blue (105, 105, 255), with an empty line where the other has text.
- **Stat names read well.** The game's own text for the stats shows (`+40% Enhanced Damage`, `+5 to Strength`, `Fire Resist +9%`,
  `15% Better Chance of Getting Magic Items`); the fallback table and the tidied name cover the rest (`+14% Movement Speed`).
  `Increase Maximum Life 35%` is the game's wording for the monster life stat.
- The map name line is gone and the groups read `Monsters have` and `Players have`.

Not checked: other languages, a unique map, multiplayer, the panel next to a terror zone text, and the negative-value text
(`descstrneg`) on a real game string (unit tested only).
