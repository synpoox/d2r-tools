# Act 1 Tristram (#276): whole-preset only

**Verdict: whole-preset only.** Effort **S** once the shared whole-preset slot exists (that first slot is **M**: see "Whole-preset route" below, which the other fixed-area notes link to).

Research note, 2026-11-12, from the vanilla build 3.3.0 tables read-only. No game run.

## How vanilla builds it

- Level 38 Tristram: DrlgType 2 (preset), LevelType 11, 43 x 48 tiles, Position 1, no Vis links, SubType -1 (no LvlSub pass).
- One LvlPrest row, Def 300 "Act 1 - Tristram", LevelId 38, **one DS1** (Tri_Town4). No piece rows exist for it, so there is nothing to recombine: the maze path (room pieces with matching doors) and the outdoor stamp planner (sockets) both need pieces and neither applies.
- Level 136 Pandemonium Finale (Act 5, Uber Tristram) is DrlgType 2, LevelType 11 and points at the **same DS1** (LvlPrest Def 1090). LevelType 11 is used by these two levels only, and no other tileset issue covers it.

## What a map could be

The same layout every time. Only the map Boss, the event, the monster list and the stats are Cartographer's to randomise. Boss and event rectangles would be fixed numbers per area, found in a game check.

## Whole-preset route (shared by #270, #271, #274, #276, #293)

- The installer adds a DrlgType 2 Act 5 slot level plus a LvlPrest row with the slot's own LevelId and the DS1 path. The old `Preset` tileset did exactly this with a copy of Colossal Summit (commit 948bf56; `docs/reverse-engineering/cartographer/03-phase1-parts.md` records the portal opening to it and back, live), so a preset slot level is **proven in game**; a DS1 other than Colossal Summit's is not.
- Vanilla already runs this exact DS1 as an Act 5 level (136): the best evidence that Act 1 Tristram tiles and objects load in an Act 5 slot.
- No engine hook is needed to build it: D2R builds a preset itself.
- New code: a "fixed plan" tileset kind (no layout graph; constant entrance, boss and event rectangles), a catalogue row per area, one slot row per area in `slots.txt`, the installer rows, README and `tables.md`.

## Game checks needed

1. A slot copy of level 38 with its own LvlPrest row opens, and the player arrives (the slot's Position is 0, so arrival falls to the entry finder's fallbacks; log where).
2. Rooms: how many rooms the preset level has and their rectangles (log room x, y, w, h), to choose the entrance, boss and event rectangles.
3. Quest objects and superuniques of level 38 do not misfire in a slot level (inference: that code is keyed by level id, so it stays off).
4. Boss and event spawn in their rectangles (existing populator hook).

## Closing option

Because the layout never varies, this is the weakest fit for a procedural map. Reasonable to defer behind landmarks inside generated maps (plan phase 9) or close as low value.
