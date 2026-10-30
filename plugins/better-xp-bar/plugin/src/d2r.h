// Game image interface for Better XP Bar: ONLY the addresses this plugin uses, each with its evidence.
//
// EVERYTHING HERE IS FOR ONE IMAGE: D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (TimeDateStamp 0x6ab3782c), checked by
// build_guard.h before any of it is used, and every function's first bytes are checked again live (VerifyImage).
// Evidence: docs/re/findings.md (dump analysis) and the d2rAI-toolkit registry. Nothing is guessed.
//
// NOT RUN IN GAME YET. Every function in xp_reader.cpp that touches these is marked "needs live check".
#pragma once

#include <cstddef>
#include <cstdint>

namespace d2r {

struct Unit;

// --- Functions (RVA, signature, evidence) -------------------------------------------------------------------------
// () -> u32 local client slot: `mov eax,[rip+..] (0x2a23704); ret`. findings (a). 1,233 call sites.
inline constexpr uint64_t RvaLocalSlot = 0x8b2d0;
// (u32 slot) -> Unit* or null: player unit of a client slot. findings (a). 1,035 call sites pair it with RvaLocalSlot.
inline constexpr uint64_t RvaPlayerUnit = 0x9a480;
// (Unit*, const char* file, i32 line) -> i32 class id. registry "Units"; used by ExpBarWidget at 0x14cc926.
inline constexpr uint64_t RvaUnitClass = 0x349860;
// (Unit*, i32 stat, i32 layer) -> i32 base stat. registry "Stats"; level (12) read at 0x14cc938. Redirected by
// D2RLoader/D2RCore (FF 25 at the entry): call only, never hook.
inline constexpr uint64_t RvaBaseStat = 0x2f48c0;
// (Unit*, i32 stat, i32 layer) -> i32 total stat. registry "Stats"; experience (13) read at 0x14cc94a. Redirected too.
inline constexpr uint64_t RvaTotalStat = 0x2f5020;
// (Unit*) -> u8 dataFlag (table bank). registry "Units"; used at 0x14cc967.
inline constexpr uint64_t RvaUnitDataFlag = 0x34a0e0;

inline constexpr int32_t StatLevel = 12;       // immediate at 0x14cc934 (lea edx,[r8+0xc])
inline constexpr int32_t StatExperience = 13;  // immediate at 0x14cc946 (lea edx,[r8+0xd])

// --- Experience table (read directly; see findings (b)) -----------------------------------------------------------
// Data managers: *(base + 0x2a9a580 + 16 * dataFlag), dataFlag < 4 (0x300920: lea rbx,[rip..]; add rax,rax;
// mov rbx,[rbx+rax*8]; asserted cmp rax,4). Experience rows vector at manager + 0x14e0 = {u32* rows, u64 count}.
// Row = 9 x u32 (lea rdx,[rax+rax*8]); class column 0..7 (cmp edx,7 / cmova 0); row 0 = max level; row L = XP at
// which level L starts (Threshold(L-1) = rows[L], 0x300920 reads row L+1 for its argument L).
inline constexpr uint64_t RvaDataManagers = 0x2a9a580;
inline constexpr uint32_t DataManagerStride = 16;
inline constexpr uint32_t DataBanks = 4;
inline constexpr uint32_t ExpVectorOffset = 0x14e0;
inline constexpr uint32_t ExpRowWords = 9;
inline constexpr uint32_t ExpClasses = 8;

// --- Live byte checks: the first bytes of every function called, and the two instructions of 0x300920 that show
// the manager table and the 0x14e0 offset. All from the 93847 dump (findings tables (a), (b)).
struct ExpectedBytes {
	uint64_t rva;
	const uint8_t* bytes;
	uint32_t size;
	const char* what;
};

inline constexpr uint8_t BytesLocalSlot[] = {0x8B, 0x05, 0x2E, 0x84, 0x99, 0x02, 0xC3};
inline constexpr uint8_t BytesPlayerUnit[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x83, 0xF9, 0x08, 0x0F, 0x83, 0x85};
inline constexpr uint8_t BytesUnitClass[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x85, 0xC9, 0x75, 0x1D, 0x88, 0x4C, 0x24, 0x30, 0x48, 0x8D, 0x4C};
inline constexpr uint8_t BytesBaseStat[] = {0xFF, 0x25, 0x4A, 0x59, 0xB3, 0x03, 0x90, 0x90, 0x90, 0x90};
inline constexpr uint8_t BytesTotalStat[] = {0xFF, 0x25, 0xF2, 0x51, 0xB3, 0x03, 0x90, 0x90, 0x90, 0x90};
inline constexpr uint8_t BytesUnitDataFlag[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x85, 0xC9, 0x75, 0x1A, 0x88, 0x4C, 0x24, 0x30, 0x48, 0x8D, 0x4C};
inline constexpr uint8_t BytesManagersLea[] = {0x48, 0x8D, 0x1D, 0x11, 0x9C, 0x79, 0x02};      // 0x300968 lea rbx,[rip..] -> 0x2a9a580
inline constexpr uint8_t BytesExpOffset[] = {0x48, 0x81, 0xC3, 0xE0, 0x14, 0x00, 0x00};        // 0x30097c add rbx,0x14e0
inline constexpr uint8_t BytesExpRowStride[] = {0x48, 0x8D, 0x14, 0xC0};                         // 0x3009b8 lea rdx,[rax+rax*8]

inline constexpr ExpectedBytes Checks[] = {
	{RvaLocalSlot, BytesLocalSlot, sizeof(BytesLocalSlot), "local slot 0x8b2d0"},
	{RvaPlayerUnit, BytesPlayerUnit, sizeof(BytesPlayerUnit), "player unit 0x9a480"},
	{RvaUnitClass, BytesUnitClass, sizeof(BytesUnitClass), "class id 0x349860"},
	{RvaBaseStat, BytesBaseStat, sizeof(BytesBaseStat), "base stat 0x2f48c0"},
	{RvaTotalStat, BytesTotalStat, sizeof(BytesTotalStat), "total stat 0x2f5020"},
	{RvaUnitDataFlag, BytesUnitDataFlag, sizeof(BytesUnitDataFlag), "data flag 0x34a0e0"},
	{0x300968, BytesManagersLea, sizeof(BytesManagersLea), "exp table managers 0x300968"},
	{0x30097c, BytesExpOffset, sizeof(BytesExpOffset), "exp table offset 0x30097c"},
	{0x3009b8, BytesExpRowStride, sizeof(BytesExpRowStride), "exp row stride 0x3009b8"},
};

using LocalSlotFn = uint32_t (*)() noexcept;
using PlayerUnitFn = Unit* (*)(uint32_t slot) noexcept;
using UnitClassFn = int32_t (*)(Unit* unit, const char* file, int32_t line) noexcept;
using StatFn = int32_t (*)(Unit* unit, int32_t stat, int32_t layer) noexcept;
using UnitDataFlagFn = uint8_t (*)(Unit* unit) noexcept;

}  // namespace d2r
