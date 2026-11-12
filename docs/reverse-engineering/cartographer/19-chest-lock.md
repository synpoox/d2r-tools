## 19. Treasure chests are never locked (issue #220)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs.

### Finding

A locked chest only says "I need a key" and never reaches its Drop (section 18). The lock is a bit in the object's data:

- The object data pointer is `*(unit + 0x10)`. Its byte `[8]` is the chest's flag byte: bit 7 (`0x80`) is "locked", the low 7 bits are the chest's look. The chest operate handler (`0x58e2f0`) tests that bit.
- Chest InitFn 3 (`0x50e920`, init table `0x239b980`) sets or clears bit 7 at random when the object is created, so about two in three placed chests were locked.
- The game reads the byte through `0x34ad40(unit)`, which returns `(data[0x78] << 8) | data[8]` (the same function the portal destination level id uses, see the registry), and writes it through `0x34e9d0(unit, byte)`, which stores `data[8]` and zeroes `data[0x78]`. Both are small wrappers that check the unit type (object, type 2) and jump on to the body.

### Parts

| Part | Role | RVA | Signature | Hook | Guard |
|---|---|---|---|---|---|
| ObjectFlagsGet | Reads the flag word of an object. Call only. | `0x34ad40` | the whole wrapper, ending `E9 ?? ?? ?? ?? 90 90 90 5B C3` | none | Unique on the dump; `tools/check_portal_signatures.py` checks it. Off (logged) on a miss. |
| ObjectFlagsSet | Writes the low byte of the flag word. Call only. | `0x34e9d0` | the whole wrapper, ending `E9 ?? ?? ?? ?? 90 90 90 5F C3` | none | Same. |

### What the plugin does

Right after the game creates the plugin's own Treasure chest (and only that unit), the plugin reads the flag byte through the getter, clears bit 7 through the setter, and reads it again. If the read-back differs, both parts are turned off with a logged warning and the chest stays as the game made it. If either part is missing, the chest is left alone (logged). No raw memory is written, and no other chest is touched. The pure decision (`unlockChest`) is unit tested.

### In game (slot 7, Hell test save `cartoh`, build above, log showed the build just made)

- Six placed chests across games: four were locked and logged `unlocked (flag byte 0x80 -> 0x00)`, two logged `was not locked`.
- Four chests, two of them unlocked by the plugin, opened without a key: `the operate call ... returned 1, mode 0 -> 1: rolled`, then the Drop `Act 5 (H) Super C` (class 1227) rolled.
