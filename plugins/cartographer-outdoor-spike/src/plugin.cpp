// cartographer-outdoor-spike: THROWAWAY spike for Cartographer's own outdoor level builder, phases S0 and S1
// (synpoox/d2r-tools#22, plan docs/cartographer/plans/cartographer-outdoor-builder.md). Lives on a spike branch and is
// never merged. Ported from the uncompiled WIP on pd2-resurrected spike/cartographer-outdoor-s1 (b78cfe2).
//
// Every address is found by byte signature (unique match in the executable sections, or the part is off); hooks go in
// through the SDK's InstallInlineHook, which byte-checks the entry again. No version gate. The two D2RLoader thunks
// (act lookup, outdoor entry) are found by following the call inside the function that uses them.
//
// S0, console `cartographer probe-outdoor`: byte checks (at load, before our hooks, and live through the Diagnostics
// service), the module behind the outdoor thunk's pointer, call counts of 0x3fac10 / 0x3ee320 / 0x3ee780 per level.
// S1, the slot level (levels.txt row, Act 5, DrlgType 3): 0x3271c0 hook -> level rectangle -> 0x3ee320 (our 0x3fe2e0
// hook stamps the jungle line with 0x3ecfc0) -> 0x3ee780 makes the rooms -> native tail of 0x3271c0 with the drlg type
// set to 0 for the call, so D2RCore's GenerateOutdoorLevel is never reached for the slot.
// Config: <game>\d2rloader\config\cartographer-outdoor-spike.toml.
#include <D2RLPlugin/api.h>
#include <D2RLPlugin/diagnostics.h>

#include "crash_guard.h"
#include "log.h"

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace {

constexpr const char* PluginName = "cartographer-outdoor-spike";
constexpr const char* PluginVersion = "0.0.3";

// --- Signatures (build 93847; docs/reverse-engineering/cartographer.md). `??` = any byte. ---------------------------
// Each is the function's entry up to its first relative operand. `hook` is the instruction-boundary length the hook
// copies (and the loader byte-checks).
struct Part {
	const char* name;
	const char* sig;
	uint32_t hook;
	uint64_t expectRva;  // the 93847 RVA, for the log only
};
enum PartId { LevelBuild, OutdoorEntry, Act5, Stamp, Rooms, GridAlloc, GridRead, Act1, PartCount };
constexpr Part Parts[PartCount] = {
	{"level build 3271c0",
	 "48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2 44 0F B6 F9 8B 90 60 08 00 00 49 8D 8E E4 01 00 00 41 03 96 F8",
	 14, 0x3271c0},
	{"outdoor entry 3ee320",
	 "48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48 83 EC 50 4C 8B 7A 18 33 C0 4C 8B F2 44 0F B6 E1 49 89 87 A8 00 00 00 49 8D 4F 08 8B 42 2C 99 83",
	 19, 0x3ee320},
	{"act 5 builder 3fe2e0", "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 48 81 EC C0 00 00 00 83 BA F8 01 00 00 6E 48 8B F2 44 0F B6 F1", 16, 0x3fe2e0},
	{"piece stamp 3ecfc0",
	 "48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 4C 8B 62 18 48 8B FA 48 63 B4 24 90 00 00 00 45 8B E9 8B D6 4C 89 64 24",
	 16, 0x3ecfc0},
	{"grid to rooms 3ee780", "40 55 41 54 41 55 41 56 48 81 EC 98 00 00 00", 15, 0x3ee780},
	{"grid alloc 3fa7f0", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 49 63 E8 44 8B FA 4C 8B F1 89 51 08 89 69 0C 8D 34", 16, 0x3fa7f0},
	{"grid read 3fa790", "48 8B 41 20 4D 63 C0 48 63 D2 4E 63 0C 80 48 8B 41 18 4C 03 CA 4A 8B 04 C8", 0, 0x3fa790},
	{"act 1 builder 3fac10", "48 89 5C 24 08 57 48 83 EC 20 0F B6 F9 48 8B DA 8B 8A F8 01 00 00 8D 41 FE 83 F8 01 76 0D 83 F9 11 74 08 48 8B CA", 10, 0x3fac10},
};

// --- Level and room fields (decompiles of 0x3271c0 / 0x3ee320) ---------------------------------------------------------
constexpr int LevelDrlgType = 0x00, LevelRoomCount = 0x08, LevelFirstRoom = 0x10, LevelGrid = 0x18;
constexpr int LevelX = 0x24, LevelY = 0x28, LevelW = 0x2c, LevelH = 0x30;
constexpr int LevelDrlg = 0x1c8, LevelTileset = 0x1e0, LevelSeed = 0x1e4, LevelId = 0x1f8;
constexpr int DrlgFlags = 0x110, DrlgSeedBase = 0x860;  // flags bit 0: the client's DRLG
constexpr int GridCellsW = 0xb0, GridCellsH = 0xb4, GridNeighbours = 0x338, GridPieces = 0x08, GridFlags = 0x58;
constexpr int RoomPiece = 0x40, RoomNext = 0x48, RoomX = 0x60, RoomY = 0x64, RoomW = 0x68, RoomH = 0x6c;

// --- Config ------------------------------------------------------------------------------------------------------------
struct Config {
	int level = -1;                         // levels.txt id of the slot row
	char line[64] = "E,EW,EW,EW,W";         // the fixed jungle line, left to right
	int offsetX = 6000;                     // level rectangle origin (tiles); the row's OffsetX/OffsetY
	int offsetY = 3000;
	bool stamp = true;                      // false: leave the Act 5 branch to D2R (control run)
	bool build = true;                      // false: probe only
};
Config g_config;
char g_status[400] = "off: not started";
uint8_t g_mask[32];
int g_blocks = 0;

// --- Resolved engine --------------------------------------------------------------------------------------------------
using LevelFn = void (*)(uint8_t, void*) noexcept;
using StampFn = void (*)(uint8_t, void*, int32_t, int32_t, int32_t, int32_t, char) noexcept;
using GridReadFn = uint64_t (*)(void*, int32_t, int32_t) noexcept;
using ReseedFn = void (*)(void*, int32_t) noexcept;
using ActLookupFn = int8_t (*)(uint8_t, int32_t) noexcept;

const D2RL::PluginContext* g_context = nullptr;
const uint8_t* g_addr[PartCount];       // found address, or null
int g_found[PartCount];                 // number of signature matches
int g_preState[PartCount];              // 0 match (unique), 1 not found / not unique, 2 unreadable
LevelFn g_levelBuild = nullptr, g_outdoorEntry = nullptr, g_act5 = nullptr, g_act1 = nullptr, g_rooms = nullptr;
StampFn g_stamp = nullptr;
GridReadFn g_gridRead = nullptr;
ReseedFn g_reseed = nullptr;
ActLookupFn g_actLookup = nullptr;      // D2RLoader thunk into D2RCore, followed from 0x3ee320
const uint8_t* g_actThunk = nullptr;
const uint8_t* g_outdoorThunk = nullptr;  // D2RLoader thunk, followed from 0x3271c0
uint64_t g_outdoorPointer = 0;          // the address the outdoor thunk jumps through

std::atomic<uint32_t> g_act1Calls{0}, g_entryCalls{0}, g_roomsCalls{0}, g_slotBuilds{0};
struct LevelCount {
	std::atomic<int32_t> id{-1};
	std::atomic<uint32_t> act1{0}, entry{0}, rooms{0};
};
LevelCount g_levels[64];
thread_local void* t_slotLevel = nullptr;  // the slot level this thread is building right now

template <typename T>
auto F(const void* p, int off) noexcept -> T& {
	return *reinterpret_cast<T*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(p)) + off);
}

