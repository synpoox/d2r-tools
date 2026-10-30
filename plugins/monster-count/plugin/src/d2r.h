// D2R game image interface: RVAs, record offsets, structures and the game functions a plugin calls.
//
// EVERYTHING IN THIS FILE IS FOR ONE GAME IMAGE: D2R 3.3.0 build 93847 running under D2RLoader 1.3.1 (PE header
// TimeDateStamp 0x6ab3782c, checked by plugin.cpp before any of it is used). The values come from the Ghidra
// analysis of the dumped image (tools/dump_image.py, tools/ghidra_analyze.ps1) and were used in game by a
// large overhaul port. A different D2R patch or D2RLoader release moves them: re-find each one before
// changing ExpectedImage in plugin.cpp.
//
// Calling convention: plain Windows x64 (rcx, rdx, r8, r9, then stack). Unit/Game are opaque; read fields with At<T>.
#pragma once

#include <cstddef>
#include <cstdint>

namespace d2r {

struct Game;
struct Unit;

// --- Tables (RVAs, build 93847) -----------------------------------------------------------------------------------
// Server skill "do" functions, indexed by skills.txt srvdofunc: 191 slots of 8-byte pointers. D2R 3.3 fills slots up
// to 171; 172-190 are empty (zero) and free for a mod's own functions (skill_functions.cpp).
inline constexpr uint64_t SrvDoTable = 0x238ea00;
inline constexpr int SrvDoSlots = 191;
inline constexpr int SrvDoFirstFree = 172;

// --- Functions (RVAs, build 93847), signature in the comment ------------------------------------------------------
inline constexpr uint64_t RvaGetSkillRecord = 0x97790;    // (dataFlag, skillId) -> skills.txt record or null
inline constexpr uint64_t RvaEvalSkillCalc = 0x3b5160;    // (dataFlag, unit, calc, skillId, level) -> value
inline constexpr uint64_t RvaSkillTargetXY = 0x432b70;    // (game, unit, &x, &y) -> non-zero when the skill has a target point
inline constexpr uint64_t RvaSkillTargetUnit = 0x48fe20;  // (game, unit) -> the skill's target unit or null
inline constexpr uint64_t RvaCreateMissile = 0x5371a0;    // (game, &MissileParams) -> missile unit or null
inline constexpr uint64_t RvaMissileRecord = 0x166040;    // (dataFlag, missileId) -> missiles.txt record or null
inline constexpr uint64_t RvaMissileCount = 0x1b89a0;     // (dataFlag) -> rows in missiles.txt
inline constexpr uint64_t RvaUnitSeed = 0x34a1e0;         // (unit) -> &seed {lo, hi} (D2 RNG, see Roll())
inline constexpr uint64_t RvaSetUnitSkillFlag = 0x34e790; // (unit, on): unit flags (+0x124) bit 0x40, set by do functions
inline constexpr uint64_t RvaApplyMeleeHit = 0x44b2b0;    // (game, attacker, target, 0, 1): the queued melee hit
inline constexpr uint64_t RvaUnitStat = 0x2f5020;         // (unit, stat, param) -> value (stat with a parameter/layer)
inline constexpr uint64_t RvaUnitStatNoParam = 0x2f5c60;  // (unit, stat) -> value
inline constexpr uint64_t RvaSummonPet = 0x4350e0;        // (game, &SummonParams) -> pet; registers it, enforces petmax
inline constexpr uint64_t RvaSetPetLevel = 0x55fb50;      // (game, owner, pet, 0, skillLevel): pet level from the owner's
inline constexpr uint64_t RvaPetSkillStats = 0x55ef80;    // (game, owner, pet, skill, level, owner+0x12c, 0): the skill's
                                                          // passivestat/aurastat/aurastate on the pet
inline constexpr uint64_t RvaAddPetToParty = 0x58d950;    // (game, pet, 0, owner+0x130)

// --- Offsets (build 93847) ----------------------------------------------------------------------------------------
inline constexpr int GameDataFlag = 0x106;  // Game: byte passed to every data-table getter

inline constexpr int UnitType = 0x00;    // int32: 0 player, 1 monster, 2 object, 3 missile, 4 item, 5 tile
inline constexpr int UnitPath = 0x38;    // dynamic path (x +0x2, y +0x6, uint16) or static path (x +0x10, y +0x14)
inline constexpr int UnitArg12c = 0x12c; // int32 passed by D2R's summon skills to RvaPetSkillStats
inline constexpr int UnitArg130 = 0x130; // int32 passed to RvaAddPetToParty

// skills.txt record (748 bytes in D2R; offsets differ from 1.10/1.13c)
inline constexpr int SkPrgCalc1 = 0x58;       // int32 calc index (pass to EvalCalc)
inline constexpr int SkSrvMissile = 0x66;     // int16 srvmissile
inline constexpr int SkSrvMissileA = 0x68;    // int16 srvmissilea; srvmissileb +0x6a, srvmissilec +0x6c
inline constexpr int SkAuraRangeCalc = 0x84;  // int32 calc index
inline constexpr int SkSummon = 0x110;        // int16 monstats row
inline constexpr int SkPetType = 0x112;       // uint8 pettype row
inline constexpr int SkSumMode = 0x113;       // uint8 monmode, 255 = none
inline constexpr int SkPetMaxCalc = 0x114;    // int32 calc index
inline constexpr int SkCalc1 = 0x190;         // int32 calc index
inline constexpr int SkCalc2 = 0x194;
inline constexpr int SkCalc3 = 0x198;

// --- Structures (build 93847) -------------------------------------------------------------------------------------
// Missile creation parameters for RvaCreateMissile. 1.10's 0x5c-byte block, shifted by 0x10 after the three pointers.
struct MissileParams {
	uint32_t flags;           // 0x00 (seen: 0x820/0x420 caster -> point, 0x21 start at x/y, 1 placed at x/y)
	uint32_t pad04;
	Unit* owner;              // 0x08
	Unit* origin;             // 0x10 (start at this unit's position when x/y are 0)
	Unit* target;             // 0x18
	int32_t missile;          // 0x20 missiles.txt row
	int32_t x, y;             // 0x24 start (0: the origin's position)
	int32_t targetX, targetY; // 0x2c aim point
	int32_t gfxArg;           // 0x34
	int32_t velocity;         // 0x38
	int32_t skill;            // 0x3c
	int32_t skillLevel;       // 0x40
	int32_t loops;            // 0x44
	int32_t unk48;            // 0x48
	int32_t frame;            // 0x4c
	int32_t activateFrame;    // 0x50
	int32_t attBonus;         // 0x54 (1.10 +0x44)
	int32_t unk58;            // 0x58
	int32_t range;            // 0x5c (1.10 +0x4c): used with flag 0x8000
	uint8_t unk60[0x70 - 0x60];
	int32_t initKind;         // 0x70: replaces 1.10's init function pointer (1 = charged-strike bolt, 4 = multishot)
	int32_t initArg;          // 0x74: e.g. the missile's index
	uint8_t rest[0xa0 - 0x78];
};
static_assert(sizeof(MissileParams) == 0xa0);
static_assert(offsetof(MissileParams, missile) == 0x20);
static_assert(offsetof(MissileParams, targetX) == 0x2c);
static_assert(offsetof(MissileParams, skill) == 0x3c);
static_assert(offsetof(MissileParams, attBonus) == 0x54);
static_assert(offsetof(MissileParams, range) == 0x5c);
static_assert(offsetof(MissileParams, initKind) == 0x70);

// Pet summon parameters for RvaSummonPet (1.10's block with an 8-byte owner pointer).
struct SummonParams {
	uint32_t flags;    // 0x00: 1 = at x/y (else at the owner)
	uint32_t pad04;
	Unit* owner;       // 0x08
	uint16_t monster;  // 0x10 monstats row
	uint16_t pad12;
	int32_t aiState;   // 0x14
	int32_t mode;      // 0x18 monmode
	int32_t x, y;      // 0x1c
	int32_t petType;   // 0x24
	int32_t petMax;    // 0x28
	int32_t pad2c;
};
static_assert(sizeof(SummonParams) == 0x30);
static_assert(offsetof(SummonParams, x) == 0x1c);
static_assert(offsetof(SummonParams, petMax) == 0x28);

// --- Function types -----------------------------------------------------------------------------------------------
using GetSkillRecordFn = uint8_t* (*)(uint32_t dataFlag, int32_t skillId) noexcept;
using EvalSkillCalcFn = int32_t (*)(uint32_t dataFlag, Unit* unit, int32_t calc, int32_t skillId, int32_t level) noexcept;
using SkillTargetXYFn = int32_t (*)(Game* game, Unit* unit, int32_t* x, int32_t* y) noexcept;
using SkillTargetUnitFn = Unit* (*)(Game* game, Unit* unit) noexcept;
using CreateMissileFn = Unit* (*)(Game* game, MissileParams* params) noexcept;
using MissileRecordFn = const uint8_t* (*)(uint32_t dataFlag, int32_t missileId) noexcept;
using MissileCountFn = int32_t (*)(uint32_t dataFlag) noexcept;
using UnitSeedFn = uint32_t* (*)(Unit* unit) noexcept;
using SetUnitSkillFlagFn = void (*)(Unit* unit, int32_t on) noexcept;
using ApplyMeleeHitFn = void (*)(Game* game, Unit* attacker, Unit* target, int32_t bonus, int8_t extra) noexcept;
using UnitStatFn = int32_t (*)(Unit* unit, int32_t stat, int32_t param) noexcept;
using UnitStatNoParamFn = int32_t (*)(Unit* unit, int32_t stat) noexcept;
using SummonPetFn = Unit* (*)(Game* game, SummonParams* params) noexcept;
using SetPetLevelFn = int64_t (*)(Game* game, Unit* owner, Unit* pet, int32_t levelOverride, int32_t skillLevel) noexcept;
using PetSkillStatsFn = int64_t (*)(Game* game, Unit* owner, Unit* pet, int32_t skillId, int32_t level, int32_t arg6,
                                    int64_t arg7) noexcept;
using AddPetToPartyFn = void (*)(Game* game, Unit* pet, int32_t zero, int32_t ownerArg) noexcept;

// Bound by Bind() from the image base (plugin.cpp, after the image check).
struct Api {
	GetSkillRecordFn getSkillRecord;
	EvalSkillCalcFn evalSkillCalc;
	SkillTargetXYFn skillTargetXY;
	SkillTargetUnitFn skillTargetUnit;
	CreateMissileFn createMissile;
	MissileRecordFn missileRecord;
	MissileCountFn missileCount;
	UnitSeedFn unitSeed;
	SetUnitSkillFlagFn setUnitSkillFlag;
	ApplyMeleeHitFn applyMeleeHit;
	UnitStatFn unitStat;
	UnitStatNoParamFn unitStatNoParam;
	SummonPetFn summonPet;
	SetPetLevelFn setPetLevel;
	PetSkillStatsFn petSkillStats;
	AddPetToPartyFn addPetToParty;
};
extern Api api;
void Bind(uint64_t exeBase) noexcept;
auto Bound() noexcept -> bool;

// --- Monster count: RE spike (build 93847) ------------------------------------------------------------------------
// Static analysis of the D2RLoader_6ab3782c dump (Capstone + .pdata, no Ghidra project); see docs/reverse-engineering/monster-count.md.
// "Hookable" = whole instructions >= 14 bytes, no RIP-relative operand, no branch, no FF 25 (D2RLoader hook) at the
// entry in the dumped image. Nothing here has been hooked in game yet.
//
// Per-level monster ledger (MonsterRegion.cpp). Region array = *(game + GameMonRegions), indexed by level id.
// remaining-known = region[+RegionSpawned] - region[+RegionKilled]; D2R's own Den of Evil check (0x5dde20) uses it.
inline constexpr int GameMonRegions = 0x1d0;    // Game: MonsterRegion*[] (region per level id; null when none)
inline constexpr int RegionRoomTotal = 0x0c;    // int32: level's DRLG room count, cached by the populator (negative until set)
inline constexpr int RegionDensity = 0x2b8;     // int32: spawn budget used by the populator (clamped to 10000)
inline constexpr int RegionUniques = 0x2c8;     // int32: unique/champion pack counter (incremented by 0x499ba0)
inline constexpr int RegionSpawned = 0x2cc;     // int32: counted hostile monsters spawned in the level
inline constexpr int RegionKilled = 0x2d0;      // int32: counted hostile monsters killed in the level
inline constexpr int RegionNoAlignCount = 0x2d8; // uint8: non-zero = alignment changes do not adjust RegionSpawned
inline constexpr uint64_t RvaRegionOf = 0x508740;      // (regions, levelId) -> region; raw index, no bounds check
inline constexpr uint64_t RvaRegionSpawned = 0x509520; // (regions, room, unit, skip) -> void: ++Spawned unless skip,
                                                       // else marks the unit "not counted" (0x543870(unit, 2, 1))
inline constexpr uint64_t RvaRegionKilled = 0x509590;  // (regions, unit): ++Killed if counted and alignment == 0
inline constexpr uint64_t RvaRegionUnkilled = 0x5095e0;// (regions, unit): --Killed (monster resurrect skills)
inline constexpr uint64_t RvaRegionAlign = 0x508eb0;   // (regions, unit, oldAlign, newAlign): hostile->ally --Spawned,
                                                       // ally->hostile ++Spawned. Called only by RvaSetAlignment
inline constexpr uint64_t RvaRegionRemove = 0x509630;  // (regions, oldLvl, newLvl, a4, a5, a6, unit): --Spawned when a
                                                       // counted unit leaves (SUnitInactive.cpp, 0x505d70)
inline constexpr uint8_t RegionSpawnedEntry[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x49, 0x8B, 0xC0, 0x48, 0x8B,
	0xD9, 0x45, 0x85, 0xC9}; // PUSH RBX / SUB RSP,0x20 / MOV RAX,R8 / MOV RBX,RCX / TEST R9D,R9D (15)
inline constexpr uint8_t RegionKilledEntry[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48,
	0x8B, 0xFA, 0x48, 0x8B, 0xD9}; // MOV [RSP+8],RBX / PUSH RDI / SUB RSP,0x20 / MOV RDI,RDX / MOV RBX,RCX (16)
                                   // RvaRegionUnkilled has the same 16 entry bytes.
inline constexpr uint8_t RegionAlignEntry[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89,
	0x7C, 0x24, 0x18}; // MOV [RSP+8],RBX / MOV [RSP+0x10],RSI / MOV [RSP+0x18],RDI (15)
inline constexpr uint8_t RegionRemoveEntry[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x7C, 0x24, 0x60, 0x48,
	0x63, 0xC2}; // PUSH RDI / SUB RSP,0x20 / MOV RDI,[RSP+0x60] / MOVSXD RAX,EDX (14)

// Spawning. Every monster that can be counted is built by RvaSpawnCore; the 12 wrappers 0x4929e0..0x493870
// (RvaSpawnMonster among them) only pack their arguments for it. Hirelings are allocated elsewhere (PlrSave2.cpp
// 0x5331f0, 0x531790, 0x53bd30) and never reach the ledger.
inline constexpr uint64_t RvaSpawnMonster = 0x492cf0;  // (game, room, x, y, monster, mode, search, flags) -> unit;
                                                       // wrapper: packs a block and calls RvaSpawnCore
inline constexpr uint64_t RvaSpawnCore = 0x493cc0;     // (&SpawnBlock) -> unit or null; block +0 game, +8 room,
                                                       // +0x18 monstats row; calls RvaRegionSpawned for every monster
inline constexpr uint64_t RvaAllocUnit = 0x490410;     // (type, classId, ...) -> unit; generic unit allocator
inline constexpr uint64_t RvaPopulateRoom = 0x50acf0;  // (game, room) -> void: random monsters for one room. Call-only
                                                       // (RIP-relative load at +0xc)
inline constexpr uint64_t RvaSetAlignment = 0x48e600;  // (unit, align 0 hostile/1 neutral/2 ally, updateLedger): stat
                                                       // 172 via state 105. Call-only (JE rel32 at +3)
inline constexpr uint8_t SpawnMonsterEntry[] = {0x4C, 0x8B, 0xDC, 0x48, 0x83, 0xEC, 0x68, 0x8B, 0x84, 0x24, 0x90, 0x00,
	0x00, 0x00}; // MOV R11,RSP / SUB RSP,0x68 / MOV EAX,[RSP+0x90] (14)
inline constexpr uint8_t SpawnCoreEntry[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89,
	0x7C, 0x24, 0x20}; // MOV [RSP+0x10],RBX / MOV [RSP+0x18],RSI / MOV [RSP+0x20],RDI (15)

// Rooms. Monsters are created when a room first activates (0x408670 per frame, 0x4042a0 for one room such as a
// portal's destination), not when the level is generated.
inline constexpr uint64_t RvaRoomLevelId = 0x2efc10;   // (room) -> level id (room+0x18 -> +0x90 -> +0x1f8). Call-only
inline constexpr uint64_t RvaRoomInTown = 0x2f0750;    // (room) -> non-zero in town
inline constexpr uint64_t RvaLevelRoomCount = 0x3269d0;// (?, level) -> DRLG rooms (list +0x10, next +0x48, skips
                                                       // rooms with +0x50 & 0x800000); used for RegionRoomTotal
inline constexpr int RoomFlags = 0x54;                 // uint32: bit 0 populated once (0x2f04e0), bit 1 active (0x2f0440)
inline constexpr int RoomDrlgRoom = 0x18;              // Room -> DRLG room

// Units.
inline constexpr int UnitMode = 0x0c;                  // uint32 mode; monsters: 0 death, 12 dead
inline constexpr int UnitGame = 0xd8;                  // Game* (written by the allocator)
inline constexpr uint64_t RvaUnitAlignment = 0x2f4190; // (unit) -> stat 172 for players/monsters (0 hostile), else 2.
                                                       // D2RLoader hooks its entry (FF 25): call only, never hook
inline constexpr uint64_t RvaUnitLevelId = 0x543810;   // (monster) -> level id
inline constexpr uint64_t RvaUnitHasFlag = 0x5439c0;   // (monster, flag) -> non-zero; flag 2 = "not counted"

// monstats.txt records: data tables +0xf58 (vector), count +0xf60, getter 0x976e0 (dataFlag, row). Flag bytes at
// +0x3c.. in classic MONSTATSFLAG bit order (inferred: confirm column names with `modkit show monstats`):
inline constexpr int MonStatsNeverCountByte = 0x3e;    // & 0x04: not counted by RvaRegionSpawned (neverCount)
inline constexpr uint8_t MonStatsNeverCountMask = 0x04;
inline constexpr int MonStatsEnabledByte = 0x3f;       // & 0x02: row enabled (allocator refuses others)
inline constexpr uint8_t MonStatsEnabledMask = 0x02;

// Death. Prefer D2RLoader's MonsterDeath listener (LifecycleService) over a hook here.
inline constexpr uint64_t RvaUnitDeath = 0x44db20;     // see the registry; entry unchanged in this dump
inline constexpr uint64_t RvaMonsterDeathMode = 0x444f50; // monster mode-table handler (table 0x1d1a200, "MMODE_DEATH"):
                                                          // calls RvaRegionKilled for every counted death

// --- Helpers ------------------------------------------------------------------------------------------------------
// Read a field of an opaque game structure: At<int32_t>(unit, UnitType).
template <typename T>
inline auto At(const void* p, int offset) noexcept -> T {
	return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(p) + offset);
}

