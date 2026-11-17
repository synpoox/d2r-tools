# 51. Map drop behaviours: monster drop, item make, socket parts (issue #538)

Build 93847. RVAs are file offsets in the loader dump. Byte signatures are in `src/core/part_signatures.h`.

| Part | RVA | Use |
|---|---|---|
| DeathDrop | 0x447b80 | hooked (already, for the Boss Drop); a context for the kill opens before the original and closes after the `drop_tc` rolls |
| DropRoller | 0x441300 | now hooked (20 bytes, instruction boundary): after the monster's own roll, the same call again for each bonus pick. DeathDrop calls it once, with the monster's class as the fourth argument |
| ItemMake | 0x43cd50 | hooked (20 bytes). `(game, request, int)`, returns the new item unit (0 on a refusal). Called only by 0x444680 (drop item), from 0x44486b. The request's +0 is the dropper, +0xc0 a flag word; 0x4 is ethereal (from the PD2R notes; set before the call) |
| ItemSocketable | 0x36b410 | called: nonzero for an item that can take sockets and has none. 0x36b1d0 is a twin; the signature keeps the first call's distance unmasked to tell them apart |
| ItemSetSockets | 0x36e880 | called: `(item, count)`, cut to the item's own maximum (0x36e880 reads the base row's +0x11e x +0x11f, at most 6), writes stat 0xc2 and flag 0x800. 0x375560 is a twin |
| UnitStat, UnitRoom, RoomLevelId | | as the portal and stat parts |

Hostile is `UnitStat(unit, 172, 0) == 0`, as the route table does. Minion is `monster data + 0x1a & 0x10`, from the PD2R notes; **not re-verified in D2R code** (the byte is logged at every kill: `map behaviours: kill of unit ...: monster flags 0x..`). Each signature is unique in the dump (checked by a scan).

Needs a game check (see the PR): that the ethereal flag at +0xc0 reaches the made item, that sockets set after ItemMake show on the dropped item, and the minion bit.
