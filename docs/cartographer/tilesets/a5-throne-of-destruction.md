# Act 5 Throne of Destruction (#293): whole-preset only

**Verdict: whole-preset only.** Effort **S** after the shared whole-preset slot exists (route in [a1-tristram.md](a1-tristram.md)); the Baal wave behaviour is an open **M** risk.

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 131 Throne Room: **DrlgType 2 (preset)**, LevelType 34, **40 x 52 tiles**. LvlPrest Def 1086 "Act 5 - ThroneRoom", LevelId 131, **one DS1**, populated from the level's monster columns.
- Level 132 World Stone Chamber: DrlgType 2, LevelType 34, 55 x 55, Position 1. LvlPrest Def 1087, LevelId 132, **one DS1, Populate 0** (no monsters from the level; Baal's chamber is scripted). It is not a separate map candidate and the issue's "levels 131 and 132" reduces to 131.
- Both are in Act 5, the slot's act, so no cross-act tile loading is involved.

## Why it cannot be generated

One DS1 per level, no sockets, no doors. The Worldstone Keep levels 128 to 130 (below) are a maze of unrelated pieces and cannot be joined to the throne.

## What a map could be

A fixed 40 x 52 arena: arrival at the south end, event area in the middle, boss at the north (Baal's end). Random boss, event, monsters and stats.

## Game checks needed

As [a1-tristram.md](a1-tristram.md) (slot copy opens, arrival spot, room rectangles, boss/event placement), plus the one that decides value:

- **Baal waves.** The five waves and Baal are spawned by game script, not placed in the DS1 (inference from Populate and the superunique/wave design). Check whether, in a slot copy, they run, are absent, or crash (any level-id-keyed code, wave triggers, Baal's throne objects). If they run, the Cartographer boss competes with Baal; if absent, the map is a plain arena.

## Shared LevelType 34 (Worldstone Keep, #292)

LevelType 34 is shared by Baal Temple 1 to 3 (128 to 130, **DrlgType 1 maze**, 16-tile pieces, LvlPrest Def 1059 to 1085, no catalogue rows yet: tilesets.tsv has no Baal row), the Throne Room (131) and the World Stone Chamber (132). **#292 does not cover the Throne**: its pieces are the Baal mask families, not Def 1086. What #292 gives is the LevelType 34 tile set; since this is an Act 5 type and the slot is Act 5, no extra loading is expected. The Throne needs only the preset slot above, and does not wait on #292.