auto CountFor(int32_t id) noexcept -> LevelCount* {
	for (LevelCount& c : g_levels) {
		int32_t cur = c.id.load();
		if (cur == id) return &c;
		if (cur < 0) {
			int32_t expect = -1;
			if (c.id.compare_exchange_strong(expect, id) || expect == id) return &c;
		}
	}
	return nullptr;
}

auto ReadBytes(const void* from, void* to, size_t n) noexcept -> bool {
	__try {
		std::memcpy(to, from, n);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto IsSlot(const void* level) noexcept -> bool {
	if (level == nullptr || g_config.level < 0) return false;
	int32_t id = -1;
	return ReadBytes(static_cast<const uint8_t*>(level) + LevelId, &id, sizeof(id)) && id == g_config.level;
}

auto Side(const void* level) noexcept -> const char* {
	const void* drlg = F<void*>(level, LevelDrlg);
	return drlg != nullptr && (F<uint32_t>(drlg, DrlgFlags) & 1) != 0 ? "client" : "server";
}

// --- Signature scan -----------------------------------------------------------------------------------------------------
auto ParseSig(const char* sig, uint8_t* bytes, uint8_t* mask, int cap) noexcept -> int {
	int n = 0;
	for (const char* c = sig; *c != '\0' && n < cap;) {
		while (*c == ' ') c++;
		if (*c == '\0') break;
		if (c[0] == '?') {
			bytes[n] = 0;
			mask[n++] = 0;
			while (*c == '?') c++;
		} else {
			unsigned v = 0;
			if (std::sscanf(c, "%2x", &v) != 1) return -1;
			bytes[n] = static_cast<uint8_t>(v);
			mask[n++] = 1;
			c += 2;
		}
	}
	return n;
}

// Counts matches in every executable section of the main image; keeps the first.
auto ScanImage(uintptr_t base, const char* sig, const uint8_t** first) noexcept -> int {
	uint8_t bytes[96], mask[96];
	const int len = ParseSig(sig, bytes, mask, 96);
	if (len <= 0) return -1;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	int found = 0;
	*first = nullptr;
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || section->Misc.VirtualSize < static_cast<DWORD>(len)) continue;
		const auto* start = reinterpret_cast<const uint8_t*>(base + section->VirtualAddress);
		const size_t end = section->Misc.VirtualSize - static_cast<size_t>(len);
		for (size_t at = 0; at <= end; ++at) {
			if (start[at] != bytes[0]) continue;
			bool ok = true;
			for (int k = 1; k < len; ++k) {
				if (mask[k] != 0 && start[at + k] != bytes[k]) {
					ok = false;
					break;
				}
			}
			if (ok) {
				if (found == 0) *first = start + at;
				++found;
			}
		}
	}
	return found;
}

