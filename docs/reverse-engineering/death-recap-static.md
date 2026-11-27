# Death Recap — independently verified static capture contracts

2026-11-27. Analyzed the owner's unpacked D2RLoader memory-layout PE, 90,451,968 bytes, SHA-256 `c6c49d1c4bf2d8f496aca6c786047a3494e99a7560e81a19aed711946c7a12d2`, TimeDateStamp `0x6ab3782c`, image base `0x140000000`. All addresses below are RVAs, not absolute addresses. The dump was acquired privately; do not commit or redistribute it. Disassembly: GNU objdump -d -M intel, with --start-address and --stop-address set to image base + the desired RVA. Scanned masked byte patterns against all four PE sections with IMAGE_SCN_MEM_EXECUTE (.text, .maho, .koishi, .btext). Raw section offsets equal virtual offsets in this dump. Static verification is not live validation. D2RLoader 1.3.1 uses this same engine image with D2R 3.3 (93847) and 3.4 (95032) data; see [game-slot compatibility evidence](https://github.com/synpoox/pd2-resurrected/blob/main/docs/agents/game-slots.md).

## 1. Direct damage and resource accounting

HitEvents (0x44ce80) takes (game, attacker, victim, int32 areaOrMissile, damageBlock*) on Win64; the fifth parameter is loaded from [rbp+0x170] after the prologue. Its return is unused in inspected call paths. The fourth argument is not an invariant 1. DamageEffects (0x44df10) takes (game, attacker, victim, damageBlock*). HitEvents calls it at 0x44cf93 only on the nonzero flag branch; melee can compute effects upstream. Observe the calculation independently, not only inside HitEvents.

HitEvents orders attacker event calls (0x44cfc5 event 6 / 0x44cfe4 event 5), victim event (0x44cffc, 1/2), leech (0x44d038), absorption heal (0x44d05d), direct life debit (0x44d093), direct mana debit (0x44d0c9), stamina debit (0x44d10a). These are separate operations, not independent copies of a single damage amount. Event wrapper 0x44d570 reaches dispatcher thunk 0x5881e0, an FF 25 indirect jump via 0x3e2a420. Treat ExecuteEvents as a semantic lead, not a recovered symbol name.

- Absorption: 0x44d054 reads block+0x130; 0x44d05d calls 0x451820 (victim, int32 offeredHeal). AddLife returns actual amount added in EAX, including zero, after checks for death/state 92 and max-life clipping. Confirm with endpoint reads; don't count offered heal as realized heal. PD2 Sacrifice already redirects this precise call.
- Direct life: 0x44d062 reads signed block+0x134; only positive values proceed. 0x44d076 reads stat 6 with layer 0 via 0x2f5020; subtracts the damage, sets stat 6 at 0x44d093 via 0x2f7d10. A remaining amount below 0x100 (one HP) is flushed to zero. Capture confirmed before/after life and attempted debit separately. Overkill for this isolated direct debit = max(0, attempted debit - life immediately before debit), not attempted minus actual loss. Fractional flush can make actual loss greater than attempted debit. Avoid claiming overkill for other writers until their rules are observed.
- Direct mana: 0x44d0a2 through 0x44d0c9 is the corresponding stat 8 path, based on block+0x124; same threshold. This is a mana drain path, not proof of Energy Shield. A shield can consume mana in an earlier event.
- Stat reader 0x2f5020 (unit, statId, layer) and setters 0x2f7d10 (set), 0x2f34f0 (add), each (unit, statId, int32 amount, layer), are D2RCore thunks in this dump, using slots 0x3e2a218 / 0x3e2a248 / 0x3e2a250 respectively. Do not inline-hook their jump stubs or overwrite their slots. The supplied engine image exposes these functions through indirect targets. Follow the resolved targets and SDK contracts during testing as needed; a separate module dump is not a prerequisite. Distinct original E8 call sites are preferred for a narrow first probe; redirect with SDK ownership/expected bytes checks, preserve original ABI and result. Read after original returns; do not infer gross change from requested stat value.

## 2. Actual damage block layout and stage boundaries

Confirmed from DamageEffects field setup, arithmetic and existing toolkit damage.h:

| Offset | Meaning |
|---|---|
| 0x00 / 0x04 | hit flags u32 / result flags u16 |
| 0x18 | physical |
| 0x20 | fire |
| 0x2c | lightning |
| 0x30 | magic |
| 0x34 | cold |
| 0x38 / 0x3c | poison rate / duration, not instant poison HP lost |
| 0x120 / 0x124 / 0x128 | life/mana/stamina leech or drain fields; not necessarily realized resource deltas |
| 0x130 | absorption heal offered |
| 0x134 | damage total at the specific stage where sampled |

