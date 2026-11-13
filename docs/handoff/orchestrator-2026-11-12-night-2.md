# Orchestrator handoff, 2026-11-12 night (2)

main: `0f0335a` (Lava maze edge fill, #377). Workers all stopped; game slots 5, 6, 7, 11 free (slot 11 still has a
hook-build cartographer.dll; run `stage_check -Restore` before using it).

## Merged this session

#335 Ice caves, #373 rarity follow-ups, #376 #328 crash note (abort at exit, not Cartographer), #375 unique pacing
(Refs #317, experimental), #374 spellicon fix (Closes #307), #368 automap waypoint follow-ups, #382 rarity in-game record,
#377 Lava edge fill (Refs #372). Closed: #366, #307, #321, #326, PR #318 (superseded).

## Open PRs, next step each (priority order)

1. **#386** size rename (Closes #381), `604982a`: review has 2 blocking one-word doc fixes: `cartographer-siege.md:17`
   "Medium"->Large, `tables.md:57` "about 73 on a Large Cave"->ExtraLarge. Non-blocking: `cartographer-snow.md:35,135`,
   a line on RE notes 05/15/29. Fix cycle 1, then merge. **Owner's own maps.txt uses `Large` for Molten Depths: after
   #386 that is 9x9; tell the owner to change it to `ExtraLarge`.**
2. **#378** hud-text (Closes #370, Refs #371), `e7d12fb`: fix cycle 1 done; needs the cycle-2 review.
3. **#369** Boss fallback (Closes #364), `428dcf3`: ready, checked in game on Ice; needs a reviewer.
4. **#387** spellicon docs (Closes #379), `04ccaef`: needs review. Open point: does a blank `spellicon` cell read as 0
   (handler in D2RCore.dll)?
5. **#389** draft tz-text-hook (base hud-text), `61045a6`, Refs #371: hook works only while a terror zone is on. Left:
   README wording, squash WIP. **Owner decision pending:** is "replaces the zone lines only when a zone is on" enough?
6. **#388** draft research for #380 (tint): RE note 32, lighting via VisualDataComponent apply `0xf5dad0`. No
   implementation yet.

## Open issues with no work yet

- #383 quality hook sees about half of spawned maps; #384 Item Level 1 (lead: ItemLevel getter `0x36ccf0`); both under #353.
- #385 unique pace undercounts with Lava filler rooms (under #317).
- #380 tint (research in #388), #371 remainder (in #389).

## Owner install (done 2026-11-12)

- `d2rloader\plugins\cartographer.dll` = main `0f0335a` build (backup `cartographer.dll.before-0f0335a`).
- `install.ps1` run on ProjectForever: added level 144 Cartographer Ice Caves + its lvlmaze row.
- Rare-map affixes added to the mod (no prior files; generated from the game's own tables via CASC, script in the
  orchestrator scratchpad `affix\build.py`): `magicprefix.txt` 8 cmap rows (groups 9100-9107: Brutish, Savage, Swift,
  Infernal, Glacial, Stormbound, Blighted, Teeming), `magicsuffix.txt` 6 rows (9200-9205: of Resilience, Might, Vitality,
  Haste, Fortune, the Horde), `raresuffix.txt` cmap rows, `item-nameaffixes.json` ids 60100-60106 (English only) in
  `strings` and `strings-legacy`. Not yet checked in game: owner tries `cheat spawn t23,typ r,lvl 90,inv 0`.
- Earlier: 60 map icons (HD, lowend, DC6) imported to the mod; t20-t23 use map_08/23/37/45; owner set spellicon -1.

## Hazards

- Never file "no automap lines" issues; the owner checks those by eye.
- Keep an implementer's worktree until its PR merges (fix cycles need it).
- Briefs: no PowerShell `Set-Content` on docs (BOM / double-encoding damage on #375).
- One slot per agent; confirm window and PID before input. Harness can't do real right-clicks or walking (owner asked
  about real mouse input in d2rAI-toolkit; no answer).
- Owner messages: one-liners, only when the owner is needed.

## Pending owner questions

- pSpell 18 (pSpells Extension) for maps: recommended staying standalone; no answer.
- #389 scope (above).

## Leftover worktrees (`.claude/worktrees/`)

agent-a3588976 and agent-a64fbec0 (size-rename / pr-386, locked), agent-aa4546f7 (map-tint), agent-aac9e212
(tz-text-hook), agent-ab382b04 (spellicon-docs, locked), agent-ac826390 (maze-boss-room). Remove each after its PR merges.
