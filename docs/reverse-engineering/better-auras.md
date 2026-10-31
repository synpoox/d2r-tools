# Better Auras native contracts (`better-auras`)

**Dates:** 2026-10-29 (aura system, hooks), 2026-10-30 (player regen, reservation cap), 2026-10-31 (select callers, slot test)
**Consumer:** [Better Auras](../../plugins/better-auras/README.md) (`better-auras.dll`)
**Module:** `D2R.exe`, D2R 3.3 build 93847 (Battle.net) / 94995 (Steam) under D2RLoader 1.3.1, PE image stamp
`0x6ab3782c`. Values are RVAs (image base `0x140000000` + RVA).
**Evidence source:** a memory-layout dump of the running game (d2rAI-toolkit `tools/dump_image.py`), analysed with
Ghidra 12.2 headless through d2rAI-toolkit `tools/ghidra_query.ps1` (`Decompile`, `Refs`, `Disasm`, `NearRefs`,
`StringXrefs`, `FindDisp`). Entry bytes were read from the dump at file offset = RVA. Skill table facts come from
the game's own `skills.txt` (93847).
**Reproduce:** `tools/ghidra_query.ps1 Decompile.java <rva>` / `Disasm.java <rva>` on a dump of the same image; or
`plugins/better-auras/tools/verify_dump.py <dump>`, which re-checks the hook guards, called functions and select callers.

**Validation, by level** (as of 2026-10-30):

- Clean build of every revision below; no unit tests for the hooks.
- In game: the owner installed the build before the regen rework (game-wide plugin, config in `d2rloader\config`) and
  reported that "some features work as intended" and that life drain did not work. Which rows that covers was not
  itemised, so no single contract below is marked validated in game.
- 2026-10-31, slot 2, town only (`plugins/better-auras/TESTING.md`): hooks `0x437460`, `0x43b680`, `0x438a7a` and `0x42e600` ran as
  described, including drain and the cap. `0x44db20` (death) did not run and `0x43b3e0` (cast trigger) was not reached; out-of-town tests are the owner's.

## 1. Hooks

Every hook checks its entry bytes through `InstallInlineHook`; a mismatch refuses that hook. All five aura hooks and
the regen hook are installed together; if any is refused, every feature stays off (the installed ones pass straight
through) and the part lines in the plugin log say which one. None of these entries started with `FF 25` in the dump.

| RVA | Guard (whole instructions) | Contract | Evidence |
|---|---|---|---|
| `0x437460` | `48 89 5C 24 08 48 89 6C 24 10 56 57 41 54 41 56 41 57` (18) | **Event 8 handler, aura upkeep tick** `void(game, unit, key, a4, a5)`. key -1: the current right skill (aura bit, unit alive) runs its srvdofunc through `0x43acb0` and is rescheduled. key > 0 (a skill id): runs only while the unit has the skill's aurastate and that list's stat 350 == key; in town without InTown it only reschedules; otherwise cleanup `0x432450`. `a5` must not be 0 or -1 (assert). Server game thread | Only referenced from the unit event handler tables `0x1d192b0` / `0x1d1a7a0` at index 8 (index 9 = `0x437230`, item auras) |
| `0x43b680` | `40 55 56 57 41 56 48 83 EC 48 48 8B FA 48 8B E9` (16) | **Start the right-skill aura** `void(game, unit, skill)`: aurastate stat list with stat 350 = skill, 351 = level, 202 = `0x3e2b64c(game)`, state on, event 8 key -1 via `0x43b2b0`. With record +0x25 bit 0x80 it runs the do-function (`0x43acb0` with force) instead of building the list | Called by `0x438a70` when the new right skill has the aura bit; decompiled |
| `0x438a7a` | `44 89 4C 24 20 55 41 56 41 57 48 83 EC 40` (14) | Body of **server skill select** `0x438a70 void(unit, isLeft, skill, a4)`. The entry is `CMP R8D,5 / JE rel32`, a relative branch, so the hook sits on the next instruction: arguments untouched, RSP = entry RSP; skill 5 never reaches it (vanilla returns). Right (isLeft 0): clears state 0x55, reads the old right skill (`0x34b400`), sets the new one (`0x33ef10`; left `0x33ec70`), sends the select packet (`0x47f080`); if the OLD right skill has the aura bit it removes its aurastate list (`0x2f5940` / `0x2f7920` / `0x2f4180`), clears the state and removes event 8 key -1; if the NEW one has it, `0x43b680` | Found from `0x34b400` (right skill getter) callers and the `0x43b680` caller; decompiled |
| `0x43b3e0` | `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20` (15) | Start a skill `int(game, unit, skill, level, force)` (already in d2rAI-toolkit's registry). Used for `trigger = "cast"` | In town (2026-10-31) two right clicks with an aura on the button never reached it. Outside town not tried |
| `0x44db20` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50` (24) | Unit death `void(game, victim, killer, flag)` (already in d2rAI-toolkit's registry) | |
| `0x42e600` | `48 89 5C 24 20 55 48 83 EC 40 41 8B C1 C7 44 24 30 00 00 00 00` (21) | **Player regen tick** `void(game, unit, a3, a4)`, see [section 4](#4-player-regen) | `D2Game\src\Player\PlrModes.cpp`; event 3, reschedules itself every frame |

**Failure mode:** a refused hook logs the reason and leaves every Better Auras feature off; the
game plays vanilla. **Coexistence:** no D2RLoader hook was found on these entries (no `FF 25`); another plugin
hooking the same entries makes this plugin's hook fail its byte check and stay off.

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

"Uses aura functions" in Better Auras: aura bit, or srvdofunc 65/66/81/82, and an aurastate >= 0. In 93847's
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

How Better Auras uses it: drain is a negative stat 74 (life) or 26 (mana) inside the aura's own state list; the
reservation cap runs after `0x42e600` and limits current life/mana to max - reserved.

## 5. Skill select callers

`0x438a70` has about 15 callers and the same arguments from each (`a4` -1), so only the return address tells them apart.
Read in game by logging it in the `0x438a7a` hook on 2026-10-31:

| Return RVA | Caller | Trigger |
|---|---|---|
| `0x4b451e` | `0x4b4250` | The player chooses a skill. Better Auras toggles only on this one |
| `0x5313f6` | `0x5311f0` | Character load: right skill, game frame 1 |
| `0x4bfdc7` / `0x4bfe16` | `0x4bea50` | Weapon swap: left, then right, same frame |

## 6. Client UI

| RVA | Notes |
|---|---|
| `0x8bd030` | HD UI setup that registers `SetShowLifeOrbText` / `SetShowManaOrbText`; too large for the decompiler's response buffer. The orb widget itself is not mapped |

## 7. Ghidra notes

- Ghidra 12.2 + JDK 25 on Windows: every headless run aborted with Felix "The data file must be inside the data dir"
  (NPE in `Felix.handleJavaVersionChange`). Workaround: set
  `JAVA_TOOL_OPTIONS=-Dapplication.settingsdir=<work dir>\settings` (and `JAVA_HOME`). Full auto-analysis of the
  93847 dump then took 9 minutes.
- `dump_image.py` needs the same rights as the game: OpenProcess error 5 when D2RLoader runs as administrator.

## 8. Open items

- In-game proof of the death hook; whether `0x43b3e0` sees aura use outside town. If not, drop `trigger = "cast"` and this hook.
- Whether auras started through the do-function branch of `0x43b680` (record +0x25 bit 0x80) build their list with
  stat 350; the plugin writes it itself when missing.
- The HD orb widget, for a tinted reserved band.
