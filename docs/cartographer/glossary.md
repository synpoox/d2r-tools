# Cartographer glossary

Terms used in the Cartographer docs. Copied from pd2-resurrected `GLOSSARY.md` (PR #941 head 3a82796, 2026-11-11).

**Cartographer**:
The standalone D2RLoader plugin that opens procedurally laid-out maps from a mod author's `maps.txt`. It shares no code with `core-maps` and is never loaded with it ([ADR 0001](adr-0001-cartographer-standalone.md)).
_Avoid_: map plugin, map kit

**Map record**:
The game's per-game map state in Cartographer: which map is open, its seed, rolled properties and event. It lives for one game. A row of `maps.txt` is a *map row*, not a map record.
_Avoid_: map definition, map entry

**Slot level**:
One of Cartographer's own Act 5 levels with no links. A map is built in one, so a mod author never edits `levels.txt`.
_Avoid_: map level, spare level

**Layout graph**:
The plan of a map's rooms and how they connect, solved from its shape and size before the level is built. The level is then built to follow it.
_Avoid_: map layout, room list

**Tileset**:
In Cartographer, the one set of room pieces a map is built from, named in plain words such as `Cave` or `Jungle`. It decides whether the map is a maze or outdoor.
_Avoid_: level type, theme

**Shape**:
In Cartographer, the feel of a map's path: `Corridor`, `Winding`, `Branching`, `Loop` or `Sprawl`. It decides the layout graph.
_Avoid_: layout type, pattern
