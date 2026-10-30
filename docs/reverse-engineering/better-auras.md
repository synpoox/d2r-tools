# Better Auras native contracts (`better-auras`)

**Dates:** 2026-10-29 (aura system, hooks), 2026-10-30 (player regen, reservation cap)
**Consumer:** Better Auras (planned toggled-aura plugin); no plugin code is part of this record
**Module:** `D2R.exe`, D2R 3.3 build 93847 (Battle.net) / 94995 (Steam) under D2RLoader 1.3.1, PE image stamp
`0x6ab3782c`. Values are RVAs (image base `0x140000000` + RVA).
**Evidence source:** a memory-layout dump of the running game (d2rAI-toolkit `tools/dump_image.py`), analysed with
Ghidra 12.2 headless through d2rAI-toolkit `tools/ghidra_query.ps1` (`Decompile`, `Refs`, `Disasm`, `NearRefs`,
`StringXrefs`, `FindDisp`). Entry bytes were read from the dump at file offset = RVA. Skill table facts come from
the game's own `skills.txt` (93847).
**Reproduce:** `tools/ghidra_query.ps1 Decompile.java <rva>` / `Disasm.java <rva>` on a dump of the same image; no
dump checker for this plugin yet (see [Open items](#7-open-items)).

**Validation, by level** (as of 2026-10-30): static analysis of the dump only. No contract below is validated in game,
and no plugin code is part of this record.

## 1. Interception points

Proposed hook sites for a toggled-aura system, each with a guard of whole instructions of at least 14 bytes and no
RIP-relative instruction. None of these entries started with `FF 25` in the dump. A plugin using them should check the
image stamp first, install them in one transaction, and on any mismatch log it and leave every aura feature off.

| RVA | Guard (whole instructions) | Contract | Evidence |
|---|---|---|---|
| `0x437460` | `48 89 5C 24 08 48 89 6C 24 10 56 57 41 54 41 56 41 57` (18) | **Event 8 handler, aura upkeep tick** `void(game, unit, key, a4, a5)`. key -1: the current right skill (aura bit, unit alive) runs its srvdofunc through `0x43acb0` and is rescheduled. key > 0 (a skill id): runs only while the unit has the skill's aurastate and that list's stat 350 == key; in town without InTown it only reschedules; otherwise cleanup `0x432450`. `a5` must not be 0 or -1 (assert). Server game thread | Only referenced from the unit event handler tables `0x1d192b0` / `0x1d1a7a0` at index 8 (index 9 = `0x437230`, item auras) |
| `0x43b680` | `40 55 56 57 41 56 48 83 EC 48 48 8B FA 48 8B E9` (16) | **Start the right-skill aura** `void(game, unit, skill)`: aurastate stat list with stat 350 = skill, 351 = level, 202 = `0x3e2b64c(game)`, state on, event 8 key -1 via `0x43b2b0`. With record +0x25 bit 0x80 it runs the do-function (`0x43acb0` with force) instead of building the list | Called by `0x438a70` when the new right skill has the aura bit; decompiled |
| `0x438a7a` | `44 89 4C 24 20 55 41 56 41 57 48 83 EC 40` (14) | Body of **server skill select** `0x438a70 void(unit, isLeft, skill, a4)`. The entry is `CMP R8D,5 / JE rel32`, a relative branch, so the hook sits on the next instruction: arguments untouched, RSP = entry RSP; skill 5 never reaches it (vanilla returns). Right (isLeft 0): clears state 0x55, reads the old right skill (`0x34b400`), sets the new one (`0x33ef10`; left `0x33ec70`), sends the select packet (`0x47f080`); if the OLD right skill has the aura bit it removes its aurastate list (`0x2f5940` / `0x2f7920` / `0x2f4180`), clears the state and removes event 8 key -1; if the NEW one has it, `0x43b680` | Found from `0x34b400` (right skill getter) callers and the `0x43b680` caller; decompiled |
| `0x43b3e0` | `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20` (15) | Start a skill `int(game, unit, skill, level, force)` (already in d2rAI-toolkit's registry). Used for `trigger = "cast"` | Whether using an aura skill reaches it is **not proven** |
| `0x44db20` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50` (24) | Unit death `void(game, victim, killer, flag)` (already in d2rAI-toolkit's registry) | |
| `0x42e600` | `48 89 5C 24 20 55 48 83 EC 40 41 8B C1 C7 44 24 30 00 00 00 00` (21) | **Player regen tick** `void(game, unit, a3, a4)`, see [section 4](#4-player-regen) | `D2Game\src\Player\PlrModes.cpp`; event 3, reschedules itself every frame |

**Failure mode (required of a consumer):** a refused hook or a stamp mismatch logs the reason and leaves every aura
feature off; the game plays vanilla. **Coexistence:** no D2RLoader hook was found on these entries (no `FF 25`); a
second hook on the same entry fails the byte check and must stay off.

## 2. Aura system (server)

Code in `D2Game\src\Skills\Skills.cpp` and `SkilPal.cpp`.

| RVA | Contract | Notes |
|---|---|---|
| `0x43b2b0` | `(game, unit, skill, level, a5, rightSkill)` | (Re)schedules event 8 at the next multiple of perdelay (record +0x180 formula, at least 5 frames). rightSkill 1: key -1; 0: key = skill id, an independent per-skill timer that a skill switch does not remove |
| `0x48b720` | `(game, unit, type, frame, key, a6, a7)` | Add a unit timer event |
| `0x48b890` | `(game, unit, type, key)` | Remove the unit's timer events of a type: key 0 = all, else only events whose key (`+0x18`) == key |
| `0x432c40` | `(game, unit, record, skill, level) -> 0 apply now` | Periodic gate inside aura do-functions (game frame at game `+0x170`) |
| `0x432ae0` / `0x4327d0` | | Aura stat list on the caster (callback `0x5616f0`) / area aura on targets (aurafilter +0x70, rangecalc +0x84) |
| `0x436a80` | `(unit, cost) -> 0 not paid` | Aura mana upkeep (players only; with state 0x72 it pays life through `0x584e80`). Entry `48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9` |
| srvdofunc 65 / 66 / 81 / 82 | `0x563d90` / `0x5630e0` / `0x563720` / `0x564260` | Stat aura / damage aura / Holy Freeze / Redemption (table `0x238ea00`) |
| `0x34b400` | `(unit) -> skill entry` | Right skill (unit `+0x100` skill list); `0x33e080(entry, file, line)` -> skill id |
| `0x33dd40` / `0x33d1e0` | `(unit, skill, 0) -> entry` / `(unit, entry, 1) -> level` | Skill entry / level with bonuses |
| `0x48ff00` | `(unit) -> game` | Reads unit `+0xd8` |
| `0x34c2c0` | `(unit) -> nonzero dead` | Flag `+0x124` bit `0x10000`, or a player in mode 0 / 17 |
| `0x34b440` / `0x2f0750` | `(unit) -> room` / `(room) -> nonzero town` | The town test `0x43b3e0` and `0x437460` use |
| `0x3e2b64c`, `0x3e2b652` | | Import thunks (`jmp [rip+x]`); vanilla passes `0x3e2b64c(game)` as the tick's `a5` |
| `0x2f5940`, `0x2f7920`, `0x2f4180` | | State list lookup `(unit, state)`, detach `(unit, list)`, free `(dataFlag, list)`: how vanilla removes an aura's list |
| `0x2f7c00`, `0x2f5df0` | | Set / read a stat inside a list `(dataFlag, list, stat, value, layer)` / `(dataFlag, list, stat, layer)` |
| `0x3354c0`, `0x3351b0` | | Set state `(unit, state, on)` / has state `(unit, state)` |

## 3. skills.txt record (748 bytes)

| Offset | Field | Evidence |
|---|---|---|
| +0x24 | bit 0x20 = `aura` (the bit `0x438a70` and `0x43b680` test), bit 0x40 = `periodic` | Consistent with every paladin aura (`aura=1`) and Thunder Storm / Blade Shield (`periodic=1`) in skills.txt; inferred, no direct column-to-bit table |
| +0x25 | bit 0x01 InTown | d2rAI-toolkit registry |
| +0x4e | u16 srvdofunc | d2rAI-toolkit registry |
| +0xa0 / +0xa2 | i16 aurastate / auratargetstate | d2rAI-toolkit registry |
| +0x180 | perdelay formula | `0x43b2b0` |

A workable "uses aura functions" rule: aura bit, or srvdofunc 65/66/81/82, and an aurastate >= 0. In 93847's
skills.txt that is every paladin aura plus Barbs/Wolverine/Oak Sage and the monster holy auras; the barbarian
masteries have `aura=1` but no srvdofunc and are excluded.

## 4. Player regen

| RVA | Contract | Notes |
|---|---|---|
| `0x42e600` | `void(game, unit, a3, a4)` | Life += stat 74 hpregen (<< 8 per frame), at most max life (`0x2f4d20`), at least `0x100` (negative hpregen drains, never kills); removes the state-100 list (potion heal) when full; party life byte stat `0x160` via `0x539270`; then `0x42e850` and the mana step |
| `0x42e350` | `(game, unit) -> nonzero changed` | Mana step: unless state 0x55, rate = max mana (`0x2f4e20`) / (charstats ManaRegen × 25, 7500 when 0), at least 1, × (100 + stat 27) / 100; + stat 26 manarecovery (flat << 8 per frame); clamped to [-mana, max - mana]: negative stat 26 drains down to 0. Removes the state-0x6a list when full |
| `0x2f4d20` / `0x2f4e20` | `(unit) -> max life / max mana` (<< 8) | `0x2f4d20` starts with `jmp [rip+x]` (D2RCore owns it); `0x2f4e20` is an import thunk; both are also called by client UI code. Not hooked: changing them would change what the player sees |
| state 0x55 | | Set by the stat-aura function while an aura with a mana cost runs: pauses base mana regen |
| charstats | | Record 0xd0 bytes (data manager `+0x1240` records, `+0x1248` count); `+0x36` u8 ManaRegen (seconds) |

Found from the `ManaRegen` column name in the charstats parser `0x30c560` (entry `{name, type 4, offset 0x36}`), then
`FindDisp.java 0x36 MOVZX` → `0x42e350` (reads `+0x36` and stats 8, 26, 27) → its only caller `0x42e600`.

What this allows (design, not implemented here): a drain as a negative stat 74 (life) or 26 (mana) inside an aura's
own state list, applied every frame by the engine; a reservation as a cap after `0x42e600` limiting current life/mana
to max - reserved, which leaves the orb's reserved part empty without changing max.

## 5. Client UI

| RVA | Notes |
|---|---|
| `0x8bd030` | HD UI setup that registers `SetShowLifeOrbText` / `SetShowManaOrbText`; too large for the decompiler's response buffer. The orb widget itself is not mapped |

## 6. Ghidra notes

- Ghidra 12.2 + JDK 25 on Windows: every headless run aborted with Felix "The data file must be inside the data dir"
  (NPE in `Felix.handleJavaVersionChange`). Workaround: set
  `JAVA_TOOL_OPTIONS=-Dapplication.settingsdir=<work dir>\settings` (and `JAVA_HOME`). Full auto-analysis of the
  93847 dump then took 9 minutes.
- `dump_image.py` needs the same rights as the game: OpenProcess error 5 when D2RLoader runs as administrator.

## 7. Open items

- A dump checker (workflow step 6) re-checking every guard above.
- In-game proof of each row; whether `0x43b3e0` sees aura use.
- Whether auras started through the do-function branch of `0x43b680` (record +0x25 bit 0x80) build their list with
  stat 350, which the per-skill tick requires.
- The HD orb widget, for a tinted reserved band.
