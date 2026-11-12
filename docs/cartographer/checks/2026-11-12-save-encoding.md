# Cartographer: "Character save failed ... encoding-error=yes" in test slots (2026-11-12)

Live in game: slot 11, D2R 3.3.0 build 93847, D2RLoader 1.3.1-beta, test mod `D2RToolsTest`, test save `cartos` (restored from a
backup after each game), Hell, single player. Refs the slot 11 check ([2026-11-12-slot11-check](2026-11-12-slot11-check.md), item
"not looked into"). The logs are not kept in the repository.

## Result

**Cartographer does not cause it.** The failure happens with no Cartographer in the slot. The cause is PD2 plugins left in the
slot's global `d2rloader\plugins` folder.

| Game | Plugins in the slot | Result |
|---|---|---|
| A | `core-maps`, `core-rules`, `core-ui` (the PD2 plugins, copies of 7 Nov), `test-harness`; no Cartographer, no map, no staged tables | `BC_ASSERT: nStat >= 0 && size_t(nStat) < dataTable->statsTbl.size()` at character load, then `Character save failed ... native-result=1 encoding-error=yes` about 30 s after the character loaded (the first autosave), the save was cancelled |
| B | `test-harness` only (the `core-*` DLLs moved out) | No assertion, no save failure in 3 minutes; `cartos.d2s` was rewritten by the autosave |

Same save, same slot, same loader, same minute of the day; the only change between A and B was the three `core-*` DLLs.

The other observations fit: the slot 11 Snow/Cave game of the check had the same three DLLs loaded (the loader log lists
"PD2 Resurrected Core Maps/Rules/UI"), and `core-maps` reported three hooks it could not install. The assertion comes before any
Cartographer code runs in game (13:55:55, Cartographer's local player at 13:55:57). Slots 5 and 6 carry other builds of the same
three DLLs (SHA-256 differs from slot 11's and from slot 7's); the slot 5 run showed the `nStat` assertion too (plus an `nShift`
one) but no save failure, so the failure depends on the build of the PD2 plugin, not on every copy. Which of the three DLLs
does it was not narrowed down (not needed: none belongs in a test slot of this repository).

Not the cause, ruled out by game A: Cartographer's boss names (Latin-1 or not), `MapNames`, map stats, spawned items, the Snow
level, the `cartos` save (it saves in game B), `itemstatcost.txt` (the mod had no staged tables in game A). The earlier
suspicion in `testing.md` that map items make the save fail ("Map items in a test slot") may also have been this; it was not
separated.

## Fix

`plugins\cartographer\tools\stage_check.ps1` now parks every other `.dll` in the slot's plugins folder (all except the staged
DLL and `test-harness.dll`) in `plugins-off\staged-<owner>\`, records it in the manifest, and `-Restore` moves it back when its
place is empty. `-KeepPlugins` skips this. Verified with `-WhatIf` against slot 11 (a copy of `core-maps.dll` in its plugins
folder is listed as parked, the plan is printed, nothing moved). The real park and restore paths were not run.

Checked at the level of: live in game (games A and B, one run each, slot 11, test save only); the script change by `-WhatIf` only.
Slot state afterwards: game stopped, save restored from the backup (byte-identical), slot unlocked, the slot's plugins folder holds
`test-harness.dll` only (identical copies of the three `core-*` DLLs are in `plugins-off`).
