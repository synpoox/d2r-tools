# Quick NPC native contracts (`quick-npc`)

**Dates:** 2026-10-29 (vendor path, hooks), 2026-10-30 (Cain Identify, Resurrect, Travel)
**Consumer:** [Quick NPC](../../plugins/quick-npc/README.md) (`quick-npc.dll`)
**Module:** `D2R.exe`, D2R 3.3 build 93847 (Battle.net) / 94995 (Steam) under D2RLoader 1.3.1, PE image stamp
`0x6ab3782c`. Values are RVAs (image base `0x140000000` + RVA).
**Evidence source:** a memory-layout dump of the running game (d2rAI-toolkit `tools/dump_image.py` method).
Function bounds from `.pdata`, disassembly with Capstone, raw disp32 scans for leaf functions without unwind data;
the NPC menu table was read by emulating its static initializer with Unicorn.
**Reproduce:** `python plugins/quick-npc/tools/verify_dump.py <dump> --table` re-checks every value below.

Cross-check: the entry bytes of `0x2f7ea0`, `0x33aa00` and `0x56ac00` from d2rAI-toolkit's
`knowledge/d2r-93847-addresses.md` match the same dump byte for byte.

## 1. Left click on an NPC

| Step | RVA | Evidence |
|---|---|---|
| Player action `(player, action, x, y)`. A switch on `action` (jump table `0xfeb78`, 18 cases) sets command flags: left press `\|5` (`0xfe65a`), left held `\|9`, left release `\|0x11`, right press `\|6`, right held `\|0xa`, right release `\|0x12`, plus `0x200`/`0x400` variants | `0xfe3b0` | Called by the input handlers `0x839e0`, `0x83b00`, `0x83fc0`, `0x8abc0`, `0x8acc0`, `0x8b3c0`. Command struct on the stack at `rsp+0x58`: `+0` flags, `+8` player, `+0x10` target unit |
| Act on the target, branching on its unit type | `0x102490` | Assert path `D2Client\src\Unit\Player.cpp` |
| Monster-type target: needs flag 4 (press, `test bl,4` at `0x102919`) and the monstats interact flag (record `+0x3d`, via `0x976e0`), then calls interact | `0x102919` → `0x101910(cmd, 1, unitId)` | Held repeats (flag 8) never reach `0x101910` for an NPC |
| **Interact with unit** `(cmd, unitType, unitId)`. Type 1: range 2 (NPC) or 5; sends `CCMD 0x4d` (STOPNPCREQUEST) first. In range: `CCMD_GET_QUEST_DIALOG` (`0x2f`, `0x101e10`). Out of range: walks and stores the pending command in the player path data (`+0x1c8` = 1, `+0x1cc` = `0x2f`, `+0x1d0` type, `+0x1d4` id), sent on arrival | `0x101910` | Only caller `0x102490` (`0x10290f`, `0x102968`); result unused |

Command flag bits at `+0`: `1` left, `4` press, `8` held, `0x10` release, `0x20` Stand Still, `0x40` Run. UI clicks
never become player actions on a unit, so a click on a panel, chat or menu cannot reach `0x101910`.

**Input layer.** PluginSDK 0.3.0 `InputService` registers keyboard actions only (no lone Ctrl/Shift/Alt, no mouse
buttons, no hovered unit), so Quick NPC reads the click from the `0x101910` hook and the hotkey with
`GetAsyncKeyState` at that moment.

## 2. Modifier keys