DamageEffects builds its type descriptors at 0x44e142 onward; its explicit summation at 0x44ec71..0x44ec99 writes +0x134. Event 16 is dispatched at 0x44eca5, followed by a conditional adjustment at 0x44ecb6. This is a concrete calculation boundary, not proof of a separately named CalculateTotalDamage export. Sample per-element inputs before effects and outputs after effects, and sample final attempted direct debit at its own commit point; further callbacks can modify the block or resources. Do not sum poison rate into immediate HP loss or proportionally distribute actual loss across elements. The toolkit describes a 0x180-byte block; never copy classic D2DamageStrc wholesale.

## 3. Player death — corrected branch

The previously proposed 0x44db20 alone is NOT the player death detector. In 0x44a9b0 the monster branch calls it at 0x44aaff; the player branch begins at 0x44ad9a, checks result bit 2 at 0x44aee3, rejects already-dead modes and a protection flag, extracts attacker ID/type, then calls 0x42d020 at 0x44af4a. That call passes (game, victim, null skill, mode=0, attackerType, attackerId, 0, 0); all eight argument slots matter. Another branch calls 0x42d020 with mode 19 at 0x44af9f, so entry alone does not imply death.

0x42d020 is a generic player-mode dispatcher: it validates the transition through 0x42bfb0, resolves attacker/target through 0x48fe80, and dispatches at 0x42d29c using three-qword records at 0x1d18fb8. Mode 0 record first pointer = 0x429d10. That handler reaches the death cleanup call at 0x42a47a. Cleanup 0x42e7c0 (game, unit) explicitly sets life to zero at 0x42e7dd, followed by stat/state and event cleanup. Other callers are 0x42a6fd and 0x42e2c2, following mode-17 dispatch at 0x42a6ea / 0x42e2af. Confirm local identity and living-to-dying/dead transition; don't count repeated cleanup, mode-17 follow-up, or nondeath mode transitions. Capture identity and previous endpoints before cleanup. Link fatal event only when causal context matches; defer freeze until enclosing fatal transaction completes. Inspect mode after original; live test required for ordering and unusual deaths. SDK upstream lifecycle header exposes LocalPlayerReady / GameLeft / PlayerResurrected and MonsterDeath, not a player-death event; verify the pinned version.

## 4. Persistent damage and regeneration

Player regen 0x42e600 reschedules event 3; 0x42e657 reads signed stat 74 (hpregen), adds it to life and applies cap / state removal. 0x42e6ba..0x42e6d1 clamps to at least 0x100 and commits stat 6: this particular pathway cannot by itself establish a fatal poison or OW tick. Observe before/after and net only when simultaneous healing and damage are unresolved. Mana step 0x42e350 reads stat 26, computes clamped delta, calls AddStat stat 8 at 0x42e540; HitEvents' direct mana debit is separate. Do not count the parent regen and child mana step twice.

PoisonApply at 0x4518c0 checks positive duration, body 0x4518c9 has original arguments intact; callers at 0x44d380 and 0x44d21a pass attacker, victim, rate, length and three additional parameters; latter iterates 12-byte records. At 0x451a71 the statlist creation has attacker type/id. Track applied state/list identities, expiry and replacement; poison and OW can coexist with positive regeneration. D2RCore/mod-specific OW stacking and damage attribution require more evidence. PD2 core-rules' crushing_blow.cpp directly debits stat 6 and sets result bit 2 on a kill; the stock D2RCore item-event copy can bypass 0x581bd0 (PD2 research #446). Thus the direct hit commit points are deliberately insufficient for all mechanics. Never report them as a complete feed.

## 5. Source and skill attribution

AreaApply 0x436e90 resolves missile to owner at 0x436ed5, passes owner to HitEvents and DealDamage at 0x436f95 / 0x436fa6. Direct missile path 0x462e40 resolves owner at 0x462ea2, calls HitEvents and DealDamage at 0x4630ac / 0x4630ce. Retain immutable missile identity before replacement; later block and attacker alone do not preserve it. MissileOwner 0x490300 (game, missile) checks ownership flag 0x400 and reads type/id at +0xe8/+0xec to resolve current unit; null and despawn are valid. MissileSkill 0x3bc450 reads signed int16 at missile unit's data pointer (+0x10), then +0x0c; zero may mean absent data OR valid skill 0. Validate data pointer and active skill table. Never substitute owner's current skill at missile arrival. PD2 already hooks both missile entry points; do not compete. Pet owner and modifiers remain candidates, resolve active tables and verify before using.

## 6. Verified signature catalog

