// Aura engine interface: the D2Game functions Better Auras calls or hooks (D2R 3.3.0 build 93847, image 0x6ab3782c).
//
// Found with the toolkit's Ghidra scripts on the dumped image (docs/reverse-engineering/better-auras.md). Every
// signature below was read from the decompiled callers in the game's own aura code (Skills.cpp / SkilPal.cpp) unless
// marked [A] (assumed: verify in game). plugin.cpp binds these only after the image check.
#pragma once

#include "d2r.h"

#include <cstdint>

namespace aura {

using d2r::Game;
using d2r::Unit;

// --- Hooked functions (RVA, entry bytes) --------------------------------------------------------------------------
// Event 8 handler, the aura upkeep tick: (game, unit, key, a4, a5). key -1 = the right-skill aura, key > 0 = a
// per-skill aura timer (the skill id). Re-runs the skill's srvdofunc and reschedules itself every perdelay.
inline constexpr uint64_t RvaAuraTick = 0x437460;
// MOV [RSP+8],RBX / MOV [RSP+0x10],RBP / PUSH RSI / PUSH RDI / PUSH R12 / PUSH R14 / PUSH R15: 18 bytes
inline constexpr uint8_t AuraTickEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24,
                                            0x10, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57};

// Start the right-skill aura when an aura is selected: (game, unit, skill).
inline constexpr uint64_t RvaStartRightAura = 0x43b680;
// PUSH RBP / PUSH RSI / PUSH RDI / PUSH R14 / SUB RSP,0x48 / MOV RDI,RDX / MOV RBP,RCX: 16 bytes
inline constexpr uint8_t StartRightAuraEntry[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83,
                                                  0xec, 0x48, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xe9};

// Server skill select: (unit, isLeft, skill, a4). The function starts with CMP R8D,5 / JE rel32 (a relative branch
// the hook would have to relocate), so the hook sits on the next instructions, at +0x0a, where the four argument
// registers are still untouched and RSP is the entry RSP. Skill 5 never reaches the hook (vanilla ignores it too).
inline constexpr uint64_t RvaSelectSkillBody = 0x438a7a;
// MOV [RSP+0x20],R9D / PUSH RBP / PUSH R14 / PUSH R15 / SUB RSP,0x40: 14 bytes
inline constexpr uint8_t SelectSkillBodyEntry[] = {0x44, 0x89, 0x4c, 0x24, 0x20, 0x55, 0x41,
                                                   0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x40};

// Client player action (D2Client Player.cpp): (player, action, x, y, ...); the mouse input handlers call it. Action 3
// = right button press, once per press, also with an aura on the button (which never reaches the server's start-skill
// code). A click on a panel or the skill menu never gets here. Stack arguments past the fourth are passed through.
inline constexpr uint64_t RvaPlayerAction = 0xfe3b0;
// MOV [RSP+0x10],RBX / PUSH RBP / PUSH RSI / PUSH RDI / PUSH R12 / PUSH R13 / PUSH R14 / PUSH R15: 16 bytes
inline constexpr uint8_t PlayerActionEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x10, 0x55, 0x56, 0x57,
                                                0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
inline constexpr int ActionRightPress = 3;

// Unit death: (game, victim, killer or null, flag). Already in the registry (24 bytes).
inline constexpr uint64_t RvaUnitDeath = 0x44db20;
inline constexpr uint8_t UnitDeathEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89,
                                             0x74, 0x24, 0x18, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x50};

// Player regen tick (event 3, every frame; D2Game\src\Player\PlrModes.cpp): (game, unit, a3, a4). Life += stat 74
// hpregen (<< 8 per frame) up to max life, never below 1 point; then the mana step 0x42e350: mana += max mana /
// (charstats ManaRegen * 25) * (100 + stat 27) / 100 + stat 26 manarecovery, up to max mana, never below 0.
inline constexpr uint64_t RvaRegenTick = 0x42e600;
// MOV [RSP+0x20],RBX / PUSH RBP / SUB RSP,0x40 / MOV EAX,R9D / MOV dword [RSP+0x30],0: 21 bytes
inline constexpr uint8_t RegenTickEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x20, 0x55, 0x48, 0x83, 0xec, 0x40, 0x41,
                                             0x8b, 0xc1, 0xc7, 0x44, 0x24, 0x30, 0x00, 0x00, 0x00, 0x00};

using AuraTickFn = void (*)(Game* game, Unit* unit, int32_t key, int32_t a4, int32_t a5) noexcept;
using RegenTickFn = void (*)(Game* game, Unit* unit, int32_t a3, int32_t a4) noexcept;
using StartRightAuraFn = void (*)(Game* game, Unit* unit, int32_t skill) noexcept;
using SelectSkillFn = void (*)(Unit* unit, int32_t isLeft, int32_t skill, int32_t a4) noexcept;
using PlayerActionFn = uint64_t (*)(Unit* player, int32_t action, int32_t x, int32_t y, uint64_t a5, uint64_t a6) noexcept;
using UnitDeathFn = void (*)(Game* game, Unit* victim, Unit* killer, int32_t flag) noexcept;