| What | RVA / value |
|---|---|
| Key-binding names by binding id (switch `0x121e9c`, 0..0x44) | `0x121b20`: 0x22 Run, 0x23 Run Lock, 0x24 Stand Still, 0x25 Show Items, 0x3b Force Move |
| Default binding tables, 20-byte records `{u16 key, ..., u32 binding id, ...}` | `0x22a92b0`, `0x22a9820`. Both: **Run = VK 0x11 (Ctrl)**, Stand Still = 0x10 (Shift), Show Items = 0x12 (Alt) |
| Run held state (binding 0x22, `lea ecx,[rbx+0x22]` at `0x83eef`) | `0x2a23758` |
| Stand Still held state | `0x2a23760` |
| `0x8afd0` packs them for the player action: high byte Run (and `0x2a2375c`), low byte Stand Still | read at `0x8afdd`, `0x8b018` |
| `0xfe3b0`: Run held and stamina (stat 0xa) > 0 → flag `0x40` (`0xfe603`); Stand Still → flag `0x20` (`0xfe619`) | |
| `0x102490`: flag `0x20` takes the stand-still branch before NPC interaction (`and ebp,0x20; jne 0x102992` at `0x102626`) | Shift + click never reaches `0x101910` |
| Flag `0x40` is not tested on `0x102490` → `0x101910` | Ctrl + click interacts normally; the player runs |

The left-button handler `0x83fc0` checks no modifier: it always passes action 0 (left press) to `0xfe3b0`.

## 3. NPC menu table

`D2Client\src\UI\npcmenu.cpp`.

| What | RVA / layout |
|---|---|
| Table: vector of **0x48-byte rows** | data `0x2a48830`, count `0x2a48838`, capacity `0x2a48840` |
| Row | `+0` u32 class (compared as u16), `+4` u32 option count including the final Cancel, `+8` u16 string ids[8], `+0x18` 5 handler pointers, `+0x40` byte |
| Built at start-up | static initializer `0x360d0`: 48 rows (`0xd80` bytes) copied out at `0x3724d` |
| Rows for customnpcs.txt | `0x10b6e0`, called per customnpcs record (`dataTables+0x1510`, 0x10e bytes) by `0x10cfc0`; Talk only |
| Quest options added when a dialog opens | 9 entries at `0x22a70e0` (class, quest, flags, function), applied in `0x1141d0` through `0x325c50`. They add Hire, travel and others; none adds or removes Trade |
| Resurrect added to hireling rows at run time | `0x1164d0` (`lea r13, 0x113130`) |
| Lookup | `0x113990`: the **first** row whose u16 class matches |
| Options shown | `0x1147a0`: options `0 .. count-2` via `0x1e9030(menu, text, 0, enabled, handler)`, then Cancel (`0x112c30`) |
| Option call | the menu calls `handler(dataFlag)` at `0x2bb5e1`; `dataFlag` comes from `0x2bb058..0x2bb088`: the local player's (`0x8b2d0` id → `0x9a480` unit → `0x34a0e0`), else the client game's (`0x76010`, u8 at `+0x9cf`) |

Emulating the initializer gives 16 vendor classes (147, 148, 154, 177, 178, 199, 202, 252, 253, 254, 255, 257, 405,
511, 512, 513) and 31 menu classes without Trade (Cain, mercenary and travel NPCs, quest NPCs, a gamble-only row 514).
Monsters and other units have no row. Quick NPC's vendor test is "the class's first row shows the Trade handler".

Command ids come from D2R's own name switches: `CCMD` names `0xa1f8b0` (table `0xa1fe44`, 0..0x70), `SCMD` names
`0xa20010` (table `0xa208d8`). The client `SCMD` handler table starts at `0x22bbc40` (0x18-byte entries
`{handler, size, 0}`).

## 4. Menu option handlers

All are `bool(u8 dataFlag)` and act on the NPC in the menu globals: id `0x2a4875c`, has-NPC `0x2a48760`, class
`0x2a48764`, NPC UI state `0x2a486a4` (0 idle, 1 menu shown, 2 trade/gamble requested, 7 identify), menu widget
`0x2a487b0` (null when closed).

