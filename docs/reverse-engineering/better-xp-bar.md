# RE findings (Phase 2): D2R 3.3 build 93847, image 0x6ab3782c

Source: the static image dump (memory layout, file offset = RVA; see [better-xp-bar-dump-inventory.md](better-xp-bar-dump-inventory.md)) and the
d2rAI-toolkit registry (`knowledge/d2r-93847-addresses.md`). Tool: `tools/re/d2rre.py` (pefile/numpy/capstone;
Ghidra was not available). Every claim below cites the instruction it comes from. Disassembly is quoted only as far as
it is evidence; no bulk decompiled output.

Confidence levels:
- **confirmed in dump**: read directly from the code or data of this image;
- **registry**: listed in the toolkit registry (hooked or called in game by another plugin) and its entry bytes match
  this dump;
- **inferred**: follows from the code but not from one instruction (calling thread, meaning of a flag);
- **needs live check**: only the running game can show it. Each one has a step in TESTING.md (Phase 4).

## Key lead: the stock XP bar update, `ExpBarWidget` (0x14cc8c0)

The only code referencing the assert path `D2Client\src\UI\Widgets\ExpBarWidget.cpp` (string 0x1fd9030) is at
0x14cc8f8 (`lea rdx, [rip+...]`, passed as the file argument of 0x349860 with line 0x23). The `.pdata` entry
around it (0x14cc8d6-0x14cca28) is a chained fragment; the function starts at **0x14cc8c0**
(`40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9`, push rbx / sub rsp,0x50 / cmp byte [rcx+0x168],0 / mov rbx,rcx).
It is not called directly: its only reference is a pointer at **0x1fd8fd0**, in an 8-byte method table at 0x1fd8fc0
(0x14cc1c0, 0x14cc7e0, 0x14cc8c0, 0x14cc3e0, ...) that sits next to the `ExpBarWidget`, `isHireling`,
`TooltipHUDExperienceBar` and `panelexp` strings (0x1fd90c0..0x1fd9100). StaminaBarWidget's table is just before it.

