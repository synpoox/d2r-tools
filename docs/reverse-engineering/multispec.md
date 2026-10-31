# Player skills, skill points, respec, client: build 93847

**Build 93847 only**: D2R 3.3, D2RLoader 1.3.1, image PE timestamp **`0x6ab3782c`**. RVAs. Found for MultiSpec by
static analysis of an in-memory dump of the owner's own install (`D2RLoader_6ab3782c_dump.exe`, memory layout, file
offset = RVA; sections, stamp and three registry entry-byte sequences checked against the toolkit's
`d2r-93847-addresses.md`). Tool: capstone 5 over the dump plus a `.pdata` function index (rea's Ghidra provider was not
installed). Nothing here has run in game yet: "static" = read from code, "in game" = also seen working.

Fast lead that found most of this: D2R ships its **built-in test cases**. Their `BC_ASSERT:` strings name engine
functions (`SkillsGetLevel(player, skill, false) == 1`, `UnitIsInTown(player)`, `ClientGetControlUnit(hClient) ==
hPlayer`). Path: string -> 8-byte pointer to it in `.data` (assert descriptor) -> the small stub that loads the
descriptor (`lea rcx, [rip+desc]`, then `jmp` to the assert handler) -> the caller of the stub = the test body.

## Already hooked by D2RLoader 1.3.1 (`FF 25` at entry)

Call them normally; **never inline-hook them**, and do not expect their original entry bytes.

| RVA | Function |
|---|---|
| `0x2f5020` | stat total (unit, stat, layer) |
| `0x2f48c0` | stat base (unit, stat, layer) |
| `0x2f34f0` | add stat (unit, stat, value, layer) |
| `0x33d1e0` | `SkillsGetLevel` (below) |

Bytes after the 6-byte jump stay as in the dump: `0x2f5020` and `0x2f48c0` continue `90 90 90 90 48 89 74 24 20`,
`0x2f34f0` `57 41 56 48 83 EC 40 45 0F B7 F1`, `0x33d1e0` `48 8B F9 48 85 C9 75 1B`.

## Units and clients (server side)

| RVA / offset | Signature | Meaning | Evidence |
|---|---|---|---|
| `0x34b9d0` | (unit) -> i32 | unit type (0 player, 1 monster) | gates player-only paths in `0x4360f0`, `0x48fde0`, `0x436bd0` |
| `0x34b240` | (unit) -> PlayerData* | player data = unit +0x10 (asserts the unit is a player) | disassembly |
| PlayerData +0x00 | char[] | **character name** | passed as the `Character: %s` argument of the "Wrong count" log in `0x4b3ee0` |
| PlayerData +0xf0 | Client* | the player's server client | `0x48fde0` |
| `0x48fde0` | (unit) -> Client* | unit -> client (null unless player) | disassembly |
| unit +0xd8 | Game* | the unit's game; getter `0x48ff00` | disassembly; spend function passes it on |
| `0x484c90` | (client, 0) -> unit | **`ClientGetControlUnit`**: looks the unit up by type/id (client +0x26c / +0x270) in client +0x2b0's game via `0x48fe80`; 0 if client +0x278 is null or flag +0x4e0 bit 0 is set | test `ClientGetControlUnit(hClient) == hPlayer` (`0x5989c0`) and `player == ClientGetControlUnit(client)` (`0x4b60e0`) both call it |
| Client +0x2c | char[] | character name | `%s` argument (`lea r8, [r14+0x2c]`) in `0x484550`'s load-error log (`ClientAddPlayerToGame() Error Loading:%s`) |
| Client +0x2b0 | Game* | client's game | `0x484c90` |
| Game +0x106 | u8 | dataFlag (matches the registry) | `0x4b3ee0`, `0x4360f0` |

## Town

`UnitIsInTown(unit)` is **`0x2f0750(0x34b440(unit))`**: room of unit, then room -> "in town" (room +0x18 -> level
`0x360fc0` -> `0x327850`). Evidence: D2R's own test for `BC_ASSERT: UnitIsInTown(player)` (`0x25a100`) does exactly
this. Same pair as the registry.

