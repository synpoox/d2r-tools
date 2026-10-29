# Testing MultiSpec

Only in a toolkit game slot (`../d2rAI-toolkit/testing/`, docs/testing.md there), never in the owner's own install.
Back up the test character first (`.d2s`, `.d2rl`, `.multispec`). Phase 3 turns these rows into named checks.

## Phase 2: first slot runs (in this order)

| # | What | How | Expected |
|---|---|---|---|
| 1 | Loads on the right image | Start the slot, read `mods\<Mod>\d2rloader\logs\multispec.log` | `image 0x6ab3782c (... match)`, `specswap on: entry bytes ok: 19 functions`, `speccmd on` |
| 2 | Console route | In game, D2RLoader console: `spec` | Spec list for the character. Then type `/spec` in the **chat box** and note whether it is handled (design section 9, check 1) |
| 3 | Client pointer | Same `spec` | No "cannot find your character" line. If it appears, copy the log line: the console's client is not the server client (check 2) |
| 4 | File | After 3 | `<save folder>\<name>.multispec` exists; the log says "next to the save" (not FALLBACK) |
| 5 | Swap | Allocate a few points; in town `spec 2` | "spec 2 loaded (0 points, N unspent)"; skill tree shows all points unspent; spend some |
| 6 | Swap back | `spec 1` | Spec 1's exact levels, unspent as before; skill tree, hotbar and passives right (check 3) |
| 7 | Save/load | Save and exit, reload the character, `spec` | Same active spec and levels; character still listed in the menu |
| 8 | Refused outside town | Leave town, `spec 2` | "specs can only be changed in town"; nothing changes |
| 9 | Unspent preserved | Level up or use a skill point source in spec 1, then `spec 2` | The new point shows as unspent in spec 2 |
| 10 | Summons | With a summoner's spec active and pets out, `spec 2` (pets' skill drops to 0) | Note what happens to the pets (check 3) |
| 11 | Status | `multispec` | Counters match what you did |
