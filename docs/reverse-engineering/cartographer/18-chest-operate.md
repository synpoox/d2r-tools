## 18. Chest open never reached the hook (issue #220)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs.

### Finding

The treasure chest hook sat on the dispatcher `0x592c40` (section 13). A click on an object never runs it:

- The click goes packet handler (call at `0x4b17e4`) -> `0x593630`, which indexes the operate table `0x23a51c0` by the byte at the objects record `+0x15e` and calls the entry inline (`call rbx` at `0x5938dc`). It never calls `0x592c40`.
- `0x592c40` has one caller, `0x4a3b33`, the skill-on-object path.
- Both paths end at the table entry. Slot 4 is the chest handler `0x58e2f0`, so the hook moved there.

### Part

| Part | Role | RVA | Signature | Hook | Guard |
|---|---|---|---|---|---|
| ChestOperate | Chest operate handler. One argument: a pointer to the operate context, `[0]` game, `[8]` object, `[0x10]` activator. Returns 0 early when the object's mode is nonzero; says "It's locked" / "I need a key" (`0x491960(player, 0x17, ...)`) when the lock bit is set and returns 1. | `0x58e2f0` | `48 89 4C 24 08 55 48 8B EC 48 81 EC 80 00 00 00 48 8B 49 08` | 16 bytes (four whole instructions, no relative operand) | Matches once; `tools/check_portal_signatures.py` checks it against the dump. A miss logs and leaves the chest roll off. Nothing else hooks `0x58e2f0`. |

### In game (slot 5, Hell test save, build above, log showed the build just made)

- Part found by signature and hooked at load; the placed chest (objects row 146) is tracked.
- A click on a locked chest reached the hook (return 1, mode 0 -> 0, "I need a key"); the old hook logged nothing for any click.
- Two opens of an unlocked chest in separate games: `the operate call ... returned 1, mode 0 -> 1: rolled`, then the Drop `Act 5 (H) Super C` (class 1227) rolled with level value 80 and loot (potions, gold, weapons) fell beside the chest.

### Left over

Placed chests were randomly locked (InitFn 3), and a locked chest needs a key (4 of 6 test chests were locked). The plugin now clears the lock on its own chest: section 19.
