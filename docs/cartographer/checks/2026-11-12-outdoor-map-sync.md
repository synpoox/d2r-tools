# Cartographer: outdoor map sync, two games on one PC (2026-11-12)

Live in game: #146, a TCP/IP client builds the same outdoor (Jungle) layout as the host. Two game instances at once on
this PC, slot 11 as host and slot 6 as client, Hell, test saves only, joined through Direct Join to 127.0.0.1 with the
host set to "LAN Only". The map was 'Overgrown Reach' (maps.txt row 2, Jungle Winding Medium, outdoor slot level 140,
seed 14817885552194207151). The host opened it with the plugin's AutoCheck; both characters then walked into the red
portal in Harrogath. The plugin code is the merged one from #182 (no code change was needed for #146); it was built with
`tools\build.ps1 -Plugin cartographer -Test` on a tree that includes `85724a3`. Logs and screenshots are kept outside the
repo. Both slots were put back with `stage_check.ps1 -Restore` and are unlocked.

| Check | Result |
|---|---|
| 1. Client receives the host's map | PASS: the client log shows row 2, slot level 140, the host's seed, "accepted", then the name set on the client's slot level 140. |
| 2. Client uses the host's seed | PASS: "outdoor map 'Overgrown Reach' ... opened with the host's seed 14817885552194207151" on the client. |
| 3. Equal layouts | PASS: both logs show the same plan hash `d6b28c5d207a65de`, 22 stamps, 656 void cells, level 288 x 224 tiles at (6000,3000), 352 rooms, entrance cell (6224,3192), and the same level entry room (room 10 of 352 at 6232,3200). |
| 4. Both entered | PASS: both characters were in level 140 (host `AUTOCHECK arrival: the player is in level 140`; the host's log also shows the client's player entering the map). |
| Stability | PASS: 0 `[ERROR]` and 0 `[FAULT]` in both `cartographer.log` files. |

The room lists were compared by the plan hash, room count and entry room, not room by room. The two sides do not log
each room's position.

Log lines:

```
host:   outdoor slot level 140: planned 'Overgrown Reach' (Jungle Winding Medium, seed 14817885552194207151): 22 stamps, 656 void cells, level 288 x 224 tiles at (6000,3000), plan d6b28c5d207a65de
client: map sync: the host's open map: row 2 'Overgrown Reach', slot level 140, opening 1, outdoor seed 14817885552194207151 (accepted)
client: outdoor slot level 140: planned 'Overgrown Reach' (Jungle Winding Medium, seed 14817885552194207151): 22 stamps, 656 void cells, level 288 x 224 tiles at (6000,3000), plan d6b28c5d207a65de
client: outdoor slot level 140: built as planned (22 stamps, 656 void cells; grid 1008 cells: 22 pieces, 352 covered, 656 void, 0 neither), 352 rooms
both:   outdoor arrival: room 10 of 352 (at 6232,3200) marked as the level entry, in the entrance cell (6224,3192)
```

Setup notes for the next run:

- The host must be in "LAN Only" mode for Direct Join; otherwise the join fails (JoinFailed, reason 7).
- The two slots' slot level ids must match (`slotLevelMatches`); slot 6 had different ids, so slot 11's `levels.txt`,
  `lvlmaze.txt` and `slots.txt` were copied into slot 6's test mod. `-Restore` warns about those changed files.
- Use different character names on the two sides.
- The save must be past Act 4 for the AutoCheck open ("can only be opened in town").
- AutoCheck waits 180 s for a portal entry on the host.

Not tested: a client that joins after the host already built the slot (the "no rebuild" skip), a client whose maps.txt row
or slot level differs (skip with a logged reason, covered by unit tests only), Battle.net, more than two players, and
the Maze and Snow terrains over the network.