What 0x14cc8c0 does, in order (this is the game's own recipe; the plugin repeats it):

| Step | Instruction(s) | Meaning |
|---|---|---|
| 1 | `cmp byte [rcx+0x168], 0` / `jne` | skip when the widget's byte +0x168 is set (inferred: the `isHireling` property; the hireling bar takes another path) |
| 2 | `call 0x8b2d0` -> `mov ecx, eax` -> `call 0x9a480` -> `test rax,rax / je` | local player unit, may be null |
| 3 | `call 0x349860(unit, file, 0x23)` -> r15d | class id |
| 4 | `xor r8d,r8d / lea edx,[r8+0xc] / call 0x2f48c0` -> r13d | **base stat 12 = level** |
| 5 | `xor r8d,r8d / lea edx,[r8+0xd] / call 0x2f5020` -> r14d | **stat 13 = experience** |
| 6 | `call 0x34a0e0(unit)` -> al | dataFlag (table bank selector) |
| 7 | `call 0x300c70(flag, class)`; `cmp r13d, eax / jge` | max level; no progress at or above it |
| 8 | `call 0x300920(flag, class, level-1)` -> edi; `call 0x300920(flag, class, level)` -> esi | start of this level, start of the next |
| 9 | `cmp edi, esi / jae`; `cmova ebp, ecx` (xp - lo when xp > lo, else 0); `cvtsi2ss` / `divss` | progress = (xp - lo) / (hi - lo), **unsigned 32-bit** compares |
| 10 | `call 0x85a190(this, progress)`; `mov [rbx+0x158], r14d`; `mov [rbx+0x15c], esi` | set the bar; keep xp and next threshold on the widget |

This also confirms the brief's Level % formula against the game's own code.

## (a) Reading the player's XP and level

| What | RVA | Signature | Entry bytes (first 16) | Evidence | Confidence |
|---|---|---|---|---|---|
| Local client slot | `0x8b2d0` | `() -> u32` | `8B 05 2E 84 99 02 C3` (`mov eax,[rip+..]` = global **0x2a23704**; ret) | 7-byte leaf, no `.pdata`; 1,233 call sites | confirmed in dump |
| Player unit for a slot | `0x9a480` | `(u32 slot) -> Unit* or null` | `48 89 5C 24 08 57 48 83 EC 20 83 F9 08 0F 83 85` | `slot >= 8` -> null; id table **0x2a238f0** (8 x i32, -1 = none); lookup `0x9f270(0x2a23910, id & 0x7f, id, 0)`; 1,102 call sites, **1,035** of them right after `call 0x8b2d0` | confirmed in dump |
| Class id | `0x349860` | `(Unit*, const char* file, i32 line) -> i32` | `48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C` | registry *Units*; used at 0x14cc926 | registry + confirmed in dump |
| Base stat | `0x2f48c0` | `(Unit*, i32 stat, i32 layer) -> i32` | `FF 25 4A 59 B3 03 90 90 ...` (**redirected by D2RLoader/D2RCore: call it, never hook it**) | registry *Stats*; level read at 0x14cc938 with stat 0xc | registry + confirmed in dump |
| Total stat | `0x2f5020` | `(Unit*, i32 stat, i32 layer) -> i32` | `FF 25 F2 51 B3 03 90 90 ...` (**redirected: call, never hook**) | registry *Stats*; experience read at 0x14cc94a with stat 0xd; treated as u32 by the bar | registry + confirmed in dump |
| dataFlag | `0x34a0e0` | `(Unit*) -> u8` | `48 83 EC 28 48 85 C9 75 1A 88 4C 24 30 48 8D 4C` | registry *Units*; used at 0x14cc967 | registry + confirmed in dump |
| Stat ids | 12 level, 13 experience | | | immediates at 0x14cc934 / 0x14cc946; toolkit saves.md also names stat 12 = level | confirmed in dump |

Notes:
- Experience is unsigned 32-bit (the bar uses `jae`, `cmova` and a zero-extended `cvtsi2ss rax`). The plugin reads the
  `i32` return and reinterprets it as `uint32_t`.
- The unit pointer is fetched on every read and never kept: units are freed when a game ends.
- **Calling thread: inferred.** The stock bar runs this in a widget update, so the plugin does its reads on D2R's UI
  thread through `ThreadService::runOnUiThread`, never from the overlay draw callback.
- **Character name: not used.** No evidenced offset for it was found in this spike, so no name is read. The plugin's
  character key is evidence-only: class id + XP continuity (a character's XP cannot change between games; a first
  read in a new game with another class or another XP than the last value seen is another character). Known miss:
  two characters of the same class with identical XP (for example two fresh level 1 characters) count as one.
  With the default per-game session scope this rule is not needed (a character switch always leaves the game, which
  ends the session); it only matters for the optional `session_scope = character`.

## (b) The experience table and level thresholds

| What | RVA | Signature | Entry bytes | Evidence | Confidence |
|---|---|---|---|---|---|
| Threshold | `0x300920` | `(u8 dataFlag, i32 class, i32 L) -> u32` = XP at which level L+1 starts | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 33` | see below; 14 call sites | confirmed in dump |
| Max level | `0x300c70` | `(u8 dataFlag, i32 class) -> u32` | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 33` | see below; 45 call sites | confirmed in dump |

Both functions, instruction by instruction:

- `cmp edx, 7` / `cmova edi, eax(0)`: **class > 7 is replaced by 0**: 8 class columns (Amazon..Druid + Warlock).
- `movzx eax, cl` / `cmp rax, 4` / `jb` else `call 0x2fde30` + `int3`: **dataFlag must be < 4** (assert).
- `lea rbx, [rip+..] -> 0x2a9a580`; `add rax,rax`; `mov rbx, [rbx + rax*8]`: data manager of the bank
  = `*(0x2a9a580 + 16 * dataFlag)` (the registry lists 0x2a9a580 as "data managers, 16 bytes apart").
- `add rbx, 0x14e0`: the experience rows vector **{u32* rows, u64 count}** at manager + 0x14e0.
- Threshold: `lea eax, [rsi+1]` (row = **L + 1**), `cmp rcx, [rbx+8]` / `jb` else `call 0x2fd870` + `int3`
  (**out-of-range row asserts and crashes**), `lea rdx, [rax+rax*8]` / `add rdx, rcx` / `mov eax, [rax+rdx*4]`:
  value = `rows[(L + 1) * 9 + class]`.
- Max level: row 0 (`cmp [rbx+8], 0 / ja`), same `* 9 + class`: value = `rows[0 * 9 + class]`.

So each compiled row is **9 u32 = 36 bytes** (8 class columns + one more column, ExpRatio per the toolkit's
tables.md), row 0 is `MaxLvl`, and row L+1 holds experience.txt's row "L" (= the XP at which level L+1 starts), which
matches the toolkit's tables.md/saves.md description. The stock bar uses start(L) = Threshold(L-1) and
next = Threshold(L), and does nothing at L >= MaxLevel.

How the plugin uses it:
- **Never hardcodes a threshold.** It builds the core's table at run time: max level M = Threshold row 0, start(L) =
  rows[L] for L = 1..M.
- To avoid the asserting bounds check it reads the same vector the two functions read (`*(0x2a9a580 + 16*flag)` +
  0x14e0, `{rows, count}`), checks `flag < 4`, `class < 8`, `count >= M + 1` and a non-decreasing table, and refuses
  (part "xp" off, logged) otherwise. Reading memory the game's own function reads is lower risk than calling it with
  a value that might assert.
- **needs live check**: the logged values (flag, class, count, M, start(2), start(M)) and, as a cross-check,
  D2RLoader's `DataTableService` row size for `TableId::Experience` (expect 36) and row count.

## (c) Where the stock XP bar is drawn

| What | RVA | Evidence | Confidence |
|---|---|---|---|
| ExpBarWidget update (computes and sets the progress) | `0x14cc8c0` | above | confirmed in dump |
| ExpBarWidget method table | `0x1fd8fc0` (update at slot 2, `0x1fd8fd0`) | only reference to 0x14cc8c0; adjacent class strings | confirmed in dump (slot meanings other than update: unknown) |
| Progress setter (generic bar) | `0x85a190` `(Widget*, float)` | clamps to [0, 1] (`comiss` with 0.0 and the float 1.0 at 0x1cba904), stores **+0x154**, then recomputes the fill rectangle (`0x8562f0`, `0x8562a0`, scale `+0x80`) | confirmed in dump |
| Tooltip builder | `0x14cc426` (fragment) | references `TooltipHUDExperienceBar` (0x1fd90e0) and `panelexp` (0x1fd90f8) | confirmed in dump |
| Actual drawing | D2R's generic HD widget renderer | ExpBarWidget holds no draw code of its own in the parts read; it feeds a progress value to the shared bar code | inferred |
| The bar's name and rectangle in the HUD layout (`hudpanelhd.json`) | not in the image (game data) | | **needs live check** (WidgetService `findPanel("HUDPanel")` + `getWidgetRect`; the plugin logs which names it finds) |

## (d) Safest way to draw text in the HUD

### Options

| Option | Game addresses | Risk | Verdict |
|---|---|---|---|
| **1. D2RLoader `OverlayService`** (SDK 0.3.0): a frame callback after the game UI with `drawText`, `drawFilledRectangle`, `measureText` | **none** | Loader-owned drawing state, survives device changes; can only fail by being unavailable (then the part is off) | **Recommended** |
| 2. Inline hook on ExpBarWidget update `0x14cc8c0` and write text into the widget | 0x14cc8c0 + unknown text APIs | Hookable in principle (16 bytes of whole instructions `40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9` before the `jne rel32` at +0x10), but putting text into the widget needs text/label APIs this image alone can't confirm, and the hook runs inside UI layout code every frame | Rejected |
| 3. HD layout change (`hudpanelhd.json` TextBoxWidget) | none for the layout, but text must be set from code | WidgetService has no "set text" in SDK 0.3.0; ties the plugin to one mod's layout files and breaks other UI mods | Rejected |

### Recommended approach

- Register one overlay frame callback (`Phase::AfterGameUi`). It only draws cached, pre-formatted lines (a mutex
  guards them); it calls no game function.
- A UI-thread task (queued from the frame callback at most 4 times a second, `ThreadService::runOnUiThread`) does the
  reads of (a)/(b) and updates the core and the cached lines.
- Game join/leave, local-player-ready and level changes come from `LifecycleService` gameplay events (no addresses).
- Hotkeys come from `InputService` actions (they appear in D2R's Controls menu), plus console commands.
- Position: anchored to the screen size of each frame (`Frame::screenWidth/Height`), configurable in the ini.

### Risks (and what is done about them)

| Risk | Mitigation |
|---|---|
| The overlay draws after all game UI, so also over open panels and the Esc menu | Compact block, configurable anchor and offset, toggle hotkey; drawn only while in a game with a valid read. Hiding it under panels needs more RE, left out |
| Text looks different from D2R's font (loader's ImGui-style text) and does not follow the UI scale setting | Text size and background opacity are configurable |
| `OverlayService`/`ThreadService`/`InputService` missing on another D2RLoader build | Each is queried with its size check; a missing one turns its part off with a log line, never a crash |
| Reading game memory from a wrong thread | All game reads happen in the UI-thread task; the draw callback reads only the cache |
| A wrong address on another image | Build guard: all engine parts off unless the image stamp is 0x6ab3782c; the read part also checks the first bytes of every function it calls (`CheckExpectedBytes`) and refuses on a mismatch |
| D2RCore patches applied after the dump was taken | Those byte checks run live; the status command shows the result |

## (e) OverlayService draws nothing on screen (live result, 2026-10-29)

Live test on the owner's install (D2RLoader 1.3.1, D2RCore with ImGui, 3440x1440, Reimagined + 12 plugins): the
frame callback runs every frame, every `drawText` / `drawFilledRectangle` / `measureText` returns `Success`, and
`better-xp-bar test` (opaque full-screen-centre magenta rectangle, 100+ frames) shows **nothing**, with or without
`debug_mode`. D2RCore draws plugin overlays into an ImGui window `##D2RLoaderPluginOverlay` from
`FinalizeImGuiFrameWithPluginOverlays` (strings in D2RCore.dll); that pass never reaches the screen here. Treat
`OverlayService` as unusable until D2RLoader fixes it; option 1 above is withdrawn.

Ghidra note: under the Claude desktop app, `%APPDATA%\ghidra` is redirected into the app's package cache and
Ghidra's OSGi (Felix) cache fails ("The data file must be inside the data dir"). Run headless with
`JAVA_TOOL_OPTIONS=-Dapplication.settingsdir=C:\ghidra-work\settings`.

## (f) Native HUD text: widget APIs in 93847 (static, from the dump; NOT RUN IN GAME YET)

Goal: draw the lines as D2R `TextBoxWidget`s under `HUDPanel`, built from a layout the plugin ships under its own
name (global `.mpq`, like other global plugins), so no mod's `hudpanelhd.json` is overridden.

| RVA | What | Evidence |
|---|---|---|
| `0x856220` | `Widget* FindChild(Widget* parent, const char* name)`: direct children only; children = `Widget**` at `+0x58`, count at `+0x60`; child name (string) at `+0x8`, compared by `0x12277a0` | decompile; used by `0x1536020` for `"BuildVersionTextBox"`, `"InputPrompt"` |
| `0x86d960` | `TypeDesc* TextBoxWidget::Type()` (registers properties `style`, `altStyle`, `fitToText`, `truncateWithEllipsis` ...) | decompile |
| vtable `+0x58` | `TypeDesc* GetType()`; TypeDesc base at TypeDesc `+0x10`; is-a = walk that chain | `0x1536020` loop before the text set |
| vtable `+0x50`, `+0x48` | `(Widget*, bool)`: visible / enabled-like setters (`ExpBarWidget` uses `+0x50` to show/hide itself) | `0x14cc8c0`, `0x1536020` |
| `0x86dd50` | `TextBoxWidget::SetText(Widget*, const StringView* {const char* ptr, u64 len})`: assigns the `text` property (string at `+0x88`, length `+0x90`) and refreshes the shown text (`0x86e570`, cached at `+0x200`) | hundreds of UI callers; entry `mov r8,[rdx+8]; mov rbx,rcx; mov rdx,[rdx]`; `text` property entry at `0x2535310` = {"text", kind 3, offset 0x88} |
| `0x3e2b124` | **Not** a general setter: only caller is `0x1536020` (BuildVersionTextBox); D2RCore redirects it (`jmp [0x3e2a618]`) and it shows D2RLoader's build text whatever it is given. Live: every line read "D2RLOADER" in red | `Refs` = 1 caller; live 2026-10-30 |
| `0x8c70f0` | `BuildLayout(const char* name, int profile, out*)`: prefixes `data/global/ui/Layouts/`, reads with `0x8c7a90`, builds the tree | decompile |
| `0x854640` | result of `0x8c70f0` -> root `Widget*` | caller `0x89fe20` (creates panels by layout name, checks the root type against `UI\Panel`) |
| `0x8c7a90` | `bool ReadLayoutJson(doc*, const char* path)`; failure logs "Could not open the layout file/parse the JSON" (`0x8af5e0`) | decompile |
| `0x8c80d0` | loads the global style profiles (not widgets) | decompile |
| `0x14cc8c0` | `ExpBarWidget` update (section (c)); `0x1500fe0` = `HUDPanelHD` | string xrefs |

| `0x854de0` | `AddChild(Widget* parent, Widget* child)`: appends to the owned-children array (`+0x58` data, `+0x60` count, `+0x68` capacity). Does **not** set the child's parent or run layout/style init | the layout `children` parser `0x8c8cc0` calls it right after building each child |
| `0x854640` | `Panel* AsPanel(Widget*)`: is-a against `0x857f30` (`Panel` TypeDesc), else null | decompile |
| `0x846170` | `Panel* FindPanel(const char* name)`: PanelManager at `[0x3440170]`; searches its lists at `+0x88` / `+0xa0`, then the root's children; null when the UI is not up | decompile; `0x14c6760` uses it for `"HUDPanel"` |
| `0x3440840` | static `TextBoxWidget` TypeDesc (what `0x86d960` returns) | disasm |

Entry bytes (dump = memory layout, file offset = RVA): `0x846170` `48 8B D1 48 8B 0D F6 9F BF 02 48 85 C9 0F 85`;
`0x856220` `48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20`; `0x86d960` `40 53 48 83 EC 20 48 8D 0D AB 8A 45 01`;
`0x86dd50` `40 53 48 83 EC 20 4C 8B 42 08 48 8B D9 48 8B 12`.

Stock XP bar hover (show the hidden display while the bar is hovered):

| RVA | What | Evidence |
|---|---|---|
| `0x14cc3e0` | `ExpBarWidget` per-frame function (vtable entry `0x1fd8fd8`): shows the popup `TooltipHUDExperienceBar` only when `0x876180(*(this + 0x160))` | decompile |
| `0x876180` | `bool IsHovered(void* target)`: `PanelManager()[+0x178] == target`; entry `40 53 48 83 EC 20 48 8B D9 E8 92 FE FC FF 48 39` | decompile |
| `0x846020` | `PanelManager()`: `[0x3440170]`, asserts when null | disasm |
| `0x14cc700` | `TypeDesc* ExpBarWidget::Type()` (base `ProgressBarWidget`); property `isHireling` = {kind 1 (bool), offset `0x168`} (table `0x29eb260`); entry `48 83 EC 28 48 8D 0D 85 59 82 00 E8` | decompile + dump |

The plugin finds the player's bar by a bounded depth-first walk under `FindPanel("HUDPanel")` (children `+0x58`,
count `+0x60`) for an ExpBarWidget with `isHireling == 0`, then polls `0x876180` on the UI thread (every 50 ms).
Tooltip style for the background (profile): `inGameBackgroundColor` `[0, 0, 0, 0.8]`, padding `[20, 12, 20, 12]`,
no border; `RectangleWidget` takes `"color": [r, g, b, a]`.

