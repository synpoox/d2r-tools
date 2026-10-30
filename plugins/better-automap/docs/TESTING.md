# Better Automap test checklist

Test in a copy of the game (the toolkit's `testing/` game slots), never the live install. Restart the game after
every config or DLL change. After each run, read the plugin's lines in the D2RLoader log and run the console
command `better-automap`: it prints the image check, the config file read, each part's install line and live
counters.

Setup: copy `better-automap.dll` into `<copy>\d2rloader\plugins\`. Start once, then edit
`<copy>\d2rloader\config\better-automap.toml` (or put a `better-automap.toml` next to the DLL).

| # | Test | Steps | Expected |
|---|---|---|---|
| 1 | Load, right image | Start through D2RLoader, enter a game, run `better-automap` | Image `0x6ab3782c ... match`; `minimap on`, `overlay on`, `zoom on`; no warnings in the log |
| 2 | Default position (top right) | Defaults. Options > Gameplay > Automap Size: **Mini Map** (either side). Open the automap (Tab) | Mini map at the **top right** |
| 3 | Option and key no longer move it | As 2, switch the option between the two mini map sizes, press the Toggle Minimap key | Mini map stays top right |
| 4 | Right panel open | As 2, open the inventory | Mini map moves to the top left while the inventory is open (D2R's own behaviour), back to the right when closed |
| 5 | Left position | `[minimap] position = "left"`, restart, as 2 | Mini map at the **top left** |
| 6 | Overlay on | Mini map shown. Press Tab | Full-screen map; **no mini map** drawn; `overlay up, 1 opened` |
| 7 | Overlay off | Press Tab again | Mini map back on its side, same size and opacity as before; `1 mini map restored` |
| 8 | Overlay closed another way | Mini map, Tab (overlay), Escape (or the option changes the size), then Tab | Next Tab shows the mini map (vanilla state), not the overlay |
| 9 | Full-screen size unchanged | Automap Size: **Full Screen**. Tab, Tab | Map opens and closes as in vanilla; `presses passed on` counts them |
| 10 | Saved option untouched | After 6, quit to the menu, look at Options > Automap Size | Still the mini map size the player chose |
| 11 | Zoom in / out | Overlay up, no panel open. Wheel up a few notches, then down | Map grows by `step` per notch, then shrinks; `wheel events used` counts up |
| 12 | Zoom limits | Wheel up past `max`, down past `min` | Stops at `max` / `min` (default 2.5 / 0.5); status shows the clamped value |
| 13 | Zoom on the mini map | Mini map shown, wheel | Mini map size does not change; wheel works as in vanilla (`passed on` counts) |
| 14 | Wheel passthrough: inventory | Overlay up, open inventory, wheel | No zoom; wheel does what vanilla does |
| 15 | Wheel passthrough: stash | Overlay up, open the stash, wheel (also over the tabs) | No zoom |
| 16 | Wheel passthrough: chat | Overlay up, open chat (Enter), wheel | No zoom; chat scrolls as in vanilla |
| 17 | Wheel passthrough: menus | Overlay up, Escape menu / Options, wheel on a list | No zoom; lists scroll |
| 18 | Wheel passthrough: no overlay | Automap closed, wheel | Vanilla behaviour, no zoom |
| 19 | Each switch off | One at a time: `[minimap] enabled = false`, `[overlay] enabled = false`, `[zoom] enabled = false` | That part's line says `off: [...] enabled = false`; that feature behaves as vanilla; the other two still work |
| 20 | Off-file | `better-automap-off.txt` next to the DLL with `zoom` | `zoom off: left out by better-automap-off.txt`; others on |
| 21 | Bad config | `position = "middle"`, `min = 0` | Log warns, defaults used, no crash |
| 22 | Wrong build | Load with another D2R/D2RLoader build (or a test build with a different `ExpectedImage`) | Log: `not the expected game image`; all three parts `off: game image differs`; game plays as vanilla; console command still answers |
| 23 | No crash | Play 15 minutes with all parts on: change areas, town portals, resize the window | No crash; counters keep counting |

Report: for each row, pass / fail, plus the `better-automap` command output and the plugin log lines.