// Follow the first `E8 rel32` inside [fn, fn + span) whose target begins with a `FF 25` thunk.
auto FindThunkCall(const uint8_t* fn, int span) noexcept -> const uint8_t* {
	uint8_t code[256];
	if (span > 256 || !ReadBytes(fn, code, static_cast<size_t>(span))) return nullptr;
	for (int i = 0; i + 5 <= span; ++i) {
		if (code[i] != 0xE8) continue;
		int32_t rel;
		std::memcpy(&rel, code + i + 1, 4);
		const uint8_t* target = fn + i + 5 + rel;
		uint8_t head[2] = {};
		if (ReadBytes(target, head, 2) && head[0] == 0xFF && head[1] == 0x25) return target;
	}
	return nullptr;
}

// --- Logging the built level ---------------------------------------------------------------------------------------------
struct PieceInfo {
	int32_t id = -1, file = -1, x = 0, y = 0, w = 0, h = 0;
};
auto ReadPiece(const void* room, PieceInfo* out) noexcept -> bool {
	const uint8_t* piece = nullptr;
	if (!ReadBytes(static_cast<const uint8_t*>(room) + RoomPiece, &piece, sizeof(piece)) || piece == nullptr) return false;
	int32_t v[10];
	if (!ReadBytes(piece, v, sizeof(v))) return false;
	out->id = v[0];
	out->file = v[1];
	out->x = v[6];
	out->y = v[7];
	out->w = v[8];
	out->h = v[9];
	return true;
}

void LogRooms(const void* level, const char* when) noexcept {
	const int lx = F<int32_t>(level, LevelX), ly = F<int32_t>(level, LevelY);
	plog::Info("[%s] %s: level %d tileset %d type %d: %d rooms, level rect (%d,%d %dx%d)", Side(level), when, F<int32_t>(level, LevelId),
	           F<int32_t>(level, LevelTileset), F<int32_t>(level, LevelDrlgType), F<int32_t>(level, LevelRoomCount), lx, ly,
	           F<int32_t>(level, LevelW), F<int32_t>(level, LevelH));
	int n = 0;
	for (void* r = F<void*>(level, LevelFirstRoom); r != nullptr && n < 4000; r = F<void*>(r, RoomNext), n++) {
		PieceInfo p;
		const bool havePiece = ReadPiece(r, &p);
		plog::Info("  room %s%d file %d at (%d,%d) %dx%d", havePiece ? "piece " : "fill ", havePiece ? p.id : -1, havePiece ? p.file : -1,
		           F<int32_t>(r, RoomX) - lx, F<int32_t>(r, RoomY) - ly, F<int32_t>(r, RoomW), F<int32_t>(r, RoomH));
	}
}

void LogGrid(const void* level) noexcept {
	const uint8_t* grid = F<uint8_t*>(level, LevelGrid);
	if (grid == nullptr || g_gridRead == nullptr) return;
	const int w = F<int32_t>(grid, GridCellsW), h = F<int32_t>(grid, GridCellsH);
	plog::Info("[%s] grid block %p: %dx%d cells, neighbours %p", Side(level), static_cast<const void*>(grid), w, h, F<void*>(grid, GridNeighbours));
	if (w <= 0 || h <= 0 || w > 200 || h > 64) return;
	for (int y = 0; y < h; y++) {
		char row[256];
		int used = 0;
		for (int x = 0; x < w && used < 250; x++) {
			const uint64_t flags = g_gridRead(const_cast<uint8_t*>(grid) + GridFlags, x, y);
			const uint64_t piece = g_gridRead(const_cast<uint8_t*>(grid) + GridPieces, x, y);
			row[used++] = piece != 0 ? '#' : ((flags & 0x200) != 0 ? '+' : ((flags & 0x100) != 0 ? ' ' : '.'));
		}
		row[used] = '\0';
		plog::Info("  grid %2d |%s|", y, row);
	}
}

// --- The S1 build ---------------------------------------------------------------------------------------------------------
void ParseLine(const char* text) noexcept {
	g_blocks = 0;
	std::memset(g_mask, 0, sizeof(g_mask));
	uint8_t mask = 0;
	bool any = false;
	for (const char* c = text;; c++) {
		if (*c == ',' || *c == '\0') {
			if (any && g_blocks < 32) g_mask[g_blocks++] = mask;
			mask = 0;
			any = false;
			if (*c == '\0') break;
			continue;
		}
		any = true;
		switch (*c) {  // the native jungle's bits: W 1, E 2, S 4, N 8
			case 'W': mask |= 1; break;
			case 'E': mask |= 2; break;
			case 'S': mask |= 4; break;
			case 'N': mask |= 8; break;
			default: break;
		}
	}
}

// 0x3fe2e0 for the slot: stamp the jungle line, piece = 0x211 + mask (LvlPrest 530-544), one 32x32-tile block = 4x4 cells.
void StampLine(uint8_t dataFlag, void* level) noexcept {
	for (int i = 0; i < g_blocks; i++) {
		if (g_mask[i] == 0) {
			plog::Warn("[%s] block %d has mask 0 (LvlPrest 529, Act 3 Town): skipped", Side(level), i);
			continue;
		}
		g_stamp(dataFlag, level, i * 4, 0, 0x211 + g_mask[i], -1, 0);
	}
	plog::Info("[%s] stamped %d jungle blocks on level %d", Side(level), g_blocks, F<int32_t>(level, LevelId));
}

void Act5Hook(uint8_t dataFlag, void* level) noexcept {
	if (IsSlot(level) && t_slotLevel == level && g_config.stamp) {
		StampLine(dataFlag, level);
		return;
	}
	g_act5(dataFlag, level);
}

