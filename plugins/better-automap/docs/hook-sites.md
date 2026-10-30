# Hook sites and evidence (D2R 3.3.0 build 93847, image `0x6ab3782c`)

Every address the plugin uses, with its signature, the bytes checked at install and how it was found. Source: the
in-memory dump `D2RLoader_6ab3782c_dump.exe` (sections at their RVAs, D2RLoader 1.3.1 running, so its own patches
are visible). Found with capstone over the dump, `.pdata` for function bounds and chained unwind info for split
functions. The toolkit's registry (`knowledge/d2r-93847-addresses.md`) had no automap, mini map or wheel entries.

Status words: **verified** = read in the disassembly; **inferred** = behaviour deduced from code, not yet seen in
game.

## What D2R does (the model the plugin relies on)

- One panel draws both maps: `AutoMapPanel` (layout `automaphd.json`, panel id 10). The "Automap Size" setting
  (`AutoMapMode`, 0..2) picks full screen (0), mini map left (1) or mini map right (2). There is no separate
  mini map widget, so the mini map is never drawn while the full-screen map is up.
- `AutoMapPanel` layout fields (reflection table at `0x29ece60`, field offsets in the panel): `tileScale` +0x15a8,
  `miniTileScale` +0x15ac, `miniScreenSizeFraction` +0x15b0, opacities +0x15b4..+0x15c0, `miniOffsetLeft` +0x15c4,
  `miniOffsetRight` +0x15cc. **verified**
- `AutoMapPanel` vtable `0x1fdc8a8`: [2] Update `0x14e48b0`, [3] Draw `0x14e3480`, [4] OnMessage `0x14e4470`,
  [6] `0x14e48a0` (re-layout). **verified**
- D2RLoader/D2RCore already replaces some automap code: the automap key handler, the END key reset and the panel's
  base Update jump through `.maho` thunks (`exe+0x3e2b16c`, `exe+0x3e2b148`, `exe+0x3e2b83e`) to code outside the
  image. None of those is patched here; the key handler is only wrapped through its table slot. **verified**

## Sites

### `0xd34b0` MinimapOnLeft (minimap part, byte patch)

`bool MinimapOnLeft(void)` = `movzx eax, byte ptr [0x2a2add5]; ret`. Whole function, 8 bytes:
`0F B6 05 1E 79 95 02 C3`. **verified**

- Only caller: `0x14e48f5` in the mini-map layout `0x14e48e0`, which picks
  `left = MinimapOnLeft() || ScreenShift == 1`, then anchors x at 0.0 + `miniOffsetLeft` (left) or 1.0 +
  `miniOffsetRight` (right) and sizes the panel from `miniScreenSizeFraction`. **verified**
- `0x2a2add5` is written only by the setting sync `0xd34c0` / change callback `0xd3d80` (`= mode == 1`) and the
  Toggle Minimap key handler `0xd4300` (key table entry 45, `CfgToggleminimap`), which flips it. **verified**
- Patch: `B0 01 C3 CC CC CC CC CC` (left) or `32 C0 C3 CC CC CC CC CC` (right). The caller only tests `al`.

### `0x14e3480` AutoMapPanel::Draw (zoom part, inline hook)

`void AutoMapPanel::Draw(AutoMapPanel* this) const` (string `void __cdecl AutoMapPanel::Draw(void) const`).
Entry, 20 bytes / 8 instructions, no RIP-relative operand: `4C 8B DC 55 53 56 57 41 54 49 8D 6B A1 48 81 EC E0 00 00 00`.
**verified**

- If `0xd3480()` (`AutoMapMiniFlag == 1`): mini map, scale = uiScale * `this+0x80` * `miniTileScale`. Else full
  screen centred on the screen (shifted by a quarter width when a side panel is open, `0xb9b80`), scale = uiScale *
  `this+0x80` * `tileScale` (`mulss xmm6, [rdi+0x15a8]` at `0x14e3724`), then `0xd2600` draws. **verified**
- `+0x15a8` is read nowhere else in AutoMapPanel code (FindDisp over `.text`: constructor default 1.0 at `0x14e3400`,
  reflection at `0x14e3b31`, Draw). The hook scales it only for the duration of the call. **verified**

### `0x14e4470` AutoMapPanel::OnMessage (zoom part, inline hook)

`uint64 AutoMapPanel::OnMessage(AutoMapPanel* this, Message* msg)`. Entry, 19 bytes / 6 instructions, no
RIP-relative operand: `48 89 5C 24 18 48 89 74 24 20 55 57 41 56 48 8D 6C 24 F0`. **verified**

- Handles `InputMessage`/`KeyDown` (arrow keys 0x25..0x28 pan via `0xd3d00/0xd3d10/0xd3d20/0xd3d30`, END via a
  D2RCore thunk) only when `0x8460d0()` (input already handled) is false, then calls `0x843fc0()` (mark handled);
  handles `SystemMessage`/`ResolutionChanged`; then returns `Panel::OnMessage` (`0x856b00`). **verified**
