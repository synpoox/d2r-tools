# Testing Potion Counter

Run in a toolkit game slot, in town, on a level 99 character. `potion-counter` in the console prints the live state
(the counts, the panel, the SDK results); the plugin log has one `belt:` line per count change, a `belt popped up:`
line per popup change (the first 20 together), with each belt item as `slot:code`, and a `belt rows:` line when the
row count changes.

| # | What | Expected | Status |
|---|---|---|---|
| 1 | Loads | Log: `counter on: find panel rva 0x846170, find child rva 0x856220, visible flag +0x51, belt boxes +0x6a4, belt popped +0x6a0` | pass (Slot10, 2026-11-18) |
| 2 | 3-row Belt, mixed potions and a scroll | three squares above each slot in the belt slot frame, each with the item's own picture; empty ones dark | pass (Slot10): `belt rows: 3`, `belt: 3 2 2 1 (slot:item 1:hp5 2:mp5 3:tsc 0:hp1 4:hp5 8:hp5 5:hp5 6:mp5)` shown as such |
| 3 | Belt popped up with the `` ` `` key | the squares hide; they come back when it closes | pass (Slot10): `belt popped up: yes, squares hidden`, then `no` |
| 4 | Mouse on the belt (the game pops it up) | the squares hide | pass (Slot10): hidden while hovering, back after the mouse left |
| 5 | Right-click a belt slot | drinks it; the square empties | pass (Slot10): slot 2, `belt: 3 1 2 1` |
| 6 | Left-click a belt slot | the item goes to the cursor; its square empties | pass (Slot10, earlier build, same panel): `belt: 2 2 2 1` |
| 7 | 4-row belt (Demonhide Sash), also popped up | four smaller squares per slot; hidden when popped up | pass (Slot10): `belt rows: 4`, popped `yes` then `no` |
| 8 | No belt | one square per slot | pass (Slot10): `belt rows: 1`, `belt: 1 1 1 1` |
| 9 | 2-row belt | two squares per slot | not run: the only Sash in the save is unusable (red) and refused |
| 10 | Controller HUD (virtual pad) | the squares sit above each controller belt slot, same count as the mouse HUD, 1, 3 and 4 rows | pass (Slot10, 2026-11-18): panel at 118, -176 (4 rows), cells at x 12 142 272 402; D-pad drink empties a square. Popped-up hiding not reachable on the pad (the `` ` `` key does nothing there) |
