# Testing Player Bars

Run in a toolkit game slot. `player-bars` in the console prints the live state.

| # | What | Expected | Status |
|---|---|---|---|
| 1 | Loads | Log: `bars on: ... shift found` | passed, slot 2 |
| 2 | Bars above the character at 1600x900 and 1720x720 | Life 293/293 and mana 148/148 match the character panel | passed, slot 2 |
| 3 | Inventory, character, both panels | Bars follow the character | passed, slot 2 (top); inventory only for bottom |
| 4 | Click to walk with bars up | Character walks | passed, slot 2 |
| 5 | `show_text = true` | Numbers on the bars | passed, slot 2 |
| 6 | Default config in town at 1600x900 and 1720x720 | Bars show, globes and statues gone, frame kept | passed, slot 2 |
| 7 | `position = "bottom"` | Bars below the character's feet | passed, slot 2 |
| 7a | Click ground beside the frame | Character walks | passed, slot 2 |
| 7b | Click the frame | Character does not walk | passed, slot 2 |
| 7c | Character button at 1600x900 | Opens the panel | passed, slot 2 |
| 7d | Controller HUD at 1600x900, from a stand-in `xinput9_1_0.dll` in the slot | Bars show, globes and statues gone, frame kept | passed, slot 2 |

## Owner: in the field

| # | What | Expected |
|---|---|---|
| 8 | Take damage, cast, drink a potion | Fill follows life and mana |
| 9 | Die and respawn | Health bar empties, then refills; orbs stay hidden |
| 10 | Leave and rejoin a game, change area | Bars come back; no faults in `player-bars.log` |
| 11 | A real controller | Same as 7d, also with side panels open and when switching back to the mouse |
| 12 | Your resolution | Bars clear the head; adjust `offset_y` if not |
