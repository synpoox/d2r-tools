# DPS Counter: native findings

Build: D2R 3.3 build 93847 under D2RLoader 1.3.1, image stamp `0x6ab3782c`. Evidence: the in-memory dump
`D2RLoader_6ab3782c_dump.exe` (file offset = RVA) and d2rAI-toolkit's Ghidra project of it.

| RVA | Signature | Contract | Evidence | Guard / failure |
|---|---|---|---|---|
| `0x44a9b0` | `40 53 55 57 41 56 48 81 EC 88 00 00 00 49 8B D9 49 8B F8 4C 8B F2 48 8B E9 E8 ?? ?? ?? ?? 85 C0 75 0C F7 03 00 10 00 00 0F 84` (1 match) | hit results `(game, attacker, victim, &damage)`, run after the life is taken; `damage+0x134` is the hit's total damage in 1/256 life; **hooked** (first 25 bytes, no relative operand) | Ghidra: asserts name `D2Game\src\Unit\SUnitDmg.cpp`; 35 call sites by byte scan, among them melee `0x44b2b0`, area `0x436e90`, missile hit `0x464610`, damage auras `0x561280`. Callers run `0x44ce80` (the damage itself) first, then it (melee `0x44b2b0`, `0x4395e0`); inside, `0x451fb0` reads `+0x134 >> 8` against max life (stat 7 `>> 8`) | signature or hook refused: the plugin stays off, logged |
| `0x490300` | `48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 48 85 D2 75 20 48 8D 4C 24 38 88 54 24 38 E8 ?? ?? ?? ?? 84 C0 74 1F CC` (1 match) | `(game, missile) -> owner`; called only for a type 3 attacker | d2rAI-toolkit `knowledge/d2r-93847-addresses.md` | off with the hook |
| `0x846170`, `0x856220`, `0x86dd50` | player-bars' FindPanel, FindChild and SetText signatures | the text widget | player-bars.md | off with the hook |

The hook counts the record's total at `+0x134` (reading the victim's life around the call gave 0: the life is already
taken when `0x44a9b0` runs). It counts a
hit when the victim is a monster (unit type 1 at `+0`) and the attacker is a player, or a missile a player owns.

Not validated: in game. Poison and other damage over time do not pass through `0x44a9b0` and are not counted (not
traced). Whether missile hits pass the missile or its owner as the attacker is not traced; both are handled.
