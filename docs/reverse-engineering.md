# Quick NPC: reverse-engineering notes (build 93847)

Everything here is for **D2R 3.3.0 build 93847 under D2RLoader 1.3.1**, PE image stamp **`0x6ab3782c`**. Values are
RVAs (image base `0x140000000` + RVA). The source was a memory-layout dump of the running game
(`D2RLoader_6ab3782c_dump.exe`, 90,451,968 bytes, written by d2rAI-toolkit's `tools/dump_image.py` method).

**Tools.** Ghidra could not be installed in the analysis environment: its release download was blocked by the network
policy. The same queries the toolkit's Ghidra scripts run (string xrefs, call refs, displacement users, switch tables,
disassembly) were done with a capstone-based index over every `.pdata` function, plus a raw disp32 scan for leaf
functions without unwind data. The NPC menu table was read by emulating its static initializer with unicorn.
`tools/verify_dump.py` re-checks every value below against a dump.

**Cross-check against the toolkit registry.** The entry bytes of three hooks listed in
`knowledge/d2r-93847-addresses.md` (`0x2f7ea0`, `0x33aa00`, `0x56ac00`) match this dump byte for byte.

## 1. How a left click on an NPC is processed (client)

| Step | RVA | Evidence |
|---|---|---|
| Player action: `(player, action, x, y)`. A switch on `action` (jump table `0xfeb78`, 18 cases) sets command flags: left press `|5` (`0xfe65a`), left held `|9`, left release `|0x11`, right press `|6`, right held `|0xa`, right release `|0x12`, and `0x200`/`0x400` variants | `0xfe3b0` | Called by the input handlers `0x839e0`, `0x83b00`, `0x83fc0`, `0x8abc0`, `0x8acc0` and `0x8b3c0`. The command struct is on the stack at `rsp+0x58`: `+0` flags, `+8` player, `+0x10` target unit |
| Act on the target, branching on its unit type | `0x102490` | Assert path `D2Client\src\Unit\Player.cpp` |
| Monster-type target: requires flag 4, the press (`test bl,4` at `0x102919`), and the monstats interact flag (record `+0x3d`, via `0x976e0`), then calls interact | `0x102919` → `0x101910(cmd, 1, unitId)` | Held repeats (flag 8) never reach `0x101910` for an NPC |
| **Interact with unit** `(cmd, unitType, unitId)`. For type 1: range 2 (interact NPC) or 5; sends `CCMD 0x4d` (STOPNPCREQUEST) first. In range: `CCMD_GET_QUEST_DIALOG` (`0x2f`, `0x101e10`). Out of range: walks and stores the pending command in the player path data (`+0x1c8` = 1, `+0x1cc` = `0x2f`, `+0x1d0` type, `+0x1d4` id), sent on arrival | `0x101910` | Only caller `0x102490` (`0x10290f`, `0x102968`) |

UI clicks never become player actions on a unit, so a click on a panel, chat or menu cannot reach `0x101910`.

## 2. Input layer

- PluginSDK 0.3.0 `InputService` registers **keyboard** actions only (`Key` = virtual-key codes, at most one modifier;
  `Pressed` / `Released` events; inactive while chat or another text field has focus). It has no mouse button,
  cursor or hovered-unit API, so the left click comes from the native hook on `0x101910`.
- The hook reads the button and press from D2R's own command flags (`+0`: bit 1 left, bit 4 press, bit 8 held,
  bit 0x10 release).

## 3. Which NPCs are vendors: D2R's NPC menu table (`D2Client\src\UI\npcmenu.cpp`)

| What | RVA / layout |
|---|---|
| Table: a vector of **0x48-byte rows** | data pointer `0x2a48830`, count `0x2a48838`, capacity `0x2a48840` |
| Row | `+0` u32 class (compared as u16), `+4` u32 option count including the final Cancel, `+8` u16 string ids[8], `+0x18` 5 handler pointers `bool()`, `+0x40` byte |
| Built at start-up | static initializer `0x360d0`: 48 rows (`0xd80` bytes) copied out at `0x3724d` |
| Rows for customnpcs.txt | `0x10b6e0`, called for every customnpcs record (`dataTables+0x1510`, 0x10e bytes) by `0x10cfc0`. They get only Talk |
| Quest options added when a dialog opens | 9 entries at `0x22a70e0` (class, quest, flags, function), applied in `0x1141d0` through `0x325c50`. They add Hire, travel and other options. **None adds or removes Trade** |
| Lookup | `0x113990`: the **first** row whose u16 class matches |
| Options shown | `0x1147a0` builds the menu: options `0 .. count-2` with `0x1e9030(menu, text, 0, enabled, handler)`, then Cancel (`0x112c30`) |