## Skills on a unit

| RVA / offset | Signature | Meaning | Evidence |
|---|---|---|---|
| unit +0x100 | SkillList* | getter `0x34b6e0` (unit) | disassembly |
| SkillList +0x00 | Skill* | first skill; `0x34b720` (unit) also returns a first skill (used by add-skill) | `0x33dcd0`, `0x33a010` |
| Skill +0x00 | SkillsTxt* | record; record +0x00 i16 = skill id | `0x33dcd0` compares `movsx [rec]` with the id |
| Skill +0x08 | Skill* | next | list walks |
| Skill +0x10 | i32 | set from record +0x30 / +0x31 at creation (mode?) | `0x33a010` |
| Skill +0x40 | i32 | **hard (base) level** | `0x33d1e0` reads it as the base; `0x33a010` increments it |
| Skill +0x4c | i32 | owner item id, -1 = the unit's own skill | lookups compare with -1 |
| `0x33dcd0` | (unit, skillId, itemId -1) -> Skill* | `SkillsGetSkill` | test body `0x260590`: `0x33dcd0(player, 6, -1)` then `0x33d1e0(...) == 1` (`SkillsGetLevel(player, skill, false) == 1`) |
| `0x33d1e0` | (unit, skill, bonus, ?) -> i32 | `SkillsGetLevel`; bonus 0 = +0x40 only, else adds `0x3400a0`; clamped to the max from `0x300c70`. **D2RLoader-hooked** | same test; assert `SkillsGetLevel(hUnit, hSkill, TRUE, true)` |
| `0x33a010` | (unit, skillId) -> Skill* | `SkillsAddSkill`: +1 level (to max `0x214220`), or creates the skill at level 1 and appends it; applies the passive state (record +0xb4) | disassembly |
| `0x33cb30` | (dataFlag, class 0-7) -> count | number of class skills (data tables +0x11d0 per-class lists) | `0x4360f0` loop bound |
| `0x33dde0` | (dataFlag, class, index) -> skillId | the class's index-th skill: **the order of the save's "if" bytes** | `0x4360f0` loop |
| `0x97790` | (dataFlag, skillId) -> SkillsTxt* | record getter (registry); 0x2ec bytes | |
| SkillList +0x08 / +0x10 | Skill* | left / right mouse skill | setters `0x33ec70` / `0x33ef10`; right getter `0x34b400` via `0x33db60` (static) |
| `0x33a660` | (unit, skill) -> nonzero | usable: hard level plus bonuses above 0 | decompiled; `0x474aa0` and the select handler test it first |
| `0x438a70` | (unit, isLeft, skillId, itemId) | **server skill select**, also starts or ends a right-hand aura and tells the client; skill 5 is ignored. Details in [better-auras.md](better-auras.md) | select packet handler `0x4b4250`; `0x474aa0` restores with it (static) |
| `0x34d180` / `0x34d220` | (player) | store the left / right skill in player data (+0xc8 id, +0xd0 item for left), which `0x474d60` re-selects after item changes | called after `0x438a70` in `0x4b4250`; readers `0x34cec0` / `0x34cf60` (static) |