| Handler | RVA | Guard / witness | Contract |
|---|---|---|---|
| Talk | `0x112eb0` | | |
| **Trade** (strings `0xd44` "Trade", `0xd06` "Trade/Repair") | `0x112f30` | `mov edx,1` `0x112fc6`, `mov cl,0x38` `0x112fcb` | Closes the menu, UI state 2, gamble flag `0x2a48808` = 0, sends `CCMD_SUBSCRIBE_TO_VENDOR` (`0x38`) mode 1 |
| Gamble | `0x112c80` | | As Trade with mode 2, gamble flag 1 |
| Hire | `0x112620` | | Never pressed by Quick NPC |
| **Identify** (Cain: rows 244, 245, 246, 265, 520) | `0x114000` | `mov cl,0x34` at `0x1141ab` | Closes the menu, opens a wait dialog, prices via `0x325c50` quest checks (free after his quest), sends `CCMD_IDENTIFY_TRANSACTION` (`0x34`), UI state 7 |
| **Resurrect** (dead mercenary) | `0x113130` | entry `40 53 48 83 ec 30`; `mov cl,0x52` at `0x113158` | Sends `CCMD_REZZ_MERCENARY_TRANSACTION` (`0x52`) |
| **Travel** ("Go East/West", "Sail East/West") | `0x1162f0` | entry `48 89 5c 24 10 48 89 7c 24 18`; `movzx ebx,cl` `0x11631c`; `mov cl,0x37` `0x116388` | Sends `CCMD_TRAVEL` (`0x37`) |

In `0x1147a0` Identify (string `0xfb4`, `0x114a3c`) is shown only when the player has something to identify: the
live row's count becomes 3 (`0x114b09`), else 2 (`0x114bca`). Quick NPC therefore presses an option only if the open
menu shows it.

## 5. Hooks

| Hook | RVA | Entry bytes (whole instructions) | Signature | Already hooked? |
|---|---|---|---|---|
| Interact with unit | `0x101910` | `40 53 55 56 57 41 56 48 83 EC 30 41 8B F0` | `(void* cmd, i32 type, u32 id)`; result unused | No (no `FF 25`) |
| Open NPC menu | `0x1141d0` | `40 55 57 41 55 41 57 48 8B EC 48 83 EC 48` | `(void* npc, i32 flag)`; result unused | No |

`0x1141d0` sets the menu globals (§4) from the unit (`0x34a330` id, `0x349860` class), applies quest options, checks
the panel can open (`0xcd7c0(8,0)`, else `CCMD 0x30`) and builds the menu (`0x1147a0`). Callers: the
`SCMD_INTERACTION_ENABLE` (`0x28`, 103 bytes, stub `0x12b030`, entry `0x22bc000`) path `0x10e6e0` at `0x10ef90`,
and the speech-finished callback `0x10da20` at `0x10dcb7`. After it returns, `0x10e6e0` writes `0x2a48808`
(`0x10efac`), the flag Trade sets: so the option is pressed from a later `ThreadService::runOnUiThread` callback, not
inside the hook.

Neither entry has a RIP-relative instruction. Both hooks call the original (interact before, menu after) and go in
together in one `MutationService` transaction; D2RLoader removes them at unload.

**Failure mode:** image stamp or entry-byte mismatch → the plugin logs "not the expected game image" (or the failing
hook) and installs nothing.
**Coexistence:** no other plugin in `plugins/` hooks `0x101910` or `0x1141d0`.

## 6. Unit layout

Confirmed by the getters: `+0` i32 type (`0x34b9d0`), `+4` u32 class (`0x349860`), `+8` u32 unit id (`0x34a330`).
Client unit lookup `0x9a5d0(id, type)` over the hash table `0x2a23910`.

Correction for d2rAI-toolkit's `knowledge/d2r-93847-addresses.md`: its Units table says "unit +0x08 = class id"; the
getters show `+4` is the class and `+8` the unit id.

## Validation

| Level | Result |
|---|---|
| Unit tests (`tools\build.ps1 -Plugin quick-npc -Test`) | pass (MSVC, 2026-10-29) |
| Dump verification (`verify_dump.py --table`) | all checks pass on the `0x6ab3782c` dump |
| In game | vendor Trade confirmed by the owner on 0.1.0/0.2.0. Identify, Resurrect and Travel not yet tested in game |
