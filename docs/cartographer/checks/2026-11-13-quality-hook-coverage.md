# Map quality hook coverage check, 2026-11-13

Issue #383. Build 93847, D2RLoader 1.3.1-beta, slot 5, test save `cartoh`, god mode on, test mod with map items t20 to t22 staged. A scratch diagnostic hook on the item maker `0x442d60` and the creator `0x43ef10` logged each call (removed afterwards; not in the PR).

| Source | Maps made | Seen by the quality roll | Final quality | Item level at the hook |
|---|---|---|---|---|
| `cheat spawn` (3 runs) | 373 | 373 | all normal, 0 magic | 100 |
| Monster drop (`testtreasure mon=19,num=10`) | 10 | 10 | all normal (1 rolled magic) | 1 (town test) |
| Cube output | not verified (`cheat cubemake` made no items) | | | |
| Vendor or gamble | not checked | | | |

About half the rolls are low or superior, which the old summary did not print; that was the "half" of #383. The cube path passes flag 0 and no request, so it never rolls (see the RE note).
`cheat spawn` stops when the ground is full (about 130 items), so spawned counts were below those requested.

Verified: live in game for spawn and drop counts. Not verified: cube, vendor, gamble, real kills in a map.
