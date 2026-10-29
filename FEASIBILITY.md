# Quick NPC: feasibility analysis

**Result: feasible and implemented** for D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image `0x6ab3782c`), with two
pass-through native hooks, a supported input action and a UI-thread callback. Not yet tested in game.

The first version of this file stopped before implementation, because no dump of the game image was available. With
the dump the user supplied, every hook site, signature and data layout was found and checked. Details and evidence:
[docs/reverse-engineering.md](docs/reverse-engineering.md). `tools/verify_dump.py` re-checks all of them.

## Answers

1. **How the game processes world left-clicks on NPCs.** The input handlers call the player action `0xfe3b0`, which
   sets command flags (left press = 1|4). `0x102490` (D2Client `Unit/Player.cpp`) checks that the target is a
   monster-type unit, that the command is a press (flag 4) and that monstats marks it interactable, then calls
   `0x101910(cmd, 1, unitId)`. That function sends `CCMD_GET_QUEST_DIALOG` in range, or walks there and sends it on
   arrival.
2. **Input layer.** PluginSDK 0.3.0's `InputService` exposes keyboard actions only (`Pressed`/`Released`, one
   optional modifier, saved in D2R's Controls menu). It has no mouse or hovered-unit API. The left press comes from
   D2R's own command flags inside the `0x101910` hook.
3. **NPC identification and vendor services.** D2R's client keeps an NPC menu table (`npcmenu.cpp`): 0x48-byte rows of
   class, option count, string ids and option handlers. It's built by `0x360d0`, extended for customnpcs.txt and quest
   options, and looked up first-match by `0x113990`. An NPC can open the vendor screen exactly when its row shows the
   Trade handler `0x112f30`.
4. **How the vendor interaction starts.** The server answers the dialog request with `SCMD_INTERACTION_ENABLE` (0x28).
   `0x10e6e0` then `0x1141d0` opens the menu. Clicking "Trade" runs `0x112f30`, which closes the menu and sends
   `CCMD_SUBSCRIBE_TO_VENDOR` (0x38) mode 1. The server then opens the store.
5. **Relevant functions and evidence.** See the tables in docs/reverse-engineering.md. Evidence includes D2R's own
   assert paths and `CCMD`/`SCMD` name switches, the `SCMD` handler table, the emulated menu table, and the entry
   bytes checked against the dump.
6. **Supported API or native hook.** A hybrid: the hotkey uses the supported `InputService` and the trade press uses
   `ThreadService`. The click and the dialog need native hooks: `0x101910` (before the original) and `0x1141d0`
   (after it). Both always call the original, and neither is hooked by D2RLoader.

## Design choices that keep vanilla behavior

- Nothing sends a packet or opens a panel itself. Movement, range, quest state and the server's store checks are all
  D2R's.
- Clicks without the key, non-vendors, held repeats, right clicks, objects and players: the hooks only record that a
  vanilla interaction happened.
- One trade per interaction. A new click, another NPC's dialog, the feature switch or a 60-second timeout cancels it.
  The UI callback checks the menu again before pressing Trade.
- The Trade press runs from `ThreadService::runOnUiThread`, not inside the packet handler, because the handler writes
  the menu globals after `0x1141d0` returns.
- Fails closed: wrong image, an `FF 25` at a hook entry, mismatched entry bytes, a missing service or a failed action
  registration installs nothing that acts (see `RunInstall`).
- Both hooks go in through one `MutationService` transaction (both or neither), which D2RLoader removes at plugin
  unload. The unload handler switches the feature off, and D2RLoader drops the queued callback and the input action.
