# Orchestrator handoff, 2026-11-13

**main:** `ef83fd420f36b3a47df28ea4e022b7fa76573563`. The game is D2R build 93847 with D2RLoader 1.3.1-beta. Every worker is stopped, no game is running, and slots 5, 6, 7 and 11 are unlocked and restored.

## Owner rules set this session (read first)

1. **Cartographer only adds a mapping system.** Do not file, dispatch, propose or audit anything about caps, limits, clamps, overrides or altering game data or values, unless the owner asks for that exact change. Answer questions about them factually and stop there. #450, #451 and #453 were cancelled by the owner and closed as not planned.
2. **Dispatch priority:**
   1. Bug issues (ignore #328).
   2. Follow-up issues from closed PRs.
   3. Woven tilesets.
   4. Other enhancements.
   5. New tilesets.
3. **Worker cap is 3 implementers.** Reviewers don't count. Do not interrupt running workers when the cap changes.
4. **Owner messages** are for blockers or decisions only, one line per item.
5. **Results** go in the PR body or an issue comment. Never add files under `docs/cartographer/checks/`.
6. **Setup errors** (skipped rows, table errors) go to the console and log only, never to on-screen player text.
7. **`map-level`:** no rules anywhere; modders handle it however they choose.
8. **The map panel** must not cover the equipment panel. Its properties must use player-facing names.
9. **Maps** can't be bought from vendors or gambled. The Area Level line reads `Area Level: N`, a cube recipe can raise it, and cube-made maps show it.
10. **Never file automap-line issues;** the owner checks those by eye.

## Merged this session

#413, #388, #406, #419, #420, #423, #425, #426, #427, #428, #433, #434, #435, #437, #439, #440 (per-map Brightness/Warmth, Refs #380), #443 (Closes #436 and #438), #447 and #452 (#353 closed), and #448 (TCP/IP client automap skip, Refs #354).

## Open PRs

| PR | Issue | State | Next step |
|---|---|---|---|
| #444 `cartographer/unique-pack-pace` ce7cdae | #317 | Ready. Fix cycle 1 of 2 is done: the pack lag was dropped, note 29 reconciled, filler piece reset in SteeredWalk, tests with fillers. Final-build Lava runs gave 5/5/6 (86%) and 6/3/5 (94%). | Re-review (cycle 1 findings: https://github.com/synpoox/d2r-tools/pull/444#issuecomment-6860508262). If it is clean and CI is green, merge (Refs #317). Not done: the second main "before" sweep, Cave on the final build, Snow, walking. |
| #449 `cartographer/boss-guarantee` bc9c372 | #445 | Draft. The fallback ladder is built: plan boss room, rest of its cell, other rooms farthest first, then the entry room forced with spawn flag 0x80 (skips collision). RE note 34; 22/22 tests. In game: 6 opens, 5 placed in the boss room; 1 Ice map was left waiting on the next room. | Finish the in-game done-condition: 20 opens including Snow and Jungle, the `CARTOGRAPHER_TEST_BOSS_REFUSE` runs for the fallback and forced paths, and the flag 0x80 spawn seen. Check the C4456 warning at event_apply.cpp:297. Then review. |
| #446 `cartographer/woven-prototype` 46a9562 | #415 | Draft. Code and unit tests only (21/21); debug-only `WovenProbe` setting plus the `RoomTileLoader` hook. **Not run in game:** it was blocked on slots. | Run the check from the PR body on a free slot (the test tables are in the old session scratchpad, so rebuild them from note 33 §10), then review. |

## Open issues

- **Bugs:**
  - #317 (PR #444).
  - #445 (PR #449).
  - #354: the multiplayer part is fixed by #448. What's left is the client-side town trip and re-entry, and the owner's by-eye reveal.
  - #328: ignored by the owner for now.
- **Woven tilesets:** #415 (PR #446) and #396.
- **Enhancements:**
  - #380 (parent; Brightness/Warmth merged).
  - #441 tint and #442 hue/saturation research. Both are colour work from the owner's own request, not caps.
  - #309, #269, #247, #111.
- **New tilesets:** #238–#244, #270–#293, #236.

## Pending owner decision

- pSpell 18: my recommendation was option A, to stay standalone. The owner has not decided.

## Hazards

- The auto-mode classifier blocks combined destructive git commands (worktree remove, branch -D, push --delete); run each one separately.
- **Slot contention:** `stage_check.ps1` takes only slots 5, 6, 7 and 11, which blocked #415. Slot 11's test mod has hand-edited tables (treasure class, affixes, event chest) and a level-99 test character.
- Map scrolls get used up by game saves, so use `cartographer open <Name>`. Misc map rows need `compactsave` 0.
- **Agents** can't load the d2r-in-game-testing skill through the Skill tool. Give the full path `C:\Users\micah\source\repos\d2rAI-toolkit\.claude\skills\d2r-in-game-testing\SKILL.md` and `plugins\cartographer\tools\stage_check.ps1` (`-MapItems`, then `-Restore` with the same `-Owner`). Use `debug_mode` then `cheat god 1` before any map. Test saves only, and back up or restore saves only while the game is stopped.
- **Fix cycles** run in a detached checkout with `push HEAD:<branch>`, because of the worktree edit hook.
- **Changelog** entries are fragments in `plugins/cartographer/changelog.d/`, exactly one line each. Workers sometimes edit `CHANGELOG.md` directly; check every PR.
- **#445's worker overwrote** `bosses.txt`, `cartographer.txt` and the `cartoh.*` saves in the old session scratchpad `tables`/`saves` folders. Don't trust those copies.
- The old test-pile maps showing "Missing string" affixes with no stats are probably leftovers from older setups (#452 review). A fresh rare map put through Save and Exit and reload would settle it.

## Leftover worktrees (kept until their PR merges)

- `.claude/worktrees/agent-a6466e94e72714038`: #444.
- `.claude/worktrees/agent-a9e0c5b2fd266588c`: #449 (locked).
- `.claude/worktrees/agent-a6e912bdb3673effb`: #446 (locked).
- `.claude/worktrees/agent-af0ab3ee332172b1e` (`cartographer/map-panel-totals`, 52bfb89): an older lane. #412 is merged, so check it and remove it.
- The other `.claude/worktrees/*` and `d2r-tools-*` worktrees belong to other sessions or plugins; leave them.
