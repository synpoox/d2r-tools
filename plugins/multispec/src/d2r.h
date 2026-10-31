// D2R game image interface used by MultiSpec: RVAs, offsets and function types.
//
// EVERYTHING IN THIS FILE IS FOR ONE GAME IMAGE: D2R 3.3 build 93847 under D2RLoader 1.3.1 (PE TimeDateStamp
// 0x6ab3782c). Each value and its evidence is in
// docs/reverse-engineering/multispec.md (and the toolkit's d2r-93847-addresses.md). MultiSpec installs no hooks: it
// only calls these functions, after CheckEntries() has compared their entry bytes with the image.
//
// Calling convention: Windows x64. Game, Unit, Client and Skill are opaque; read fields with At<T>.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <cstdint>

namespace d2r {

struct Game;
struct Unit;
struct Client;
struct Skill;
struct Room;

// --- Functions (RVAs, build 93847) --------------------------------------------------------------------------------
inline constexpr uint64_t RvaUnitType = 0x34b9d0;         // (unit) -> 0 player, 1 monster, ...
inline constexpr uint64_t RvaUnitClass = 0x349860;        // (unit, file, line) -> class id (players: 0-7)
inline constexpr uint64_t RvaPlayerData = 0x34b240;       // (player) -> player data (+0 name)
inline constexpr uint64_t RvaUnitClient = 0x48fde0;       // (player) -> its server client
inline constexpr uint64_t RvaUnitGame = 0x48ff00;         // (unit) -> game (unit +0xd8)
inline constexpr uint64_t RvaControlUnit = 0x484c90;      // (client, 0) -> the client's player unit (ClientGetControlUnit)
inline constexpr uint64_t RvaUnitRoom = 0x34b440;         // (unit) -> room
inline constexpr uint64_t RvaRoomInTown = 0x2f0750;       // (room) -> nonzero in town
inline constexpr uint64_t RvaStatBase = 0x2f48c0;         // (unit, stat, layer) -> base value (D2RLoader-hooked)
inline constexpr uint64_t RvaGetSkill = 0x33dcd0;         // (unit, skillId, itemId -1) -> skill or null (SkillsGetSkill)
inline constexpr uint64_t RvaGetSkillLevel = 0x33d1e0;    // (unit, skill, bonus, 0) -> level (SkillsGetLevel; D2RLoader-hooked)
inline constexpr uint64_t RvaClassSkillCount = 0x33cb30;  // (dataFlag, class) -> number of class skills
inline constexpr uint64_t RvaClassSkillId = 0x33dde0;     // (dataFlag, class, index) -> skill id, in the save's order
inline constexpr uint64_t RvaSkillRecord = 0x97790;       // (dataFlag, skillId) -> skills.txt record or null
inline constexpr uint64_t RvaCanSkillClass = 0x436bd0;    // (player, skillId) -> nonzero: the skill is the player's class
inline constexpr uint64_t RvaCanSkillLevel = 0x339330;    // (player, skillId) -> nonzero: level and prerequisites met
inline constexpr uint64_t RvaCanSkillStats = 0x339c00;    // (player, skillId) -> nonzero: stat requirements met
inline constexpr uint64_t RvaSpendSkillPoint = 0x438670;  // (dataFlag, player, skillId) -> 1: one point spent, client told
inline constexpr uint64_t RvaRespecSkills = 0x4360f0;     // (game, player): D2R's skills-only respec, client told
inline constexpr uint64_t RvaSkillUsable = 0x33a660;      // (unit, skill) -> nonzero: level with bonuses above 0
inline constexpr uint64_t RvaSelectSkill = 0x438a70;      // (unit, isLeft, skillId, itemId): sets a mouse skill, client told
inline constexpr uint64_t RvaRememberLeft = 0x34d180;     // (player): keeps the left skill for after item changes
inline constexpr uint64_t RvaRememberRight = 0x34d220;    // (player): same for the right skill

// --- Offsets (build 93847) ----------------------------------------------------------------------------------------
inline constexpr int GameDataFlag = 0x106;   // u8
inline constexpr int UnitMode = 0x0c;        // u32; players: 0 death, 17 dead
inline constexpr int UnitSkillList = 0x100;  // SkillList*
inline constexpr int SkillListLeft = 0x08;   // Skill* on the left button
inline constexpr int SkillListRight = 0x10;  // Skill* on the right button
inline constexpr int SkillRecordPtr = 0x00;  // skills.txt record; record +0 i16 = skill id
inline constexpr int SkillOwnerItem = 0x4c;  // i32 item id, -1 = the unit's own skill
inline constexpr int PlayerDataName = 0x00;  // char[16]
inline constexpr int ClientName = 0x2c;      // char[16]
inline constexpr int ClientUnitType = 0x26c; // fields ClientGetControlUnit reads (checked readable first)
inline constexpr int ClientFlags = 0x4e0;    // bit 0: no control unit
inline constexpr int ClientSize = 0x4e8;     // bytes read to validate a client pointer

inline constexpr int SkCharClass = 0x2c;     // i8 skills.txt charclass
inline constexpr int SkMaxLevel = 0x184;     // i16 maxlvl (<= 0: 20)
inline constexpr int SkPointsCalc = 0x210;   // i32 skpoints formula, -1 = one point per level

inline constexpr int StatNewSkills = 5;      // unspent skill points
inline constexpr int PlayerModeDeath = 0;
inline constexpr int PlayerModeDead = 17;
inline constexpr int DefaultMaxLevel = 20;

// --- Function types -----------------------------------------------------------------------------------------------
using UnitTypeFn = int32_t (*)(Unit* unit) noexcept;
using UnitClassFn = int32_t (*)(Unit* unit, const char* file, int32_t line) noexcept;
using PlayerDataFn = uint8_t* (*)(Unit* player) noexcept;
using UnitClientFn = Client* (*)(Unit* player) noexcept;
using UnitGameFn = Game* (*)(Unit* unit) noexcept;
using ControlUnitFn = Unit* (*)(Client* client, int32_t zero) noexcept;
using UnitRoomFn = Room* (*)(Unit* unit) noexcept;
using RoomInTownFn = int32_t (*)(Room* room) noexcept;
using StatBaseFn = int32_t (*)(Unit* unit, int32_t stat, int32_t layer) noexcept;
using GetSkillFn = Skill* (*)(Unit* unit, int32_t skillId, int32_t itemId) noexcept;
using GetSkillLevelFn = int32_t (*)(Unit* unit, Skill* skill, int32_t bonus, int32_t extra) noexcept;
using ClassSkillCountFn = int32_t (*)(uint32_t dataFlag, int32_t charClass) noexcept;
using ClassSkillIdFn = int32_t (*)(uint32_t dataFlag, int32_t charClass, int32_t index) noexcept;
using SkillRecordFn = const uint8_t* (*)(uint32_t dataFlag, int32_t skillId) noexcept;
using SkillCheckFn = int32_t (*)(Unit* player, int32_t skillId) noexcept;
using SpendSkillPointFn = int32_t (*)(uint32_t dataFlag, Unit* player, int32_t skillId) noexcept;
using RespecSkillsFn = void (*)(Game* game, Unit* player) noexcept;
using SkillUsableFn = int32_t (*)(Unit* unit, Skill* skill) noexcept;
using SelectSkillFn = void (*)(Unit* unit, int32_t isLeft, int32_t skillId, int32_t itemId) noexcept;
using RememberSkillFn = void (*)(Unit* player) noexcept;

struct Api {
	UnitTypeFn unitType;
	UnitClassFn unitClass;
	PlayerDataFn playerData;
	UnitClientFn unitClient;
	UnitGameFn unitGame;
	ControlUnitFn controlUnit;
	UnitRoomFn unitRoom;
	RoomInTownFn roomInTown;
	StatBaseFn statBase;
	GetSkillFn getSkill;
	GetSkillLevelFn getSkillLevel;
	ClassSkillCountFn classSkillCount;
	ClassSkillIdFn classSkillId;
	SkillRecordFn skillRecord;
	SkillCheckFn canSkillClass;
	SkillCheckFn canSkillLevel;
	SkillCheckFn canSkillStats;
	SpendSkillPointFn spendSkillPoint;
	RespecSkillsFn respecSkills;
	SkillUsableFn skillUsable;
	SelectSkillFn selectSkill;
	RememberSkillFn rememberLeftSkill;
	RememberSkillFn rememberRightSkill;
};
extern Api api;

void Bind(uintptr_t exeBase) noexcept;

// Compares every function's entry bytes with the image (for D2RLoader-hooked functions, the bytes after its jump).
// Returns the number that differ; `report` names them ("ok: 23 functions" or "differs: 0x4360f0 ...").
auto CheckEntries(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> int;

// --- Helpers ------------------------------------------------------------------------------------------------------
template <typename T>
inline auto At(const void* p, int offset) noexcept -> T {
	return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(p) + offset);
}

}  // namespace d2r