void OutdoorEntryHook(uint8_t dataFlag, void* level) noexcept {
	g_entryCalls.fetch_add(1);
	if (level != nullptr) {
		if (LevelCount* c = CountFor(F<int32_t>(level, LevelId))) c->entry.fetch_add(1);
	}
	g_outdoorEntry(dataFlag, level);
}

void Act1Hook(uint8_t dataFlag, void* level) noexcept {
	g_act1Calls.fetch_add(1);
	if (level != nullptr) {
		if (LevelCount* c = CountFor(F<int32_t>(level, LevelId))) c->act1.fetch_add(1);
		plog::Info("[%s] 0x3fac10 (act 1 outdoor builder) called for level %d", Side(level), F<int32_t>(level, LevelId));
	}
	g_act1(dataFlag, level);
}

void RoomsHook(uint8_t dataFlag, void* level) noexcept {
	g_roomsCalls.fetch_add(1);
	if (level != nullptr) {
		if (LevelCount* c = CountFor(F<int32_t>(level, LevelId))) c->rooms.fetch_add(1);
	}
	g_rooms(dataFlag, level);
}

void BuildSlot(uint8_t dataFlag, void* level) noexcept {
	const int id = F<int32_t>(level, LevelId);
	uint8_t* drlg = F<uint8_t*>(level, LevelDrlg);
	g_slotBuilds.fetch_add(1);
	const int act = g_actLookup != nullptr ? g_actLookup(dataFlag, id) : -99;
	plog::Info("[%s] BUILD slot level %d at %p (dataFlag %u, drlg %p, level type %d, drlg type %d, rect on entry (%d,%d %dx%d)): D2RCore act lookup says %d",
	           Side(level), id, level, dataFlag, static_cast<void*>(drlg), F<int32_t>(level, LevelTileset), F<int32_t>(level, LevelDrlgType),
	           F<int32_t>(level, LevelX), F<int32_t>(level, LevelY), F<int32_t>(level, LevelW), F<int32_t>(level, LevelH), act);

	// The level rectangle: act start-up places outdoor levels, never a slot, so +0x24..+0x30 are 0 here unless the engine
	// already placed it from the row's fixed offsets (logged above). Origin from config, size from the line.
	F<int32_t>(level, LevelX) = g_config.offsetX;
	F<int32_t>(level, LevelY) = g_config.offsetY;
	F<int32_t>(level, LevelW) = g_blocks * 32;
	F<int32_t>(level, LevelH) = 32;

	// The same seed the original draws: the level RNG decides each piece's file.
	g_reseed(static_cast<uint8_t*>(level) + LevelSeed, static_cast<int32_t>(F<uint32_t>(drlg, DrlgSeedBase)) + id);

	t_slotLevel = level;
	g_outdoorEntry(dataFlag, level);  // our Act 5 branch stamps the line, then D2R makes the rooms
	t_slotLevel = nullptr;

	// Native tail of 0x3271c0 (saved room bits, warp-spot scan): drlg type 0 matches no branch of its dispatch, so the
	// tail runs and D2RCore's GenerateOutdoorLevel does not.
	F<int32_t>(level, LevelDrlgType) = 0;
	g_levelBuild(dataFlag, level);
	F<int32_t>(level, LevelDrlgType) = 3;

	LogGrid(level);
	LogRooms(level, "after build");
}

void LevelBuildHook(uint8_t dataFlag, void* level) noexcept {
	if (g_blocks > 0 && t_slotLevel == nullptr && IsSlot(level) && F<int32_t>(level, LevelDrlgType) == 3) {
		BuildSlot(dataFlag, level);
		return;
	}
	g_levelBuild(dataFlag, level);
}

// --- Config -----------------------------------------------------------------------------------------------------------------
void ReadConfig(const D2RL::PluginContext* ctx) noexcept {
	static char toml[4096];
	if (!ctx->ReadConfig(toml, sizeof(toml) - 1)) {
		plog::Warn("config not readable; probe only");
		return;
	}
	for (char* line = toml; line != nullptr && *line != '\0';) {
		char* next = std::strchr(line, '\n');
		if (next != nullptr) *next++ = '\0';
		int v = 0;
		char s[64];
		if (std::sscanf(line, " level = %d", &v) == 1) g_config.level = v;
		else if (std::sscanf(line, " line = \"%63[^\"]\"", s) == 1) std::snprintf(g_config.line, sizeof(g_config.line), "%s", s);
		else if (std::sscanf(line, " offset_x = %d", &v) == 1) g_config.offsetX = v;
		else if (std::sscanf(line, " offset_y = %d", &v) == 1) g_config.offsetY = v;
		else if (std::sscanf(line, " stamp = %s", s) == 1) g_config.stamp = std::strncmp(s, "true", 4) == 0;
		else if (std::sscanf(line, " build = %s", s) == 1) g_config.build = std::strncmp(s, "true", 4) == 0;
		line = next;
	}
}

// --- S0 probe ----------------------------------------------------------------------------------------------------------------
void Say(const D2RL::PluginContext* ctx, const char* fmt, ...) noexcept {
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	plog::Info("%s", line);
	if (ctx != nullptr) ctx->WriteConsoleMessage(line);
}

const char* StateName(D2RL::Diagnostics::ModificationState s) noexcept {
	using D2RL::Diagnostics::ModificationState;
	return s == ModificationState::Unchanged ? "unchanged" : (s == ModificationState::Tracked ? "tracked (a hook or patch)" : "UNTRACKED change");
}

