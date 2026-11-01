# Player Bars (D2R 3.3 build 93847, image `0x6ab3782c`)

Found 2026-11-01 with the dump `D2RLoader_6ab3782c_dump.exe` and Ghidra; used by `plugins/player-bars/src/bars.cpp`.
All functions are called, none hooked. Everything is found by signature; the RVAs are this build's values. Each
signature was checked unique over the image's code and resolved to these RVAs in game. When a required signature is
missing, the part logs why and stays off; the screen shift, text and HUD art functions are optional.

| RVA | Signature start | Contract | Evidence |
|---|---|---|---|
| `0x14cc8c0` | `40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9` + 132 bytes, rel32s wildcarded | ExpBarWidget update; its calls at +0x1b, +0x22, +0x8a give the next three | [better-xp-bar.md](better-xp-bar.md) |
| `0x8b2d0` | from `0x14cc8c0` | `() -> u32` local slot | same |
| `0x9a480` | from `0x14cc8c0` | `(slot) -> Unit*` or null | same |
| `0x2f5020` | from `0x14cc8c0` | `(unit, stat, layer) -> i32` total; D2RCore thunk, call only | same |
| `0xb9c20` | `48 83 EC 38 44 8B 0D ?? ?? ?? ?? 41 8B C1 99 89 0D ...` | screen shift setter; `mov [rip+x], ecx` at +0xf gives `0x2a26bb0` | decompile |
| `0x846170` | `48 8B D1 48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 85 ...` | `FindPanel(name)`; numbers and HUD art | better-xp-bar.md |
| `0x856220` | `48 89 5C 24 10 ... 48 8B 59 58 48 8B F2 48 8B` (24 bytes) | `FindChild(parent, name)`; numbers and HUD art | better-xp-bar.md |
| `0x86dd50` | `40 53 48 83 EC 20 4C 8B 42 08 48 8B D9 48 8B 12 ...` (24 bytes) | `SetText(box, {ptr, len})`; only for `show_text` | better-xp-bar.md |
| `0x8562f0` | `48 8B C4 48 89 70 20 57 48 81 EC A0 00 00 00 ...` (32 bytes) | `(widget, int32 out[2])`: screen position | decompile |

Stats: 6 life, 7 max life, 8 mana, 9 max mana, all `value << 8`. Reads run on the UI thread (`ThreadService`).

## Screen position

`0xb9c20(shift)` moves the view by `[0x22a0d70] / 4` away from an open side panel. At 1600x900 that global gives 300 px,
4:3 of the height, so the shift is a third of the height, 720 layout units; the plugin's panels are anchored at the
screen centre and offset by that. Confirmed in game at 1600x900 and 1720x720 with the inventory and character panels
open. Shift values: 0 none, 1 right panel, 2 left panel, 3 both (view centred). No world-to-screen projection is needed
for the local player.

## OverlayService still draws nothing

Retested 2026-11-01 under D2RLoader 1.3.1: the frame callback runs and draw calls return success, but nothing appears,
as in [better-xp-bar.md](better-xp-bar.md). No `d2rloader.toml` setting changes this. The plugin uses the callback only
as a clock and draws with plugin panels (`RectangleWidget` rows, one per fill step, shown with `setWidgetVisible`).
Those panels pass clicks through to the game (seen by walking in town) and do not dim the screen.

## Orbs

`hudpanelhd.json` names them `HealthBall` and `ManaBall` (`AttributeBallWidget`) under `HUDPanel`.
`WidgetService::setWidgetVisible` hides the liquid and it stays hidden, with no hook.

The black globes, the statues and the frame behind the belt and skill buttons are one raw RGBA image, `Overlay`
(`ImageWidget`, `PANEL/HUD_02/front_panel`, 2822 x 415). `ResourceService` only takes plugin-namespaced paths, so the
sprite cannot be replaced. Instead:

- `AbstractImageWidget::DrawFrame` `0x858c50` passes the widget's `+0xac` rect (x, y, width, height, screen pixels) to
  the sprite draw as a clip. `ProgressBarWidget` sets the same field in `0x85a190`. Writing it on the stock widget keeps
  the draw order behind the buttons. The plugin shows sprite x 608..2238, the frame without the globes.
- The screen position comes from `0x8562f0(widget, out)`. Layout scale = (screen width / 2 - x) / 1420, the image's
  stock distance left of centre. Written every UI tick while the bars show. Confirmed at 1600x900 and 1720x720.

## Clicks beside the frame

Clicks in `BGClickCatcher`'s rect (panel x 70..2844, y from 195) do not walk, on the stock HUD too. Measured in slot 2
by logging the player's path position around each click, 2026-11-01:

| Change | Result at the sides of the rect |
|---|---|
| Stock HUD, or art clipped | blocked |
| Catcher `setWidgetVisible(false)` | blocked: it keeps taking clicks |
| Orbs hidden, disabled or moved off screen | blocked: not the orbs |
| `HUDPanel` rect shrunk to 1 x 1 | blocked: not the panel rect |
| Catcher rect narrowed at `+0x70`/`+0x78` to x 642, width 1630 | walks; the frame still blocks |

The plugin narrows it, only while it still has the stock x 70 and width 2774, so a layout that changed it is left
alone. Widget rect: `+0x70` x, y, `+0x78` width, height (`0x8562a0`). The exact path that stops a hidden catcher's
clicks was not traced; `0x83fc0` is the left-button handler past it, and `0x8f3d0` only converts coordinates.