Every masked pattern below matched exactly once across executable PE sections of the supplied image. Call-site patterns resolve E8 rel32 target and must be checked against the expected target (including legitimate SDK-managed redirections); never treat a pattern match as permission to replace an owned call. Entry patterns are identification only; verify executable ranges, instruction boundaries, expected bytes and mutation ownership before enabling. No blanket PE-stamp gate. Offsets are hexadecimal except explicitly labeled byte counts.

| Purpose | Pattern start | Verified hook/call | Pattern |
|---|---|---|---|
| HitEvents | 44ce80 | entry, first 17 bytes | 40 55 53 56 57 41 56 41 57 48 8D AC 24 E8 FE FF FF 48 81 EC 18 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 00 01 00 00 48 8B BD 70 01 00 00 41 8B D9 |
| DamageEffects | 44df10 | entry, first 21 bytes | 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 68 FD FF FF 48 81 EC 98 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 80 02 00 00 4C 8B EA 48 89 4C |
| Life debit | 44d06c | +27 -> 2f7d10 | 45 33 C0 48 8B CE 41 8D 50 06 E8 ?? ?? ?? ?? 2B C3 45 33 C9 45 33 C0 48 8B CE 3D 00 01 00 00 44 0F 4D C0 41 8D 51 06 E8 ?? ?? ?? ?? |
| Mana debit | 44d0a2 | +27 -> 2f7d10 | 45 33 C0 48 8B CE 41 8D 50 08 E8 ?? ?? ?? ?? 2B C3 45 33 C9 45 33 C0 48 8B CE 3D 00 01 00 00 44 0F 4D C0 41 8D 51 08 E8 ?? ?? ?? ?? |
| AddLife | 451820 | entry, first 15 bytes | 48 89 5C 24 18 56 48 83 EC 20 8B F2 48 8B D9 85 D2 7E 7C E8 ?? ?? ?? ?? 85 C0 75 73 8D 50 5C |
| Regen life commit | 42e6ba | +17 -> 2f7d10 | B8 00 01 00 00 48 8B CB 3B F8 0F 4C F8 45 33 C9 44 8B C7 41 8D 51 06 E8 ?? ?? ?? ?? |
| Regen mana commit | 42e533 | +d -> 2f34f0 | 45 33 C9 44 8B C6 48 8B CF 41 8D 51 08 E8 ?? ?? ?? ?? |
| Fatal mode call | 44af2c | +1e -> 42d020 | 33 F6 45 33 C9 89 74 24 38 45 33 C0 89 74 24 30 48 8B D7 89 5C 24 28 48 8B CD 89 44 24 20 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? |
| Player mode dispatcher | 42d020 | entry, first 17 bytes | 4C 8B DC 4D 89 43 18 53 55 56 57 41 54 41 55 41 56 48 83 EC 40 45 8B F1 49 8B C0 48 8B DA 48 8B F9 |
| Death cleanup | 42e7c0 | entry, first 13 bytes (extend to minimum SDK length) | 48 89 5C 24 08 57 48 83 EC 20 48 8B DA 45 33 C9 48 8B F9 45 33 C0 48 8B CB 41 8D 51 06 |
| PoisonApply body | 4518c9 | first 16 bytes | 48 8B C4 55 56 41 56 41 57 48 83 EC 68 45 8B F1 41 8B E8 48 8B F2 4C |

The 24-byte 0x44db20 guard previously suggested in the overview matched **24 locations**, not unique. Do not use it as a discovery signature.

## 7. Coexistence and remaining gate

Cross-checked PD2 Resurrected's `plugin/core-rules/skill/leech.cpp`, `sacrifice.cpp`, `sacrifice_missile.cpp`, and `item/crushing_blow.cpp`, plus toolkit `shared/plugin-common/src/rvas_93847/damage.h`. Existing DPS Counter owns DealDamage entry; PD2 leech owns HitEvents entry; Sacrifice owns absorb call and monster kill call; missile variants own area/direct entries. Start with verified direct resource call sites that remain unowned, a death observer and endpoint sampler. Check actual runtime ownership including Cartographer/Better Auras. If a required interception is occupied, fail closed for that telemetry, or add a shared read-only observer cooperatively in the owning plugin. Do not modify foreign hooks, replay events, or alter gameplay.

Static binary work has substantially narrowed the remaining RE. Remaining implementation and validation work: actual local server/client lifecycle and thread identity; alternate mutation/event paths (follow resolved targets using the existing image and live test tooling when needed); proof of coverage across custom damage, ES, persistent effects, potion timing, max-resource changes, and alternative death routes; per-mod coexistence; controlled live trace verification. Proceed using the existing resources; do not treat a separate dump as an acquisition blocker. Static analysis did not establish host-to-client availability of internal damage data. No Windows runtime was available. No native capture hook was enabled by this research.