// Which module owns an address; D2RCore is packed on disk and its code may sit in a heap block.
void DescribeAddress(const D2RL::PluginContext* ctx, uint64_t addr) noexcept {
	MEMORY_BASIC_INFORMATION mbi {};
	if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
		Say(ctx, "  VirtualQuery failed for %p", reinterpret_cast<void*>(addr));
		return;
	}
	const char* type = mbi.Type == MEM_IMAGE ? "image" : (mbi.Type == MEM_PRIVATE ? "private (heap/VirtualAlloc)" : "mapped");
	Say(ctx, "  target %p: region base %p, allocation base %p, size 0x%llx, %s, protect 0x%lx", reinterpret_cast<void*>(addr), mbi.BaseAddress,
	    mbi.AllocationBase, static_cast<unsigned long long>(mbi.RegionSize), type, mbi.Protect);
	HMODULE mod = nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(addr), &mod) &&
	    mod != nullptr) {
		wchar_t name[MAX_PATH] = L"";
		GetModuleFileNameW(mod, name, MAX_PATH);
		Say(ctx, "  module containing it: %ls (base %p)", name, static_cast<void*>(mod));
	} else {
		Say(ctx, "  no loaded module contains it");
	}
	// The module whose image range holds it, among all loaded modules (name and range).
	HMODULE mods[512];
	DWORD needed = 0;
	if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed) == 0) return;
	const DWORD count = needed / sizeof(HMODULE);
	for (DWORD i = 0; i < count && i < 512; i++) {
		const auto* base = reinterpret_cast<const uint8_t*>(mods[i]);
		IMAGE_DOS_HEADER dos {};
		IMAGE_NT_HEADERS64 nt {};
		if (!ReadBytes(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || !ReadBytes(base + dos.e_lfanew, &nt, sizeof(nt))) continue;
		const uint64_t size = nt.OptionalHeader.SizeOfImage;
		wchar_t name[MAX_PATH] = L"";
		GetModuleFileNameW(mods[i], name, MAX_PATH);
		const bool inside = addr >= reinterpret_cast<uint64_t>(base) && addr < reinterpret_cast<uint64_t>(base) + size;
		if (inside || std::wcsstr(name, L"D2RCore") != nullptr)
			Say(ctx, "  module %ls base %p size 0x%llx: target %s it", name, static_cast<const void*>(base), static_cast<unsigned long long>(size),
			    inside ? "IS INSIDE" : "is not inside");
	}
}


// --- S1 follow-up (walk, collision, automap): position readout, memory peek, collision dump -----------------------------------
// Console only; none of this changes the game. Offsets and RVAs are build 93847 (multispec / monster-count docs).
constexpr uint64_t RvaControlUnit = 0x484c90;  // (client, 0) -> the client's player unit
constexpr uint64_t RvaCollide = 0x350a20;      // (unit, x, y, mask) -> nonzero blocked
constexpr int UnitPathPtr = 0x38;              // dynamic path: x +0x2, y +0x6 (uint16 subtiles)
using ControlUnitFn = void* (*)(void*, int32_t) noexcept;
using CollideFn = int64_t (*)(void*, int32_t, int32_t, uint32_t) noexcept;
ControlUnitFn g_controlUnit = nullptr;
CollideFn g_collide = nullptr;
std::atomic<void*> g_trackUnit{nullptr};
std::atomic<bool> g_trackOn{false};
std::atomic<int> g_trackMs{250};
std::atomic<int> g_minX{1 << 30}, g_maxX{-1}, g_minY{1 << 30}, g_maxY{-1};

auto ReadPos(void* unit, int* sx, int* sy) noexcept -> bool {
	uint8_t* path = nullptr;
	uint16_t x = 0, y = 0;
	if (unit == nullptr || !ReadBytes(static_cast<uint8_t*>(unit) + UnitPathPtr, &path, sizeof(path)) || path == nullptr) return false;
	if (!ReadBytes(path + 2, &x, 2) || !ReadBytes(path + 6, &y, 2)) return false;
	*sx = x;
	*sy = y;
	return true;
}

DWORD WINAPI TrackThread(void*) {
	int lastX = -1, lastY = -1;
	while (g_trackOn.load()) {
		int x, y;
		void* unit = g_trackUnit.load();
		if (unit != nullptr && ReadPos(unit, &x, &y)) {
			if (x != lastX || y != lastY) {
				plog::Info("pos subtile (%d,%d) = tile (%d,%d); level-relative tile (%d,%d)", x, y, x / 5, y / 5, x / 5 - g_config.offsetX, y / 5 - g_config.offsetY);
				lastX = x;
				lastY = y;
			}
			if (x > 0 && y > 0) {
				if (x < g_minX) g_minX = x;
				if (x > g_maxX) g_maxX = x;
				if (y < g_minY) g_minY = y;
				if (y > g_maxY) g_maxY = y;
			}
		}
		Sleep(static_cast<DWORD>(g_trackMs.load()));
	}
	return 0;
}