Option handlers (each referenced only by the initializer, apart from Talk):

| Handler | RVA | Body |
|---|---|---|
| Talk | `0x112eb0` | |
| **Trade** (string `0xd44` "Trade", `0xd06` "Trade/Repair") | `0x112f30` | Closes the menu, sets UI state 2 (`0x2a486a4`), gamble flag 0 (`0x2a48808`), sends `CCMD_SUBSCRIBE_TO_VENDOR` (`0x38`) mode 1 for the NPC `0x2a4875c` (`mov edx,1` `0x112fc6`, `mov cl,0x38` `0x112fcb`) |
| Gamble | `0x112c80` | Same with mode 2 and gamble flag 1 |
| Hire | `0x112620` | |
| Identify | `0x114000` | |
| Travel | `0x1162f0` | |

The plugin's vendor test is "the class's first row shows the Trade handler". This is the game's own data, not a name
list. Emulating the initializer on this dump gives 16 vendor classes (147, 148, 154, 177, 178, 199, 202, 252, 253, 254,
255, 257, 405, 511, 512, 513) and 31 menu classes without Trade (Cain, mercenary and travel NPCs, quest NPCs and a
gamble-only row, 514). Monsters and other units have no row.

Command and server-command ids come from D2R's own name switches: `CCMD` names `0xa1f8b0` (table `0xa1fe44`, 0..0x70)
and `SCMD` names `0xa20010` (table `0xa208d8`). The client `SCMD` handler table starts at `0x22bbc40` (0x18-byte
entries `{handler, size, 0}`).

## 4. How the dialog and vendor screen open

| Step | RVA |
|---|---|
| Server answers with `SCMD_INTERACTION_ENABLE` (`0x28`, 103 bytes) → stub `0x12b030` | entry at `0x22bc000` |
| `(u8 type, u32 id, questData)`: NPC interaction start | `0x10e6e0` |
| **Open NPC menu** `(npc unit, flag)`: sets `0x2a48760` = 1, `0x2a4875c` = unit id (`0x34a330`), `0x2a48764` = class (`0x349860`), applies the quest options, checks the panel can open (`0xcd7c0(8,0)`, else `CCMD 0x30`), builds the menu (`0x1147a0`: menu object `0x2a487b0`, UI state `0x2a486a4` = 1) | `0x1141d0` |
| Callers: the `SCMD_INTERACTION_ENABLE` path (`0x10ef90`) and the speech-finished callback (`0x10dcb7` in `0x10da20`) | |
| After `0x1141d0` returns, `0x10e6e0` writes `0x2a48808` (`0x10efac`), the flag the Trade handler sets | so the plugin presses Trade from a later UI-thread callback, not inside the hook |

Unit layout confirmed by the getters: `+0` type (`0x34b9d0`), `+4` class (`0x349860`), `+8` unit id (`0x34a330`). The
client unit lookup is `0x9a5d0(id, type)` over the hash table `0x2a23910`.

Note for the toolkit registry: its Units table says "unit +0x08 = class id", but `0x349860` returns `+4` (class) and
`0x34a330` returns `+8`, which the NPC menu stores as the unit id and looks up with `0x9a5d0`.

## 5. Hooks used

| Hook | RVA | Entry bytes (whole instructions) | Signature | Already hooked? |
|---|---|---|---|---|
| Interact with unit | `0x101910` | `40 53 55 56 57 41 56 48 83 EC 30 41 8B F0` (push rbx/rbp/rsi/rdi/r14, sub rsp,0x30, mov esi,r8d) | `(void* cmd, i32 type, u32 id)`; the result is unused | No (no `FF 25`) |
| Open NPC menu | `0x1141d0` | `40 55 57 41 55 41 57 48 8B EC 48 83 EC 48` (push rbp/rdi/r13/r15, mov rbp,rsp, sub rsp,0x48) | `(void* npc, i32 flag)`; the result is unused | No |

Neither entry contains a rip-relative instruction. Both hooks always call the original. The interact hook acts
before the original and the menu hook after it.

They are installed together in one `MutationService` transaction. `commit` checks both entries' bytes, overlaps and
call-through pointers, then applies both or neither. D2RLoader removes them when the plugin unloads.

## 6. Supported API or native hook?

| Need | Answer |
|---|---|
| Hotkey | Supported API: `InputService` action (configurable in Options > Controls) |
| Left click on an NPC | Native hook `0x101910`. No SDK API |
| Vendor test | Reads D2R's live menu table. No SDK API |
| Start trading | Calls the menu's own Trade handler `0x112f30` from `ThreadService::runOnUiThread`. No SDK API |

No data-table change is needed.
