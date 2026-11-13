# Map quality hook coverage check, 2026-11-13

Issue #383. Build 93847, D2RLoader 1.3.1-beta, slot 5, test save `cartoh`, god mode on, test mod with map items t20 to t22 staged. A scratch diagnostic hook on the item maker `0x442d60` and the creator `0x43ef10` logged each call (removed afterwards; not in the PR).

The three log snapshots taken during the `cheat spawn` session (20, 133, 240 maker calls) are cumulative: every count only grows. The session made 240 maps, not 373. The ground area stops at about 130 items, so spawning was repeated after clearing.

| Source | Maps made | Seen by the quality roll | Final quality | Item level at the hook |
|---|---|---|---|---|
| `cheat spawn` | 240 | 240 | all normal | 100 |
| Monster drop (`testtreasure mon=19,num=10`) | 10 | 10 | all normal (1 rolled magic) | 1 (town test) |
| Cube output | not verified (`cheat cubemake` made no items) | | | |
| Vendor or gamble | not checked | | | |

Rolls of the 240 spawned maps by quality: 114 low, 97 normal, 23 superior, 4 magic, 1 rare, 1 unique, 0 set, 0 crafted. All 240 ended normal. The plugin makes the 4 magic rolls normal and sends the rare and unique rolls (their makers fail in the test mod) to the normal fallback; low and superior rolls (137) end normal in the game itself (observed, not traced). The old summary did not list low or superior, so about half the rolls looked unseen. The cause is inferred from these counts; the old 148 of 304 run was not repeated.
The cube path passes flag 0 and no request, so it never rolls (see the RE note). The cube, vendor and gamble gap is filed as its own issue.

Verified: live in game for spawn and drop counts. Not verified: cube, vendor, gamble, real kills in a map.
