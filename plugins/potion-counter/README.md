# Potion Counter

A D2RLoader plugin for Diablo II: Resurrected that shows what is in each belt column as a row of small framed squares
above the belt slot, the way Project Diablo 2 shows it. It only shows them: it never fills, refills or moves
anything.
MIT licensed.

## Features

- **A square per belt row**: above each belt slot, one square for each row your belt has (1 to 4), in the game's own
  belt slot frame, showing the potion or scroll in that place with its own picture. Empty places stay dark; an item with no stock picture shows a
  gold mark.
- **Always current**: the squares change as you drink, pick up or move potions, or change belts.
- **Fits the belt**: the squares follow the game's own belt layout and HUD scaling, go away with the HUD and while the
  belt is popped up, and never cover the belt slots, so clicks on the belt work as before.

## Requirements

- Diablo II: Resurrected 3.3 (Battle.net or Steam). Other versions are attempted, not tested.
- D2RLoader (tested on 1.3.1)

Works with or without mods.

## Installation

1. Close the game.
2. Copy `potion-counter.dll` to `<game>\d2rloader\plugins\`.
3. Start the game through D2RLoader.

## Configuration

None: the plugin has no settings.

## License

[MIT](LICENSE) © Synpoo.

Diablo® II: Resurrected is a trademark of Blizzard Entertainment, Inc. This project is not affiliated with or
endorsed by Blizzard Entertainment.