### Chosen design: an SDK panel, filled natively

Adding widgets to the live `HUDPanel` with `0x854de0` skips parent/style/layout init, so it is not used. Instead:

1. The plugin registers its layout from memory with `ResourceService` at
   `data/global/ui/layouts/better-xp-bar/XpPanelhd.json` (the plugin's own namespace: no mod file is overridden,
   works as a global plugin, a mod can still replace it) and a panel `XpPanel` with `PanelService` (no flags).
   The layout is a `Panel` named `better-xp-bar/XpPanel` with `TextBoxWidget`s `Line1`..`Line6` (inline style:
   no mod-specific profile names).
2. On the UI thread, while in a game and shown: open the panel when it is closed (`getPanelInfo` / `openPanel`),
   then `FindPanel("better-xp-bar/XpPanel")` -> `FindChild(panel, "LineN")` -> TextBox is-a (vtable `+0x58`
   chain vs `0x86d960()`) -> `SetText` when the text changed. Hide = `closePanel`.

No hook, no patch; every native call is behind the build guard, its byte check and an SEH guard.

Live checks: the panel opens and stays open in gameplay (and what closes it: Esc menu, other panels); it does not
eat clicks; where the anchor/rect land at different resolutions; the text actually updates.

## Summary of "needs live check" items

1. The XP/level reads return the character sheet's values (compare with the character screen).
2. The experience-table values the plugin logs (flag, class, count, max level, two sample thresholds) and the
   `DataTableService` cross-check (row size 36).
3. That reads from the UI-thread task never assert or crash in town, in the wilderness, at death, at level-up and while
   leaving a game.
4. The overlay renders, where, and how it looks at your resolution; whether it covers anything important.
5. The stock XP bar's widget name/rectangle (optional anchoring).
6. Session reset on leaving a game (default scope); character-switch detection only with `session_scope = character`.
