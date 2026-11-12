# Cartographer native contracts (`cartographer`)

**Status:** the nine maze rows in section 1a and the twelve phase 1 parts in section 3 have byte signatures, checked statically against a build 93847 dump (no game
evidence; the portal hooks are written, not validated in game). Section 5 answers the maze slot's level size, origin and `0x3eb850` questions statically (issue #62), with the in-game checks still open. Section 6 answers, statically, how to apply stats to monsters, the player and the map item, how to override region density and level for one level, the event entry points and the entry text path (issue #72), each claim marked static, inference or needs game. Section 8 answers, statically, the twelve open outdoor builder questions (issue #83), each marked static, inference or needs game with the exact in-game check. The outdoor rows (section 4) have byte signatures that matched on build 93847 in a test slot, and the spike exercised the parts live at the levels shown per row. Every other row of sections 1 and 2 is still a stub, not validated in this repo.
**Date:** 2026-11-11 (carried over from pd2-resurrected research)
**Consumer:** Cartographer (planned `plugins/cartographer`; epic synpoox/d2r-tools#19). Plans:
[cartographer-plugin.md](../../cartographer/plans/cartographer-plugin.md),
[cartographer-outdoor-builder.md](../../cartographer/plans/cartographer-outdoor-builder.md).
**Module:** `D2R.exe`, D2R 3.3 build 93847, under D2RLoader. Values are D2R.exe RVAs (image base `0x140000000` + RVA); final addresses will be found by
byte signature, not fixed RVAs. The `0x3e2b5ce` / `[0x3e2ac38]` row is a D2RLoader stub in D2R.exe whose jump target was
logged inside `D2RCore.dll` (section 4, S0). `0x3e2b5c8` (6 bytes before it) is the same kind of stub, also into `D2RCore.dll`.
**Evidence source:** the pd2-resurrected research and spikes, not re-checked here:
- `docs/research/drlg-steering.md` on pd2-resurrected branch `docs/drlg-steering-spike` (pull 929);
- `docs/research/level-regeneration.md` on pd2-resurrected branch `claude/level-regen-spike-117061` (pull 930);
- `spike/cartographer-steer-proto`: the maze steering prototype (`plugin/cartographer-steer-spike/plugin.cpp`);
- `spike/cartographer-level-regen`: the level regeneration spike;
- `spike/cartographer-outdoor-s1` at b78cfe2: the pd2 outdoor builder spike (uncompiled; reference only);
- `spike/cartographer-outdoor-s1` in this repository (synpoox/d2r-tools): the compiled spike `plugins/cartographer-outdoor-spike/`
  at 8d377ca (S0 probe, S1 jungle line) and the follow-up commit named in section 4; it is the evidence for section 4.

The pd2-resurrected links above are under https://github.com/synpoox/pd2-resurrected.

Every row in section 1 has: Build 93847, Guard "not yet: needs a byte signature" (except the nine maze rows of
section 1a, which have one, and the rows marked "signed", which carry a signature that matched exactly once in the running image; their Guard and Validation are in section 4), Status "not validated in this repo" (the 1a rows: static only), and
Failure mode "fail open: log and leave Cartographer's map off". The tested image stamp is `0x6ab3782c`, read
from the loaded image and logged by the outdoor spike at load (section 4). Coexistence: D2RCore patches or thunks some DRLG code (`0x3e2b5ce`); see that row.