// --- Called functions ---------------------------------------------------------------------------------------------
struct Api {
	// (game, unit, skill, level, a5, rightSkill): (re)schedule event 8 at the next multiple of perdelay. rightSkill 1
	// keys the event -1, 0 keys it with the skill id (an independent per-skill timer).
	int64_t (*scheduleAuraTick)(Game*, Unit*, int32_t skill, int32_t level, int32_t a5, int32_t rightSkill) noexcept;
	// (game, unit, type, key): remove the unit's timer events of that type; key 0 = all, else event key == key.
	void (*removeEvents)(Game*, Unit*, int32_t type, int32_t key) noexcept;
	// (game) -> the a5 value 0x43b680 passes to the scheduler (a game-frame-like value; must not be 0 or -1).
	int32_t (*tickArg)(Game*) noexcept;
	// (unit) -> the unit's game (server).
	Game* (*unitGame)(Unit*) noexcept;
	// (unit) -> right skill entry (unit +0x100 skill list); (entry, file, line) -> skill id.
	void* (*rightSkill)(Unit*) noexcept;
	int32_t (*skillEntryId)(void* entry, const char* file, int32_t line) noexcept;
	// (unit, skill, 0) -> the unit's skill entry or null; (unit, entry, 1) -> level with bonuses.
	void* (*unitSkill)(Unit*, int32_t skill, int32_t zero) noexcept;
	int32_t (*skillLevel)(Unit*, void* entry, int32_t withBonus) noexcept;
	// (unit, state) -> the stat list bound to that state, or null.
	void* (*stateList)(Unit*, int32_t state) noexcept;
	// (unit, list) detach, then (dataFlag, list) free: how vanilla removes an aura's list.
	void (*detachList)(Unit*, void* list) noexcept;
	void (*freeList)(uint32_t dataFlag, void* list) noexcept;
	// (unit, state, on) set/clear the state (game level); (unit, state) -> nonzero has state.
	void (*setState)(Unit*, int32_t state, int32_t on) noexcept;
	int32_t (*hasState)(Unit*, int32_t state) noexcept;
	// (dataFlag, list, stat, value, layer) set a stat inside a list; (dataFlag, list, stat, layer) -> value.
	void (*setListStat)(uint32_t dataFlag, void* list, int32_t stat, int32_t value, int32_t layer) noexcept;
	int32_t (*listStat)(uint32_t dataFlag, void* list, int32_t stat, int32_t layer) noexcept;
	// Unit stats: (unit, stat, layer) total; set; add.
	int32_t (*stat)(Unit*, int32_t stat, int32_t layer) noexcept;
	void (*setStat)(Unit*, int32_t stat, int32_t value, int32_t layer) noexcept;
	void (*addStat)(Unit*, int32_t stat, int32_t value, int32_t layer) noexcept;
	// (unit) -> room or null; (room) -> nonzero when the room is in town (the town test 0x437460 uses).
	void* (*unitRoom)(Unit*) noexcept;
	int32_t (*roomIsTown)(void* room) noexcept;
	// (unit) -> nonzero when the unit is dead/dying [A: read as the "not dead" gate before 0x437460 casts].
	int32_t (*unitDead)(Unit*) noexcept;
};
extern Api api;
extern StartRightAuraFn startRightAuraOriginal;  // set by the hook; StartAura calls it

void Bind(uint64_t exeBase) noexcept;

// --- Record / stat constants (build 93847) ------------------------------------------------------------------------
inline constexpr int UnitId = 0x08;         // int32: the same number on client and server
inline constexpr int SkFlags24 = 0x24;     // bit 0x20 aura (the bit 0x438a70 / 0x43b680 test), bit 0x40 periodic
inline constexpr int SkFlags25 = 0x25;     // bit 0x01 InTown
inline constexpr int SkSrvDoFunc = 0x4e;   // u16 srvdofunc
inline constexpr int SkAuraState = 0xa0;   // i16 aurastate
inline constexpr uint8_t FlagAura = 0x20;
inline constexpr uint8_t FlagInTown = 0x01;

inline constexpr int GameFrame = 0x170;    // int32 current game frame (25 per second)
inline constexpr int FramesPerSecond = 25;

inline constexpr int StatLife = 6, StatMaxLife = 7, StatMana = 8, StatMaxMana = 9;  // values << 8
// Per-frame regen added by 0x42e600 / 0x42e350, << 8 per frame: negative values drain
inline constexpr int StatManaRecovery = 26, StatHpRegen = 74;
inline constexpr int StatAuraSkill = 350;  // in the aurastate list: the skill id (0x15e)
inline constexpr int EventAura = 8;

}  // namespace aura