Skills record offsets found here (add to the registry's list): **+0x2c i8 charclass** (`0x436bd0`), +0x184 i16 maxlvl
(0 -> 20) (`0x4b3ee0`), +0x210 i32 skpoints formula (-1 = cost 1) (`0x438670`), +0x216 / +0x218 i16 requirement fields
compared with stats 0 / 2 (`0x339c00`), +0x21e i16 first prerequisite skill (`0x339330`), +0xb4 passive state.

## Skill points (stat 5 `newskills`)

`STATS_NEWSKILLS` = **5**: the skill-up test (`0x260590`) asserts `StatsGetCurrentStat(player, STATS_NEWSKILLS) == 0`
with `0x2f5020(player, 5, 0)`. Grants seen: level-up `0x52da30` (charstats SkillsPerLevel x levels), quests
`0x5193ee` (+1, +1, +2), others `0x412f60`, `0x580750`, `0x5de448`, `0x5e6d70`.

## Spending a point (server)

| RVA | Signature | What it does |
|---|---|---|
| `0x4b3ee0` | (game, player, packet, size 5) -> 1 | client "add skill" packet handler. Packet `{u8 cmd, u16 skill, u16 count}`. Per point: skill < count of skills, **`0x436bd0`** (player, skill) class check (record +0x2c == player class), **`0x339330`** (player, skill) level and prerequisite skills, **`0x339c00`** (player, skill) stat requirements, hard level < maxlvl, then `0x438670`. Logs "Wrong count" when no point was spent |
| `0x438670` | (dataFlag u8, player, skillId) -> 1 ok / 0 | **spend one point, no prerequisite checks**: cost = skpoints formula (record +0x210, `0x3b5160`) or 1; refuses when `newskills` (base, `0x2f48c0`) < cost; `newskills -= cost` (`0x2f34f0`); `0x33a010` add level; `0x33f830(player, 1)` refresh; passive callbacks; **`0x47f330`** sends the new level; states (`0x337720`, `0x3354c0`, `0x337890`). Called by `0x4b3ee0`, `0x433c19`, `0x475e6c` |
| `0x47f330` | (client, player, u16 skill, u8 level, i32 flag) | sends a skill's level to the client (`0x4360f0` passes level 0, flag 1; `0x438670` the new level, flag 0) |

## Respec (skills only)

`0x4360f0` (game, player): returns at once unless the unit is a player. For each class skill i (`0x33cb30`,
`0x33dde0`): skill = `0x33dcd0(player, id, -1)`; if present, total += `0x33d1e0(player, skill, 0, 0)`;
`0x433910(player, id, 0, 1)` removes it (with its auras, passives, states); `0x47f330(client, player, id, 0, 1)`.
Then `newskills += total` (`0x2f34f0`), `0x46f220(game, player)`, `0x474d20(game, player, 1)`. **Stats are not
touched.** Refund = sum of levels, not of skpoints costs.

Callers: `0x4106c0` (thunk returning 1: an NPC/quest table entry, likely Akara) and `0x580f20` (Token of Absolution:
`0x4360f0`, then `0x52ddf0` stat respec, then `0x470c90`).

## Entry bytes (static, from the dump)

Whole instructions, at least 14 bytes. For a byte check before calling, not for hooks.

| RVA | Bytes |
|---|---|
| `0x2f0750` | `48 83 EC 28 48 85 C9 75 07 33 C0 48 83 C4 28` |
| `0x34b440` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13` |
| `0x34a0e0` | `48 83 EC 28 48 85 C9 75 1A 88 4C 24 30 48 8D 4C 24 30` |
| `0x349860` | `48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C 24 30` |
| `0x34b9d0` | `48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C 24 30` |
| `0x34b240` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13` |
| `0x34b6e0` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 20` |
| `0x33dcd0` | `48 89 5C 24 08 57 48 83 EC 20 41 8B D8 8B FA` |
| `0x33cb30` | `48 89 5C 24 08 57 48 83 EC 30 48 63 DA 0F B6 F9` |
| `0x33dde0` | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20` |
| `0x97790` | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30` |
| `0x436bd0` | `48 89 5C 24 10 57 48 83 EC 20 8B FA 48 8B D9` |
| `0x339330` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18` |
| `0x339c00` | `48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20` |
| `0x438670` | `48 89 5C 24 18 48 89 6C 24 20 57 41 56 41 57` |
| `0x4360f0` | `48 85 D2 0F 84 3D 01 00 00 55 57 48 83 EC 38` |
| `0x4b3ee0` | `40 55 53 56 57 41 55 41 56 41 57 48 8D 6C 24 D9` |
| `0x484c90` | `48 89 5C 24 10 57 48 83 EC 20 8B FA 48 8B D9` |
| `0x48fde0` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1E` |
| `0x48ff00` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 20` |
| `0x47f330` | `48 85 D2 0F 84 D6 00 00 00 53 55 56 57 48 83 EC 48` |
| `0x33a010` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18` |
| `0x33a660` | `40 53 48 83 EC 20 48 8B D9 48 85 D2 74 18` |
| `0x438a70` | `41 83 F8 05 0F 84 28 02 00 00` (10 bytes: Better Auras hooks `0x438a7a`) |
| `0x34d180` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 19` |
| `0x34d220` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 19` |

## Still to verify in game

- Calling `0x438a70` after a swap leaves the buttons right on the client, and what the second weapon set shows.

- The `D2R::Game::Client*` a D2RLoader console callback receives: is it the server client above (so
  `0x484c90(client, 0)` gives the player), and does the callback run on the game thread? Check: the round trip
  `0x48fde0(0x484c90(client, 0)) == client` and unit type 0, before anything else.
- Whether a chat line `/spec 2` reaches a registered console command.
- Calling `0x4360f0` and `0x438670` from a plugin (outside the NPC, item and packet paths) leaves the client in sync.

## Skill tree layout (Spike 1, static): build 93847

Source: pristine 93847 cache (CASC), `data/global/ui/layouts/`. Not yet loaded in a game slot.

| File | Size | Notes |
|---|---|---|
| `skillstreepanelhd.json` | 8307 B | HD, keyboard and mouse. Root `SkillsTreePanel`, rect `$RightPanelRect` (1162 x 1507, anchor `$RightPanelAnchor` 1.0 / 0.397) |
| `controller/skillstreepanelhd.json` | 11884 B | `basedOn` the HD file, `$ConsoleCenterPanelRect`, 4 tabs, own widget positions. Out of scope (design section 11) |
| `skillstreepanel.json` | 7757 B | Legacy (non-HD) UI |

Widgets of the HD file, top to bottom (panel coordinates): `Cancel` close button (1075, 9); `Title`
`$InGamePanelTitleRect` (91, 64, 972 x 71); `TreeBackground` `PANEL_SkillTreeBG` at (68, 145); tabs `Tab0-2`
`activationRect` y 160-259 (x 130 / 428 / 724, 294 wide) with `TextTab0-2`; skill icons by the fields `skillRow`
296 / 478 / 654 / 834 / 1014 / 1190 and `skillColumn` 253 / 515 / 775; `SkillCounter` (91, 1388, 972 x 60) with a
`Tooltip`; `CounterTemplate`, `ClickCatcher`, `MainBackground`, `RightHinge` (1076, 630).

Where a bar fits, by coordinates only (the sprite sizes were not read: the cache has no HD sprites):

- **Bottom strip, y ~1448-1500**, below `SkillCounter`: full width (91-1063), free of every stock widget. First
  choice: 5 slots + 2 arrows at about 100 px each fit in 972 px. The panel frame art may cover the last ~30 px.
- Between the last skill row (1190 + icon) and `SkillCounter` (1388): about 80 px if the icons are ~110 px. Inside
  the tree art, so a bar there sits over the background.
- Moving `SkillCounter` up or shrinking it is possible (it is found by name; only its rect changes).

SDK limit found: `registerChildLayout` takes only `StockPanel::PlayerInventory` (PluginSDK `panels.h`, 0.3.0), and a
plugin's resources are limited to `d2rloader/<plugin-id>/` and `ui/layouts/<plugin-id>/` (SDK README, "Resources and
Companion MPQs"). So **the plugin cannot add widgets to the skill tree or replace `skillstreepanelhd.json` itself**. MultiSpec is a
global plugin and ships nothing through the mod, so the spec UI is a plugin panel docked to the tree's left edge
(design section 11). The strips above are kept as layout facts only.

Still to check in a game slot: design section 11, spikes 4-6 (the tree's open and close messages, a right-anchored
plugin panel beside it, placement at other resolutions). Spike 4's probe is the `uiprobe` part (`msprobe` command,
`plugins/multispec/src/ui_probe.*`); steps in `plugins/multispec/TESTING.md`, rows P1-P11.

## Skill tree open and close messages (spike 4, in game): build 93847, D2RLoader 1.3.1

One owner run, 2026-10-30, global plugin with mod Reimagined active, 9038 UI messages logged in about 90 s. Seen by a
`UiMessageListener` (priority 1000):

| Event | Messages, in order |
|---|---|
| Tree opens (key or HUD button) | `PanelManager` `OpenPanel` text `SkillsTreePanel` (6 opens, every one seen) |
| Tree closes (key) | `InputMessage` `KeyDown`, `BindingDown`, then `PanelManager` `ClosePanel` `SkillsTreePanel` |
| Tree closes (its X button) | `SkillsTreePanelMessage` `Close`, then `PanelManager` `ClosePanel` `SkillsTreePanel` |
| Another right panel opened over it (inventory) | `PanelManager` `ClosePanel` `SkillsTreePanel`, then `OpenPanel` `PlayerInventoryExpansionLayout` |
| Escape with panels open | One `ClosePanel` per stock panel (inventory, `DropGoldModal`, `BankExpansionLayout`, `HoradricCubeLayout`, `VendorPanelLayout`, ...) |
| Noise | `MonsterHealth` `ClearInfo` (85%), `InputMessage` `MouseMove` / keys / mouse buttons, `ConsolePanelMessage` `SubmitInput` |

So **`PanelManager:OpenPanel:SkillsTreePanel` and `PanelManager:ClosePanel:SkillsTreePanel` are the open and close
signals**, whatever the route (key, button, X, a panel that replaces it). Match target, command and text; return
`Continue`.

- `dispatchUiAction` `PanelManager` `TogglePanel` `SkillsTreePanel` returns 0 and the listener sees it (twice per
  dispatch) and **the tree toggles on screen** (owner), but **no `OpenPanel` / `ClosePanel` follows**. A plugin that
  toggles the tree this way must open or close its own panel itself: the listener never hears about it.
- `findPanel("SkillsTreePanel")` finds it **open or closed** (same rect -1394, -651, 1162 x 1507 = `$RightPanelRect`),
  so it can't tell whether the tree is open. Only the messages can. The rect is a logged value only: nothing draws it.

## Plugin panel beside the skill tree (spike 5, in game): build 93847, D2RLoader 1.3.1

Run 1, 2026-10-30, test panel `multispec/ProbePanel` (`ui_probe.cpp`): root `Panel`, `anchor` `$RightPanelAnchor`,
`rect` -1564, -651, 170 x 1507, `priority` 8500, flag `CloseOnEscape`, given with `registerResource`.

- Loads and draws: `registerResource` + `registerPanel` succeed; `togglePanel` / `openPanel` / `closePanel` return 0.
  `$` variables (`$RightPanelAnchor`, `$MediumPanelFontSize`) resolve in a plugin layout.
- **Placement works**: the rect lines up with the tree's left edge at the owner's resolution. Widgets in it use
  panel coordinates, as in stock layouts.
- **Opening it doesn't close the tree, and the tree doesn't close it**: both stay open together.
- **Docking works**: an `openPanel` / `closePanel` queued with `runOnUiThread` from the listener on `PanelManager`
  `OpenPanel` / `ClosePanel` `SkillsTreePanel` opened and closed the strip with the tree every time (8 cycles,
  result 0).
- **`CloseOnEscape` takes Escape away from the tree**: Escape closed only the strip, so the tree stayed open. A docked
  panel must not set it.
- **Button clicks did not reach the `UiMessageListener`**: `onClickMessage` `MultiSpec:Probe:<n>` produced no
  message with target `MultiSpec`. Run 2 checks whether a stock message from the same button works
  (`PanelManager:ClosePanel:multispec/ProbePanel`) and logs any message naming `MultiSpec` or `Probe`.
Run 2, 2026-10-30 (icons at x 100 of the 170 strip, no `CloseOnEscape`, a 4th icon sending a stock message):

- **Flush**: the icons sit against the tree's frame (owner screenshot). Content x 100 + ~62 px sprite fills the strip.
- **A button's own target never reaches the listener**: with every message logged, a click on icon 1
  (`MultiSpec:Probe:1`) gave only `InputMessage` `LeftMouseButtonDown` / `Up`. The strip's `PanelManager`
  `OpenPanel` / `ClosePanel` messages do arrive. Run 3 sends `PanelManager:OpenPanel:multispec/ProbeSwap<n>` from
  the icons (the listener matches the text and returns `Consume`).
Run 3, 2026-10-30 (icons 1-3 send `PanelManager:OpenPanel:multispec/ProbeSwap<n>`):

- **Slot clicks work**: each icon gave one `PanelManager` `OpenPanel` with its own text, seen by the listener and
  consumed (`Consume`); nothing opened or closed.
- **A stock `onClickMessage` works from a plugin panel**: icon 4 (`PanelManager:ClosePanel:multispec/ProbePanel`)
  closes the strip.
- **Escape closes both** once the strip has no `CloseOnEscape` (tree first, the dock closes the strip).
- **No click-through**: clicking empty strip space (full-size `ClickCatcherWidget`) moves no character, in town or in
  the field.

Result: a docked plugin panel works. Root `Panel` on `$RightPanelAnchor` at x -1394 - width, no `CloseOnEscape`,
opened and closed from the listener on the tree's `PanelManager` messages, buttons sending `PanelManager:OpenPanel`
with text `multispec/<name>` that the listener consumes. A button's own target (`MultiSpec:...`) is never delivered.
## Strip placement (spike 6, in game): build 93847

Owner run, 2026-10-30, test panel from spike 5 docked, a left panel open as well.

- **4:3**: no overlap. The strip fits between a left panel (quest log) and the tree, with room to spare. The
  prediction (overlap of about 80 if the UI is 2160 units tall at every size) was wrong: D2R's UI space is wider than
  that at 4:3. So the 170-wide strip fits at every ratio tried.
- **21:9**: D2R pulls the left and right panels in from the screen edges, and the strip moves with the tree
  (it shares the tree's anchor).
- Not reported: other 16:9 sizes, 16:10, legacy graphics (G), large font mode.
- The strip has no background, so it looks like it floats; the owner wants a background that matches the panels
  (design section 11, "Strip art").
## Strip art (spike 8, in game): build 93847

Run 1, 2026-10-30, option A (`PANEL\PANEL_BG` in the strip, `priority` -5) plus a 64 x 64 sprite built by the plugin:

- **`priority` sets draw order between panels**: at -5 the strip draws under the tree (the stock tree layout sets no
  priority; the stock help button's -1 is commented "Lower than all right panels"). The tree draws over the part of
  the strip's image that runs under it.
- **A plugin's own sprite works**: `registerResource` at `data/hd/global/ui/d2rloader/multispec/probeswatch.sprite`
  (+ `.lowend.sprite`), `filename` `d2rloader\\multispec\\probeswatch` in the layout: drawn. Built in memory at load
  (header + RGBA, version 31), no file shipped.
- Still drawn with a left panel open too (character sheet).
- Owner's verdict on the look: no. The X icons must be numbered slots, the strip must be only as tall as its slots,
  the heading gold, the slots centred (or the strip narrower). Run 2 draws own art in code (`strip_art.*`) sized to
  the slots.

## Stock sprites for the spec panel (in game): build 93847

Agent run in a game slot, 2026-10-30, D2RLoader 1.3.1. Sizes read from the sprite headers.

- `PANEL\Modals\Modal_No_Title_BG`: 1245 x 453, 1 frame. Same stone and corner ornaments as `PANEL\PANEL_BG`.
- `PANEL\HUD_02\belt_popped_row`: 493 x 113, 1 frame: four empty belt slots.
- `FrontEnd\HD\Final\FrontEnd_UpButton` and `_DownButton`: 70 x 70, 4 frames; pressed 1, disabled 2, hovered 3.
- `NineTileImageWidget` needs a 9-frame sprite, so it can't use `PANEL_BG`. Only `TooltipBorder` and `AdvancedCharacterSheet_Cell` are used with it.
- An `ImageWidget` with `"visible": false` starts hidden, and `setWidgetVisible` shows it after `findPanel` / `findWidget` on `multispec/SpecPanel`.
- `ExecuteConsoleCommand` (D2RCore ordinal 14) ran `spec` and `spec N` from a UI-thread callback, and a plugin `ConsoleCommandHandler` ran with a usable client.
