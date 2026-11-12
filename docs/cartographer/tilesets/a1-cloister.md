# Act 1 Cloister (#271): whole-preset only

**Verdict: whole-preset only.** Effort **S** per level after the shared whole-preset slot exists (route in [a1-tristram.md](a1-tristram.md)). Outer Cloister is worth it; Inner Cloister is very small.

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- LevelType 6 is used by exactly these two levels, so it is shared with no other tileset issue.
- Level 27 Outer Cloister: DrlgType 2, 56 x 40 tiles, `Depend` 26. LvlPrest Def 166 "Act 1 - Courtyard 1", LevelId 27, **three whole DS1 files** (West, North, East); the level RNG draws one. Each is a complete level, not a piece.
- Level 32 Inner Cloister: DrlgType 2, **18 x 20 tiles**. LvlPrest Def 256 "Act 1 - Courtyard 2", LevelId 32, **one DS1**.
- LvlPrest Def 167 "Act 1 - Barracks Court Connect" (LevelId 0, 10 x 14, W/N/E files) is a joiner that attaches the Barracks maze (LevelType 7) to the Outer Cloister. It is the only recombinable piece near this type and does not generate a cloister.

## Why it cannot be generated

Whole DS1s, not pieces, with three variants at most. No sockets for the stamp planner, no mask doors for the maze path.

## What a map could be

Outer Cloister: three fixed layouts plus random boss, event and monsters; 56 x 40 has room for separate entrance, event and boss rectangles. Inner Cloister at 18 x 20 is too small for three distinct areas.

## Game checks needed

As [a1-tristram.md](a1-tristram.md), plus: the slot's LvlPrest row must keep all three DS1 files so the level RNG still draws a variant, and the rectangles are needed **per variant** (log which file the level drew).

## Closing option

Build Outer Cloister only; close Inner Cloister as too small.
