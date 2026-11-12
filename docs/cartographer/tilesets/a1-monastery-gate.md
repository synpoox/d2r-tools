# Act 1 Monastery Gate (#270): whole-preset only, low value

**Verdict: whole-preset only.** Effort **S** after the shared whole-preset slot exists (route in [a1-tristram.md](a1-tristram.md)).

Research note, 2026-11-12, vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 26 Monastery Gate: DrlgType 2 (preset), LevelType 5, **64 x 18 tiles**, SubType -1. LevelType 5 is used by this level alone, so it is shared with no other tileset issue.
- One LvlPrest row, Def 165 "Act 1 - Mon Front", LevelId 26, **one DS1** (the facade). No other piece row uses the Monastery type.
- Level 27 Outer Cloister has `Depend` 26 (its origin is offset from the gate); a slot copy has no such dependency.

## Why it cannot be generated

A single fixed facade with no piece rows: nothing for the maze path (needs room pieces with matching doors) or the stamp planner (needs sockets) to recombine.

## What a map could be

A 64 x 18 strip is a corridor: the entrance, event area and boss area are three constant spans along it, not separate rooms.

## Game checks needed

The list in [a1-tristram.md](a1-tristram.md) (slot copy opens, arrival spot, room rectangles, boss/event placement), plus whether the open-air view past the facade draws with Act 1 tiles in an Act 5 slot.

## Closing option

Recommend closing as not worth building on its own, or folding it into one "Monastery" set with the cloister once the preset route exists.
