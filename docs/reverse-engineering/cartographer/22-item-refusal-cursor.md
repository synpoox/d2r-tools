## 22. A refused map item and the item cursor (issue #307)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump (`C:\ghidra-work\D2RLoader_6ab3782c_dump.exe`,
Capstone). Static only: no game slot was free, nothing here was seen in game.

### Report

Using a second map item while a map is open shows the Scroll of Identify cursor, and the item stays. Map items are `misc.txt` rows copied from the Scroll of Identify
(`isc`, type `cmap`, `pSpell` 16), so a right-click on one looks like starting an identify.

### What the dump shows

- The item-use handler calls `ItemSpellCheck` (`0x5817f0`) at `0x4f52c6` with `rcx = game`, `rdx = player`, `r9 = target`, then `test al, al; jne 0x4f5438` at
  `0x4f52cb`. A nonzero result continues to the use block (`0x4f590f`, `call 0x581680` = `ItemSpellUse`, see [section 3](03-phase1-parts.md)).
- A zero result falls into `0x4f52d3`. It reads `game + 0x1aaa0`, builds a 0x24-byte string from a constant (`006D9B7D-8974-44C5-BB06-A9B63FE44DC0`, a GUID), and calls
  the object's virtual method at `[vtable + 0x50]` (`0x4f5363`) with it, then a second virtual call at `0x4f538a` through the object returned by `0x1412063c0`. If the first
  result (`dil`) is zero, control goes to the shared tail `0x4f45eb`; otherwise it runs on at `0x4f5397`. `ItemSpellUse` is not called on this path.
- Nothing on the zero path sets a targeting cursor or a pending-spell state that was recognised in this read. The packet and GUID call are not identified.

### What is not established

- That the zero return is what starts the cursor. The static read does not show it, and this note does not claim it. The cursor may come from the client's own right-click
  handling of an item with an item spell, before the server-side check runs; that would mean a check refusal (any of the five gates, or a slot problem) can never
  suppress it, whatever it returns.
- Whether changing the check's return value (for example returning nonzero and dropping the use) would avoid it: not tried, and it risks the item being used up, since what
  consumes the item after `ItemSpellUse` was not read.

### What the plugin does about it

The item interaction listener (`portal.cpp`, `OnItemActivate`) sees the right-click before the game's handler, evaluates the same gates (`CheckGates`, plus the slot problems)
from a copy of the local player's gate state that the tick keeps, and consumes the click with the check's own sentence. It does not depend on the check's return value.
It has not been seen to fire in game; the first harness-driven (posted) clicks did not reach it. A real-input check is outstanding (see the PR for #307).