- Message layout: `+0` type hash, `+0x88` subtype hash, `+0x110` payload. Hashes are FNV-1a 64 (`0x80d70`; its
  per-name seeds equal FNV-1a of the first character): `InputMessage` `0x4ae067c6248b6042`, `MouseScroll`
  `0x87a68230a588578d`. **verified**
- `MouseScroll` payload (from its producer `0x840dc0` and the consumer `ScrollControllerWidget` `0x894890`):
  `+4` x, `+8` y, `+0x10` int wheel delta divided by 120.0 (`0x1d6180c`). **verified**
- That `AutoMapPanel` receives `MouseScroll` like it receives `KeyDown`: **inferred** (both are `InputMessage`s from
  the same dispatcher `0x843d90`; the status command counts wheel events seen to confirm it in game).

### `0x22a79d8` key table slot 7, CfgAutoMap (overlay part, table write)

Key command table `0x22a7930`, 0x18-byte entries `{down, up, flags}` indexed by the classic keyconfig id. The
key-name list built by `0x14d6860` starts in the same order (0 CfgCharacter, 1 CfgInventory, 2 CfgParty,
3 CfgMessageLog, 4 CfgQuestLog, 5 CfgChat, 6 CfgHelp, 7 CfgAutoMap, 8 CfgAutoMapCenter, 9 CfgAutoMapFade,
10 CfgAutoMapParty, 11 CfgAutoMapNames) but not after that: entry 12 is the skill tree (`0x119f00`, panel 4) and
CfgToggleminimap's handler (`0xd4300`, flips the mini map side) is entry 45.

- Entries 0..11 checked against their handlers: 0 = `0x1196f0` toggles panel 2 (character), 5 = `0x119c90` toggles
  panel 5 (chat), 6 = `0x119ce0` toggles panel 0x1b ("Help Menu"), 8 = `0x119da0` opens panel 10 if closed and
  cycles AutoMapMode 0 -> 1 -> 2 -> 0, 9 = `0x119e50` cycles AutomapFade (`0xe0380`), 10 = `0x119e90` toggles
  ShowParty (`0xe2280`), 11 = `0x119ec0` toggles ShowNames (`0xe2160`). **verified**
- Entry 7 is the automap key (Tab): **inferred** from the classic keyconfig id, the verified neighbours 0..11 and
  the name list. Its handler is not in the image, so its behaviour cannot be read.
- Slot 7 `down` holds `exe+0x3e2b16c`, a `.maho` thunk (`jmp [exe+0x3e2a678]`) to D2RCore. **verified**
- Invokers: key down `0x11ff30` calls `entry->down()` (`call [r14+rcx*8]` at `0x12036f`, rcx = index * 3, return
  ignored) only after the key was offered to the UI as an `InputMessage` and no panel marked it handled. **verified**
- Write: `PatchWriteU64(0x22a79d8, expected = exe+0x3e2b16c, value = wrapper)`. The wrapper forwards rcx..r9 to the
  thunk for every press it does not handle.

### `0xd3d80` ApplyAutoMapMode (overlay part, called)

`void ApplyAutoMapMode(int32 mode)`: the change callback of the `AutoMapMode` setting object `0x2a30d50`
(registered by the accessor `0xe06a0` at `0xe077c`, slot `0x2a30dd0`). Sets `0x2a2add5 = (mode == 1)`,
`0x2a2add0 = (mode != 0)`, calls `0xd6920` (tileset scales for the new size) and clears the pan `0x2a2cf80`.
Not called directly anywhere in D2R; D2R calls it when the option changes. Does not write the saved setting
(the setter is `0xd2f020`, getter `0xd2e490`; int setting fields: +0x30 default, +0x34 min, +0x3c max). **verified**

### Called helpers and globals

| RVA | What | Status |
|---|---|---|
| `0x8460d0` | `bool InputHandled()`: `[0x3440170]+0xb8` | verified |
| `0x843fc0` | `void MarkInputHandled()`: sets that byte | verified |
| `0x2a2ada0` | `u8[0x30]` panel open flags, read by `0xce500(id)`; toggled by `0xcde00(id, ...)` | verified |
| panel 10 | automap (`0x119da0` opens it before cycling the automap size) | verified |
| panel 5 | chat (`0x119c90`, CfgChat, toggles it) | verified |
| `0x2a2add0` | `u32` 1 = mini map, 0 = full screen (getter `0xd3480`) | verified |
| `0x2a2add5` | `u8` 1 = mini map left (getter `0xd34b0`) | verified |
| `0x2a26bb0` | `u32` screen shift for open side panels: 0 none, 1 right panel, 2 left panel, 3 both (setter `0xb9c20`, called by panel open/close `0xcd7c0`, `0xcde00`, `0xcd5d0`) | verified |

## Not used, and why

- **Layout JSON (`automaphd.json`)**: the side is chosen in code (`0x14e48e0`), the offsets are relative to that
  anchor, and a game-wide plugin cannot ship a mod's layout file.
- **Windows message hook (WM_MOUSEWHEEL)**: the game's UI already turns the wheel into `MouseScroll` and tracks
  which panel consumed it; taking it at the panel keeps that.
- **The automap key handler itself**: D2RCore's code, outside the image. Only its table slot is wrapped.
