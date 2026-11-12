# Act 1 Cathedral (#274): whole-preset only

**Verdict: whole-preset only.** Effort **S** after the shared whole-preset slot exists (route in [a1-tristram.md](a1-tristram.md)).

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 33 Cathedral: DrlgType 2 (preset), LevelType 9, **28 x 34 tiles**, `Depend` 32 (Inner Cloister), SubType -1. LevelType 9 is used by this level alone, so it is shared with no other tileset issue.
- One LvlPrest row, Def 257 "Act 1 - Cathedral", LevelId 33, **one DS1**. No piece rows. The Catacombs below it (levels 34 to 37, LevelType 10, a maze plus a preset) are a different tileset (planned as a maze with its own growers).

## Why it cannot be generated

One DS1, no sockets, no doors to match. Not a maze, not outdoor.

## What a map could be

One fixed 28 x 34 hall with random boss, event and monsters. Big enough for an entrance end, an event area and a boss end.

## Game checks needed

As [a1-tristram.md](a1-tristram.md) (slot copy opens, arrival spot, room rectangles, boss/event placement), plus: the DS1's own preset units (monsters and objects) are acceptable next to Cartographer's boss, and the stair objects to the Catacombs do nothing harmful in a slot level.
