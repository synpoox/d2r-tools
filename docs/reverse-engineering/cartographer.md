# Cartographer native contracts (`cartographer`)

This is the index. Each section of the record is its own file under [`cartographer/`](cartographer/); section numbers ("section 5", "§5") name the files below. Status, date, consumer, module and evidence sources are in [the overview](cartographer/00-overview.md).

New sections go in new files (the next free number, `18-<topic>.md`), and a new check or result for an existing section goes in that section's file, so open PRs do not conflict on one file. Add a new file to this list.

- [Overview: status, date, consumer, module, evidence](cartographer/00-overview.md)
- [1. Addresses](cartographer/01-addresses.md)
- [1a. Maze signatures (issue #29)](cartographer/01a-maze-signatures.md)
- [2. Not yet recorded](cartographer/02-not-yet-recorded.md)
- [3. Phase 1 parts: the map item and the red portal (`plugins/cartographer`, issue #20)](cartographer/03-phase1-parts.md)
- [4. Outdoor S0 and S1 spike results (issue #22)](cartographer/04-outdoor-spike.md)
- [5. Maze slot engine checks (issue #62)](cartographer/05-maze-slot.md)
- [6. Map stats and events engine checks (issue #72)](cartographer/06-stats-events-checks.md)
- [7. Map stats parts (issue #80)](cartographer/07-map-stats-parts.md)
- [8. Outdoor builder engine checks (issue #83)](cartographer/08-outdoor-builder.md)
- [8a. Outdoor build hook (issue #141)](cartographer/08a-outdoor-build-hook.md)
- [9. Map events: entry message, decision, Boss and Horde (issue #92)](cartographer/09-map-events.md)
- [10. Red portal tooltip and area name (issue #90)](cartographer/10-portal-tooltip.md)
- [11. Event Spawn names from the game's compiled tables (issue #95)](cartographer/11-spawn-names.md)
- [12. Multiplayer map sync engine checks (issue #98)](cartographer/12-map-sync.md)
- [13. Ambush, Treasure and Shrine events: static checks (issue #102)](cartographer/13-ambush-treasure-shrine.md)
- [14. The map item's own stats (issue #158)](cartographer/14-item-stats.md)
- [15. Map monster columns in the slot level's row and region record (issue #156)](cartographer/15-map-monster-columns.md)
- [16. Outdoor S2 spike results (issue #89)](cartographer/16-outdoor-s2.md)
- [17. AutoCheck: a game check the plugin runs by itself (issue #173)](cartographer/17-autocheck.md)
- [18. Chest open never reached the hook (issue #220)](cartographer/18-chest-operate.md)
- [19. Treasure chests are never locked (issue #220)](cartographer/19-chest-lock.md)
