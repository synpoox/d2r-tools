# Potion Counter: native findings

Build: D2R 3.3 build 93847 under D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Evidence: the in-memory dump
`D2RLoader_6ab3782c_dump.exe` (file offset = RVA), read with a byte scan and capstone. Date: 2026-11-18.
Consumer: `plugins/potion-counter` (issue #557). Nothing is hooked; every address is found by signature at load.

The belt items come from the SDK (`InventoryService::getLocalPlayer`, `forEachInventoryItem` with the Belt container
bit), not from native code. In game each belt item's `ItemInfo::x` is its slot, row * 4 + column (log: `slot:item 4:hp5
0:hp1 8:hp5 ...` for a 3-row belt), so the column is `x % 4` and the row `x / 4`; `ItemInfo::code` gives the kind.

| RVA | Signature | Contract | Evidence | Guard / failure |
|---|---|---|---|---|
| `0x846170`, `0x856220` | player-bars' FindPanel and FindChild signatures (1 match each in the dump) | find a panel by name, a direct child by name | [player-bars.md](player-bars.md) | any missing: the plugin stays off, logged |
| `0x8580d0` | `0F B6 41 ?? 88 51 ?? 3A C2 74 ?? 48 8B 01 84 D2 74 ?? 48 FF 60 ?? 48 FF 60 ?? C3` (1 match); the byte at +3 is the flag offset, `0x51` | `Widget::SetVisible(widget, bool)`: stores the byte at `widget+0x51`, then calls the vtable's show (`+0x30`) or hide (`+0x38`) | the function body; it is the shared entry in 125 widget vtables | missing: the plugin stays off, logged |
| `0x228e47` (BeltWidget update) | `48 8B CB 4C 8B E8 E8 ?? ?? ?? ?? B9 09 00 00 00 41 89 86 ?? ?? ?? ??` (1 match); the last 4 bytes are the field, `0x6a4` | `mov [r14+0x6a4], eax` after `0x15f140(unit)`: the Belt widget keeps the belt's box count (belts.txt numboxes) at Belt`+0x6a4`; the update shows `max(1, (boxes + 3) / 4)` rows | the function body; in game 12 for the Belt, 16 for a Demonhide Sash, 4 for no usable belt | missing: 4 squares per column, logged |
| `0x228e6b` (BeltWidget update) | `B9 1A 00 00 00 E8 ?? ?? ?? ?? 84 C0 74 04 B0 01 EB 08 49 8B CE E8 ?? ?? ?? ?? 41 88 86 ?? ?? ?? ??` (1 match); the last 4 bytes are the field, `0x6a0` | `mov [r14+0x6a0], al`: the belt is popped up. False when UI state 9 is open (`0xce500(9)`); true when UI state 0x1a (the belt toggle, the `` ` `` key) is open; otherwise true while the mouse is on the belt (`0x876180`: the focused widget is this one). The rows pop up only for a belt of more than 4 boxes | the function body; in game it toggled with the `` ` `` key and with the mouse on the belt, for a 3-row and a 4-row belt | missing: the squares stay while the belt is popped up, logged |
| widget `+0x70` | layout | x, y, width, height in layout units, local to the parent | `SetPosition` `0x856fb0` writes `+0x70`/`+0x74`; player-bars reads the same field | none (the same field three plugins use); the plugin also writes it to place and size its squares |

## The BeltWidget (`D2Client\src\UI\Widgets\BeltWidget.cpp`), for reference

The plugin does not use these reflected fields; it reads the hotkey labels the widget builds instead.

- Vtable `0x1ce2fa0`: `+0x8` layout build `0x227fd0`, `+0x10` update `0x228db0`.
- Reflected fields (registration at `0x227e23`): `itemOffsetInCell` `+0x630`, `cellPadding` `+0x638` (IntPoint each);
  the layout build reads the cell size at `+0x5b8` and `cellOffsetInRow` at `+0x628`.
- The layout build clones the `HotkeyLabelTemplate` child into `HotkeyLabel0`-`3` and moves each to the template's
  position plus `cellOffsetInRow + column * (cellSize.x + cellPadding.x)` (`0x856fb0`). The plugin reads those labels'
  `+0x70`, so its squares follow any layout's belt geometry.
- The widget also supports a `QuantityLabelTemplate` child (cloned to `QuantityLabel0`-`3`): its update `0x228db0`
  counts the column's items and sets the text from the string `BeltQuantityFormatString` with (count, rows). Stock
  layouts have no such child. A plugin cannot add one to HUDPanel without replacing the HUD layout (the SDK's child
  layouts take only the player inventory), so the plugin draws its own panel.

## How the squares are drawn

The plugin registers four layouts through the SDK (Resource and Panel services), one per belt row count, and opens the
one for the live row count. Each is a Panel at `priority` 101 (HUDPanel is 100), one square high, so it covers none of
the belt slots and clicks on the belt reach the game. Squares are 40 layout units for 1 or 2 rows, 38 for 3 and 28
for 4, centred over each cell.

The squares hide while the belt is popped up (Belt `+0x6a0`, above), so they never sit on the popped rows.

Each square is a `ClipViewWidget` (it clips its children's drawing to its rect) the square's width and as tall as one
cell of the frame art. Inside it, in draw order:
- An `ImageWidget` of `PANEL/HUD_02/belt_popped_row`, the art the BeltWidget draws for its popped rows (its
  `poppedRowFilename` in hudpanelhd.json): 493 x 113, four framed slots with their dark fill. At `rect.scale`
  square / 123.25 the clip shows only the first slot, the game's own belt slot frame. The bottom row's slots are part of
  `front_panel` and have no sprite of their own.
- A gold `RectangleWidget` mark for an item with no stock picture.
- One `ImageWidget` per stock belt item, `"filename": "items/misc/potion/<name>"` (or `scroll/`),
the same `.sprite` the inventory shows (`data/hd/global/ui/items/misc/`, mapped from `data/hd/items/items.json`), at
`rect.scale` (height - 6) / 98. A layout ImageWidget can show any such sprite; there is no native call to change its
file at run time, so the plugin keeps one per item and shows the current one with `Widget::SetVisible`. A mod that
gives a stock code other art in items.json still gets the stock picture.

## Validation

- Dump: each signature above matched once.
- In game (slot 10, vanilla data through the empty D2RToolsTest mod, level 99 Barbarian, Rogue Encampment,
  2026-11-18): the log line `on the belt: panel at 250, -167; cells at x 12 137 262 387` matches
  hudpanelhd.json (HUDPanel -1454, -412; Belt 1704, 278; labels 2 + 12 + 125 * column, 60 + 6). The squares drew on
  the HUD only with the panel's `priority` 101; at the default they were hidden behind the HUD art. Left- and
  right-clicks on the belt slots worked with the panel open (potion to the cursor, potion drunk). The item pictures
  drew at the expected size (98-unit sprite at the given `scale`); `rect.scale` scales from the widget's top-left.
- Not validated: the controller HUD, a mod with its own HUD layout, a state where the HUD is hidden while in game.