void HexDump(const D2RL::PluginContext* ctx, uint64_t addr, int len) noexcept {
	for (int off = 0; off < len; off += 16) {
		uint8_t b[16] = {};
		const int n = len - off < 16 ? len - off : 16;
		char line[160];
		int used = std::snprintf(line, sizeof(line), "%016llx:", static_cast<unsigned long long>(addr + static_cast<uint64_t>(off)));
		if (!ReadBytes(reinterpret_cast<void*>(addr + static_cast<uint64_t>(off)), b, static_cast<size_t>(n))) {
			std::snprintf(line + used, sizeof(line) - static_cast<size_t>(used), " <unreadable>");
		} else {
			for (int i = 0; i < n; i++) used += std::snprintf(line + used, sizeof(line) - static_cast<size_t>(used), " %02x", b[i]);
		}
		Say(ctx, "%s", line);
	}
}

// Collision dump of the slot level: one char per subtile (800 x 160 for the 5-block line), '.' open, '#' blocked by `mask`.
// Written to d2rloader\logs\cartographer-collision-<mask>.txt next to the game executable, rows top to bottom (y up = south).
void CollisionDump(const D2RL::PluginContext* ctx, void* unit, uint32_t mask) noexcept {
	if (g_collide == nullptr) {
		Say(ctx, "collide function not bound");
		return;
	}
	const int x0 = g_config.offsetX * 5, y0 = g_config.offsetY * 5;
	const int w = g_blocks * 32 * 5, h = 32 * 5;
	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe);
	dir = dir.substr(0, dir.find_last_of(L"\\/") + 1) + L"d2rloader\\logs\\";
	wchar_t name[64];
	std::swprintf(name, 64, L"cartographer-collision-%x.txt", mask);
	FILE* f = _wfopen((dir + name).c_str(), L"w");
	if (f == nullptr) {
		Say(ctx, "cannot open the collision file");
		return;
	}
	std::fprintf(f, "# origin subtile (%d,%d) size %dx%d mask 0x%x; '.' open '#' blocked; one row per y, x runs left to right\n", x0, y0, w, h, mask);
	int blocked = 0;
	for (int y = 0; y < h; y++) {
		std::string row(static_cast<size_t>(w), '.');
		for (int x = 0; x < w; x++) {
			if (g_collide(unit, x0 + x, y0 + y, mask) != 0) {
				row[static_cast<size_t>(x)] = '#';
				blocked++;
			}
		}
		std::fprintf(f, "%s\n", row.c_str());
	}
	std::fclose(f);
	Say(ctx, "collision dump mask 0x%x: %d of %d subtiles blocked", mask, blocked, w * h);
}

auto ProbeCommand2(D2R::Game::Client* client, const D2RL::PluginContext* ctx, const char* args) noexcept -> bool {
	if (std::strncmp(args, "pos", 3) == 0 || std::strncmp(args, "track", 5) == 0 || std::strncmp(args, "coll", 4) == 0) {
		void* unit = g_controlUnit != nullptr ? g_controlUnit(client, 0) : nullptr;
		int x = 0, y = 0;
		if (unit == nullptr || !ReadPos(unit, &x, &y)) {
			Say(ctx, "no player unit / position (client %p, unit %p)", static_cast<void*>(client), unit);
			return true;
		}
		if (std::strncmp(args, "pos", 3) == 0) {
			Say(ctx, "unit %p subtile (%d,%d) tile (%d,%d) level-relative tile (%d,%d)", unit, x, y, x / 5, y / 5, x / 5 - g_config.offsetX, y / 5 - g_config.offsetY);
		} else if (std::strncmp(args, "track", 5) == 0) {
			const char* a = args + 5;
			while (*a == ' ') a++;
			if (std::strncmp(a, "off", 3) == 0) {
				g_trackOn = false;
				Say(ctx, "track off; reached x %d..%d y %d..%d (subtiles)", g_minX.load(), g_maxX.load(), g_minY.load(), g_maxY.load());
			} else if (std::strncmp(a, "reset", 5) == 0) {
				g_minX = 1 << 30; g_maxX = -1; g_minY = 1 << 30; g_maxY = -1;
				Say(ctx, "track extents reset");
			} else {
				g_trackUnit = unit;
				if (!g_trackOn.exchange(true)) CreateThread(nullptr, 0, TrackThread, nullptr, 0, nullptr);
				Say(ctx, "track on (unit %p)", unit);
			}
		} else {
			unsigned mask = 0x1c09;
			std::sscanf(args + 4, " %x", &mask);
			Say(ctx, "player at subtile (%d,%d): blocked(self, mask 0x%x) = %lld", x, y, mask,
			    static_cast<long long>(g_collide != nullptr ? g_collide(unit, x, y, mask) : -1));
			if (std::strstr(args, "dump") != nullptr) CollisionDump(ctx, unit, mask);
		}
		return true;
	}
	if (std::strncmp(args, "peek", 4) == 0) {
		unsigned long long addr = 0;
		int len = 64;
		std::sscanf(args + 4, " %llx %d", &addr, &len);
		if (len > 1024) len = 1024;
		HexDump(ctx, addr, len);
		return true;
	}
	if (std::strncmp(args, "rooms", 5) == 0) {
		const char* a = args + 5;
		while (*a == ' ') a++;
		unsigned long long lv = 0;
		std::sscanf(a, "%llx", &lv);
		if (lv == 0) {
			Say(ctx, "usage: cartographer rooms <level pointer> (see the BUILD log line)");
			return true;
		}
		const void* level = reinterpret_cast<void*>(lv);
		int n = 0;
		for (void* r = F<void*>(level, LevelFirstRoom); r != nullptr && n < 3; r = F<void*>(r, RoomNext), n++) {
			Say(ctx, "room %p:", r);
			HexDump(ctx, reinterpret_cast<uint64_t>(r), 0x100);
		}
		return true;
	}
	return false;
}

