# Quick NPC: feasibility analysis

Feature: hold a configurable hotkey and left-click a vendor NPC to start the game's normal vendor interaction.

**Status: blocked before implementation.** No plugin code was written, because the hooks it needs can't be found
or verified yet. The toolkit's rules (and this task) say to stop before writing speculative hooks.

## Sources read

From d2rAI-toolkit (default branch, shallow clone) and its `third_party/PluginSDK` submodule (commit `717f727`,
SDK 0.3.0, plugin ABI 4):

- `CLAUDE.md`, `.claude/skills/d2r-engine/SKILL.md`
- `knowledge/engine-re.md`, `knowledge/d2r-93847-addresses.md`, `knowledge/d2rloader.md`
- `plugin-template/README.md`, `plugin-template/src/plugin.cpp`
- `testing/README.md`
- PluginSDK `include/D2RLPlugin/*.h` (input, panels, data_tables, core_exports, reimplementation_exports,
  item_interactions) and `examples/input-action`

## Findings for each question

### 1. How the game handles world left-clicks on NPCs

**Not established.** The build 93847 address registry lists nothing on the client input or click path: no mouse
handler, no hovered/selected unit, no "interact with unit" request. Classic D2 sends an "interact with unit"
request to the server (unit type and id). The server walks the player into range, then the client opens the NPC
dialog. That is only a starting point for reverse engineering: the D2R function and packet layout are unverified.

### 2. Input layer: mouse buttons and keyboard modifiers

**Partly established, from the SDK headers:**

- `InputService` (`input.h`, ABI 1) registers named **keyboard** actions. Each binding is a virtual key plus at
  most one modifier (Shift, Ctrl or Alt). Callbacks get `Pressed` and `Released` events, so "hotkey held" can be
  tracked with a supported API. Bindings are stored in `d2rloader\config\input-bindings.toml` and can be changed in
  the game's controls menu, which covers "configurable".
- Actions don't fire while the player is editing text or binding controls. That suppresses chat input.
- The SDK has **no mouse button, cursor, or hovered-unit API**. No service, core export or reimplementation export
  covers world clicks. The left-click side needs a native hook.
- Unverified: whether `Released` always arrives on focus loss (alt-tab while the key is held). If it doesn't, the
  plugin would need a failsafe such as clearing the state on area change or game leave.

### 3. Identifying NPCs and whether they offer vendor services

**Not established.**

- Unit type 1 (monster) at unit `+0x00` and class id at `+0x08` are in the registry. They identify a monstats row,
  not a vendor.
- The SDK exposes the `Npc` data table (`DataTables::Npc`, npc.txt). In vanilla, npc.txt holds the buy/sell/repair
  price multipliers of the trading NPCs. Whether D2R uses "has an npc.txt row" as its vendor test is unverified.
- Classic D2 kept a hard-coded table of each NPC's dialog menu entries (Talk, Trade, Gamble, Repair, Hire, ...).
  Which D2R structure holds the menu options, and how quest state changes them, has to be found in the dumped
  image. That table, or the function that builds the menu, is the correct vendor test. A list of names or ids is
  not.

### 4. Starting the vendor interaction and opening the vendor UI

**Not established.** The registry has no NPC dialog, store or trade-menu functions. `panels.h` only lets plugins
add their own panels next to stock panels. It can't open the vendor screen, and forcing a panel open would skip
the server's eligibility and store setup. The likely vanilla path is "interact with unit", then the NPC dialog,
then "Trade" chosen, then the server opens the store. The plugin would need to start the first step itself and
choose "Trade" automatically when that NPC's dialog opens. Every function in that chain must be found and
verified first.

### 5. Relevant functions, hooks, structures and addresses

| Need | In registry or SDK? | Evidence |
|---|---|---|
| Hotkey held | Yes, `InputService` (supported API) | `input.h` |
| Left-click on a world unit (client) | No | Not in `d2r-93847-addresses.md` |
| Click on the world vs. a UI panel | No | No UI hit-test address is listed |
| Hovered/selected unit under the cursor | No | Only `0x9a570` (a unit's *skill* target) |
| Unit type / class id | Yes (`+0x00`, `+0x08`, `0x34a330`) | Registry, hooked in a past plugin |
| Vendor capability of an NPC class | No | npc.txt is in the SDK; meaning unverified |
| Interact request / NPC dialog / Trade | No | Not in registry |
| Image check (`0x6ab3782c`), off-switch, status command, logging | Yes | `plugin-template` |

### 6. Supported API or native hook?

**A native hook is needed** for the click and the interaction: no supported API covers them. The hotkey can use
`InputService`.

## Why it stops here

The toolkit's only way to find hook sites is to dump the **running** game image (`tools/dump_image.py`; D2R's code
is encrypted on disk), analyse it in Ghidra (`tools/ghidra_analyze.ps1`, which takes hours), and then check entry
bytes against the live code. This environment is a Linux container with no game install, no image dump, no
Ghidra, no PowerShell and no MSVC. So:

- no function, signature, calling convention or entry bytes could be found or verified for the click, interaction
  or vendor-menu paths;
- the plugin can't be built here (`build.ps1` needs Visual Studio and CMake 3.29 or newer on Windows);
- no game-copy test (`testing/`) can run, because it needs Windows and a game install.

Writing hooks against guessed addresses would break the toolkit's rule 9 and this task's fail-closed requirement.

## Smallest viable next step: an RE spike on a Windows machine with D2R 3.3 and D2RLoader 1.3.1

1. `python tools/dump_image.py` with the game at a character in town. Confirm the PE stamp is `0x6ab3782c`, or
   record the new stamp. Then run `tools/ghidra_analyze.ps1`.
2. Find the client interaction path. Run `StringXrefs` on the D2Client UI source paths for the NPC dialog (search
   `NpcMenu`, `Npc`, `Store`, `Trade` in assert strings). Then use `Refs` to trace up to the function that sends
   the "interact with unit" request, and down to the code that builds the NPC dialog's menu entries.
3. Find the vendor test: the menu-entry table or function that adds "Trade" to the dialog, together with its quest
   conditions. `FindDisp` on the npc.txt record offsets would show whether npc.txt rows take part.
4. Find the click-on-unit handler: the caller of the interact request that runs on a left-click. Confirm it is only
   reached for world clicks that pass the UI hit-test.
5. For each function: `Disasm` the entry, check that it doesn't start with `FF 25` (already hooked by D2RLoader),
   record the signature and at least 14 bytes of whole instructions in `knowledge/d2r-93847-addresses.md`.

With those addresses, the planned design (the smallest hook surface) is:

- an `InputService` action `quick-npc-vendor` (default: a key plus no modifier, rebindable) that tracks "held";
- **one** inline hook on the client click-on-unit handler. It always calls the original, unchanged. When the
  hotkey is held, the click is a fresh press, and the target is a monster-type unit whose class passes the
  verified vendor test, it records `pending = {unit id, frame}`;
- when that NPC's dialog opens while `pending` matches, the plugin chooses "Trade" through the game's own menu
  handler, then clears `pending`. If the dialog doesn't open (out of range and the walk is cancelled, or the menu
  has no Trade entry), `pending` expires. Vanilla movement, range, quest and store logic stay untouched;
- template parts: build guard, off-switch word `quicknpc`, a status line with counters, one log line at install,
  and hooks removed at plugin shutdown.

Possible fallback if the dialog hook proves fragile: with the hotkey held, have the click only open the NPC's
normal dialog (vanilla behaviour) and do nothing else. That would make the feature pointless, so the dialog step
is the real requirement.

## In-game checks performed

None. No game was available, and no test is claimed as passed.