inline auto DataFlag(Game* game) noexcept -> uint32_t {
	return At<uint8_t>(game, GameDataFlag);
}

// A unit's position in subtiles, read the way 0x432b70 reads it.
inline auto UnitXY(Unit* unit, int32_t* x, int32_t* y) noexcept -> bool {
	const int32_t type = At<int32_t>(unit, UnitType);
	const uint8_t* path = At<const uint8_t*>(unit, UnitPath);
	if (path == nullptr) {
		return false;
	}
	if (type == 2 || type == 4 || type == 5) {
		*x = At<int32_t>(path, 0x10);
		*y = At<int32_t>(path, 0x14);
	} else {
		*x = At<uint16_t>(path, 0x2);
		*y = At<uint16_t>(path, 0x6);
	}
	return true;
}

// Evaluate one of the skill record's calc columns (field = SkCalc1, SkPrgCalc1, ...) for this unit and level.
inline auto EvalCalc(Game* game, Unit* unit, const uint8_t* skill, int field, int32_t skillId, int32_t level) noexcept
	-> int32_t {
	return api.evalSkillCalc(DataFlag(game), unit, At<int32_t>(skill, field), skillId, level);
}

inline auto ValidMissile(Game* game, int32_t missile) noexcept -> bool {
	return missile >= 0 && missile < api.missileCount(DataFlag(game));
}

// D2's seed step (the game inlines it): lo' = lo * 0x6ac690c5 + hi, hi' = the carry. Use with api.unitSeed(unit).
inline auto Roll(uint32_t* seed) noexcept -> uint32_t {
	const uint64_t s = static_cast<uint64_t>(seed[0]) * 0x6ac690c5ULL + seed[1];
	seed[0] = static_cast<uint32_t>(s);
	seed[1] = static_cast<uint32_t>(s >> 32);
	return seed[0];
}

// Random number in [0, n) from a D2 seed.
inline auto RandomBelow(uint32_t* seed, int32_t n) noexcept -> int32_t {
	if (n <= 0) {
		return 0;
	}
	const uint32_t v = Roll(seed);
	return static_cast<int32_t>((n & (n - 1)) == 0 ? v & static_cast<uint32_t>(n - 1) : v % static_cast<uint32_t>(n));
}

}  // namespace d2r