auto ProbeCommand(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) return D2RL::ConsoleCommandResult::Failed;
	const D2RL::PluginContext* ctx = command->plugin;
	const char* args = command->args != nullptr ? command->args : "";
	while (*args == ' ') args++;
	if (ProbeCommand2(client, ctx, args)) return D2RL::ConsoleCommandResult::Handled;
	if (std::strncmp(args, "reset", 5) == 0) {
		for (LevelCount& c : g_levels) {
			c.id = -1;
			c.act1 = 0;
			c.entry = 0;
			c.rooms = 0;
		}
		g_act1Calls = 0;
		g_entryCalls = 0;
		g_roomsCalls = 0;
		Say(ctx, "%s: counters reset", PluginName);
		return D2RL::ConsoleCommandResult::Handled;
	}
	const uint64_t base = ctx->exeBase;
	Say(ctx, "%s %s: %s", PluginName, PluginVersion, g_status);
	const D2RL::DiagnosticsService* diag = nullptr;
	const bool haveDiag = ctx->QueryService(&diag) == D2RL::ServiceQueryResult::Success &&
	                      D2RL::HasDiagnosticsServiceField(diag, D2RL::DiagnosticsServiceRequiredSize);
	bool all = true;
	for (int i = 0; i < PartCount; i++) {
		if (g_preState[i] != 0) all = false;
	}
	Say(ctx, "S0 byte checks at load (signature unique, before our hooks): %s", all ? "ALL PASS" : "SOME FAILED");
	for (int i = 0; i < PartCount; i++) {
		const uint64_t rva = g_addr[i] != nullptr ? reinterpret_cast<uint64_t>(g_addr[i]) - base : 0;
		char owner[96] = "";
		const char* live = "n/a";
		if (haveDiag && g_addr[i] != nullptr && Parts[i].hook > 0) {
			uint8_t expected[96], mask[96];
			ParseSig(Parts[i].sig, expected, mask, 96);
			D2RL::Diagnostics::HookQuery q {.structSize = D2RL::Diagnostics::HookQuerySize, .rva = rva, .expected = expected, .expectedSize = Parts[i].hook};
			D2RL::Diagnostics::HookStatus st {.structSize = D2RL::Diagnostics::HookStatusSize};
			if (diag->queryHookStatus(ctx, &q, &st) == D2RL::Diagnostics::Result::Success) {
				live = StateName(st.state);
				if (st.ownerCount > 0) std::snprintf(owner, sizeof(owner), ", owner %s (%u)", st.ownerPluginId, st.ownerCount);
			} else {
				live = "query failed";
			}
		}
		Say(ctx, "  %-22s found at rva 0x%llx (expected 0x%llx), matches %d, load %s | live %s%s", Parts[i].name, static_cast<unsigned long long>(rva),
		    static_cast<unsigned long long>(Parts[i].expectRva), g_found[i], g_preState[i] == 0 ? "PASS" : "FAIL", live, owner);
	}
	if (g_outdoorThunk != nullptr) {
		Say(ctx, "outdoor thunk at rva 0x%llx, jumps through pointer at rva 0x%llx = %p", static_cast<unsigned long long>(reinterpret_cast<uint64_t>(g_outdoorThunk) - base),
		    static_cast<unsigned long long>(g_outdoorPointer - base), reinterpret_cast<void*>(*reinterpret_cast<const uint64_t*>(g_outdoorPointer)));
		DescribeAddress(ctx, *reinterpret_cast<const uint64_t*>(g_outdoorPointer));
	} else {
		Say(ctx, "outdoor thunk not found");
	}
	if (g_actThunk != nullptr) {
		uint64_t ptr = 0;
		int32_t rel = 0;
		std::memcpy(&rel, g_actThunk + 2, 4);
		ReadBytes(g_actThunk + 6 + rel, &ptr, 8);
		Say(ctx, "act lookup thunk at rva 0x%llx, pointer %p", static_cast<unsigned long long>(reinterpret_cast<uint64_t>(g_actThunk) - base), reinterpret_cast<void*>(ptr));
		DescribeAddress(ctx, ptr);
	}
	Say(ctx, "call counts since load or reset: 0x3fac10 = %u, 0x3ee320 = %u, 0x3ee780 = %u, slot builds = %u", g_act1Calls.load(), g_entryCalls.load(),
	    g_roomsCalls.load(), g_slotBuilds.load());
	for (LevelCount& c : g_levels) {
		const int32_t id = c.id.load();
		if (id >= 0) Say(ctx, "  level %d: 0x3fac10 x%u, 0x3ee320 x%u, 0x3ee780 x%u", id, c.act1.load(), c.entry.load(), c.rooms.load());
	}
	return D2RL::ConsoleCommandResult::Handled;
}

constexpr D2RL::PluginInfo Info {
	.infoSize = D2RL::PluginInfoSize,
	.abiVersion = D2RL_PLUGIN_ABI_VERSION,
	.id = "cartographer-outdoor-spike",
	.name = "Cartographer Outdoor Spike",
	.version = PluginVersion,
	.author = "Synpoo",
	.description = "Throwaway spike: probes and builds a fixed jungle line for Cartographer's outdoor level builder.",
	.flags = D2RL::PluginFlags::Shared | D2RL::PluginFlags::NativeHooks,
};

}  // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) return false;
	g_context = context;
	plog::Init(context, PluginName);
	crash_guard::Install(PluginVersion, context->exeBase);
	ReadConfig(context);
	if (g_config.level >= 0) ParseLine(g_config.line);

	// S0 byte checks, before any hook of ours: each signature must match exactly once.
	bool all = true;
	for (int i = 0; i < PartCount; i++) {
		g_addr[i] = nullptr;
		g_found[i] = ScanImage(context->exeBase, Parts[i].sig, &g_addr[i]);
		g_preState[i] = g_found[i] == 1 ? 0 : 1;
		if (g_found[i] != 1) g_addr[i] = nullptr;
		const uint64_t rva = g_addr[i] != nullptr ? reinterpret_cast<uint64_t>(g_addr[i]) - context->exeBase : 0;
		plog::Info("byte check (load, before our hooks) %-22s %s: %d match(es), rva 0x%llx (expected 0x%llx)", Parts[i].name,
		           g_preState[i] == 0 ? "PASS" : "FAIL", g_found[i], static_cast<unsigned long long>(rva),
		           static_cast<unsigned long long>(Parts[i].expectRva));
		if (g_preState[i] != 0) all = false;
	}
	g_controlUnit = reinterpret_cast<ControlUnitFn>(context->exeBase + RvaControlUnit);
	g_collide = reinterpret_cast<CollideFn>(context->exeBase + RvaCollide);
	{
		static const uint8_t expectControl[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9};
		uint8_t got[sizeof(expectControl)] = {};
		uint8_t h[16] = {};
		const bool ok = ReadBytes(reinterpret_cast<void*>(g_controlUnit), got, sizeof(got)) && std::memcmp(got, expectControl, sizeof(got)) == 0;
		ReadBytes(reinterpret_cast<void*>(g_collide), h, sizeof(h));
		plog::Info("control unit 0x%llx byte check %s; collide 0x350a20 head %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		           static_cast<unsigned long long>(RvaControlUnit), ok ? "PASS" : "FAIL", h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9], h[10],
		           h[11], h[12], h[13], h[14], h[15]);
		if (!ok) g_controlUnit = nullptr;
	}
	if (all) {
		g_stamp = reinterpret_cast<StampFn>(const_cast<uint8_t*>(g_addr[Stamp]));
		g_gridRead = reinterpret_cast<GridReadFn>(const_cast<uint8_t*>(g_addr[GridRead]));
		// Reseed 0x367110 is the first call of 0x3271c0 (E8 at +0x30); the thunks are found by following calls.
		const uint8_t* lb = g_addr[LevelBuild];
		int32_t rel = 0;
		if (lb[0x30] == 0xE8) {
			std::memcpy(&rel, lb + 0x31, 4);
			g_reseed = reinterpret_cast<ReseedFn>(const_cast<uint8_t*>(lb + 0x35 + rel));
		}
		g_outdoorThunk = FindThunkCall(lb, 0x80);
		g_actThunk = FindThunkCall(g_addr[OutdoorEntry], 0xb0);
		if (g_actThunk != nullptr) g_actLookup = reinterpret_cast<ActLookupFn>(const_cast<uint8_t*>(g_actThunk));
		if (g_outdoorThunk != nullptr) {
			std::memcpy(&rel, g_outdoorThunk + 2, 4);
			g_outdoorPointer = reinterpret_cast<uint64_t>(g_outdoorThunk + 6 + rel);
		}
		plog::Info("thunks: outdoor %p (pointer slot %p), act lookup %p; reseed %p", static_cast<const void*>(g_outdoorThunk),
		           reinterpret_cast<void*>(g_outdoorPointer), static_cast<const void*>(g_actThunk), reinterpret_cast<void*>(g_reseed));

		auto hook = [&](PartId id, void* target, void* original) {
			const uint64_t rva = reinterpret_cast<uint64_t>(g_addr[id]) - context->exeBase;
			return context->InstallInlineHook(rva, g_addr[id], Parts[id].hook, target, reinterpret_cast<void**>(original));
		};
		const bool counting = hook(Act1, reinterpret_cast<void*>(Act1Hook), &g_act1) && hook(OutdoorEntry, reinterpret_cast<void*>(OutdoorEntryHook), &g_outdoorEntry) &&
		                      hook(Rooms, reinterpret_cast<void*>(RoomsHook), &g_rooms);
		bool slot = false;
		if (counting && g_config.build && g_config.level >= 0 && g_blocks > 0 && g_reseed != nullptr && g_actLookup != nullptr) {
			slot = hook(LevelBuild, reinterpret_cast<void*>(LevelBuildHook), &g_levelBuild) && hook(Act5, reinterpret_cast<void*>(Act5Hook), &g_act5);
		}
		std::snprintf(g_status, sizeof(g_status), "byte checks all pass; counting hooks %s; slot level %d line %s (%d blocks): %s", counting ? "on" : "FAILED",
		              g_config.level, g_config.line, g_blocks, slot ? "build hooks on" : "build hooks off");
	} else {
		std::snprintf(g_status, sizeof(g_status), "off: a byte check failed (see log); nothing hooked");
	}
	plog::Info("%s", g_status);
	return context->RegisterConsoleCommand("cartographer", ProbeCommand,
	                                       "Cartographer outdoor spike: `cartographer probe-outdoor` (S0 report), `cartographer reset` (counters).");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	g_trackOn = false;
	crash_guard::Remove();
}
