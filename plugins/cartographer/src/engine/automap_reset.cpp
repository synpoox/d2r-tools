#include "automap_reset.h"

#include "core/part_signatures.h"
#include "automap_memory.h"
#include "automap_skip.h"
#include "log.h"

#include <atomic>
#include <cstring>
#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

namespace carto {
namespace {

// The restore (0xd5710) takes two arguments (rcx, rdx; its only call site, 0xd18c8, sets both) and hands them on to 0xd5450 (the map file path). The hook passes all four
// argument registers and the result straight on: a hook that took none and called plog first handed the original garbage (a fault in 0xd5450).
using RestoreFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t) noexcept;
// The record writer (0xd5fe0; record, then the two path arguments) and the cell list insert (0xd1460; list, result, cell).
using SaveFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t) noexcept;
using InsertFn = void* (*)(void* list, void* result, const void* cell) noexcept;

// The record 0xd6230 hands to 0xd5fe0: key, act, seed hi, the four list sizes in bytes, then the four lists as vectors of 16-bit words (data pointer first).
constexpr size_t kRecordSizes = 12, kRecordLists = 0x20, kRecordListStride = 0x18;
// The layer object's four cell lists (trees, header first) and the cell 0xd5710 inserts: kind 1 (read back, so never saved again), then the three saved words.
constexpr size_t kObjectLists[4] = {0x08, 0x30, 0x58, 0x80};
struct InsertCell {
	uint16_t kind;
	uint16_t a;
	int32_t b;
	int32_t c;
};
static_assert(sizeof(InsertCell) == 12);

Part g_part = {"AutomapRestore", sig::AutomapRestore, 17, false, nullptr, {}, {}};
RestoreFn g_original = nullptr;
Part g_savePart = {"AutomapSave", sig::AutomapSave, 18, false, nullptr, {}, {}};
Part g_insertPart = {"AutomapInsert", sig::AutomapInsert, 0, false, nullptr, {}, {}};
SaveFn g_saveOriginal = nullptr;
InsertFn g_insert = nullptr;
std::atomic<bool> g_memoryOn{false};       // both parts are live: a slot map's cells stay in memory, out of the character's map file
AutomapMemory g_memory;                     // per slot level: the cells of the map built last through it (automap_memory.h)
const uint8_t* g_currentObject = nullptr;   // the game's "current layer object" global, decoded from the part's own `cmp qword ptr [rip+x], 0`
std::atomic<bool> g_hooked{false};
std::mutex g_mutex;                         // the build (any thread), the player's level feed (server tick, or LevelChanged on the UI thread) and the read (game thread) share the skip state
std::vector<int> g_keptLogged;              // layers whose kept read was already logged for the armed skip (a waypoint load reads dozens)
SkipState g_skip;                           // armed / the layer left / the slot level / the player seen in it (automap_skip.h)
std::map<int, uint64_t> g_lastMaps;        // per slot level: the map built last through it in this game; building the same map again (the return from a town trip) keeps its cells
int g_playerLevel = -1;                     // the local player's level id, from the player's server tick or, on a TCP/IP client (no server tick), the LevelChanged event (-1 unknown)

// The layer object's key (levels.txt Layer) is its first dword; -1 when no object is current.
auto CurrentKey() noexcept -> int {
	if (g_currentObject == nullptr) {
		return -1;
	}
	const uint8_t* object = nullptr;
	std::memcpy(&object, g_currentObject, sizeof(object));
	if (object == nullptr) {
		return -1;
	}
	int key = -1;
	std::memcpy(&key, object, sizeof(key));
	return key;
}

// Puts a map's cells back into the current layer object, as the game's read does (kind 1: read back, so never saved again).
void Replay(const AutomapLists& lists) noexcept {
	if (g_currentObject == nullptr || g_insert == nullptr) {
		return;
	}
	uint8_t* object = nullptr;
	std::memcpy(&object, g_currentObject, sizeof(object));
	if (object == nullptr) {
		return;
	}
	alignas(16) uint8_t result[16];
	for (size_t i = 0; i < 4; ++i) {
		for (const AutomapCell& cell : lists[i]) {
			const InsertCell entry = {1, static_cast<uint16_t>(cell[0]), cell[1], cell[2]};
			g_insert(object + kObjectLists[i], result, &entry);
		}
	}
}

auto RestoreHook(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept -> uint64_t {
	bool skip = false;
	int key = -1;
	int level = -1;
	int slot = -1;
	bool logKept = false;
	ReadVerdict verdict = ReadVerdict::File;
	AutomapLists replay;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		key = CurrentKey();
		level = g_playerLevel;
		if (g_skip.armed) {
			slot = g_skip.armedLevel;
			skip = SkipOnRead(g_skip, key, level) == SkipVerdict::Skip;
			if (!skip && std::find(g_keptLogged.begin(), g_keptLogged.end(), key) == g_keptLogged.end()) {
				g_keptLogged.push_back(key);
				logKept = true;
			}
		}
		if (g_memoryOn.load()) {
			try {
				verdict = g_memory.OnRead(key, level, skip);
				if (verdict == ReadVerdict::Replay) {
					replay = *g_memory.Cells(level);
				}
			} catch (...) {
				verdict = ReadVerdict::File;  // out of memory: the game's own read
			}
		}
	}
	if (skip) {
		// the player stands in the slot level and another layer than the one left is current: this is the new map's own layer, first read. Skip it; the cells start empty.
		plog::Debug("automap: the revealed cells of an earlier map in layer %d were not read back for this new map (level %d)", key, level);
		return 0;
	}
	if (verdict == ReadVerdict::Replay) {
		// the return to a map: its own cells from memory, never the file's records of this layer (every earlier map's, and the game level's)
		Replay(replay);
		return 0;
	}
	if (logKept) {
		plog::Debug("automap: read of layer %d kept (the player is in level %d, the new map is level %d)", key, level, slot);
	}
	return g_original(a, b, c, d);
}

// The game appends a left layer's newly revealed cells to the character's map file. A slot map's are kept in memory instead, so they never load on a game level
// or a later map that shares the layer.
auto SaveHook(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept -> uint64_t {
	const auto* record = reinterpret_cast<const uint8_t*>(a);
	if (record != nullptr) {
		int key = -1;
		std::memcpy(&key, record, sizeof(key));
		std::lock_guard<std::mutex> lock(g_mutex);
		try {
			const bool kept = g_memory.OnSave(key, [record](auto&& add) {
				for (size_t i = 0; i < 4; ++i) {
					uint32_t bytes = 0;
					const int16_t* words = nullptr;
					std::memcpy(&bytes, record + kRecordSizes + 4 * i, sizeof(bytes));
					std::memcpy(&words, record + kRecordLists + kRecordListStride * i, sizeof(words));
					for (uint32_t w = 0; words != nullptr && w + 3 <= bytes / 2; w += 3) {
						add(static_cast<int>(i), AutomapCell{words[w], words[w + 1], words[w + 2]});
					}
				}
			});
			if (kept) {
				return 0;
			}
		} catch (...) {
			// out of memory: the game writes it to the file as before
		}
	}
	return g_saveOriginal(a, b, c, d);
}

}  // namespace

void InstallAutomapReset(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	try {
		ResolvePart(context, overrides, ranges, count, g_part);
		if (!g_part.status.on) {
			return;
		}
		// `cmp qword ptr [rip+disp32], 0` at +0x22 (48 83 3D disp32 00): the current layer object pointer. A part that does not look so stays off.
		const uint8_t* cmp = g_part.address + 0x22;
		if (cmp[0] != 0x48 || cmp[1] != 0x83 || cmp[2] != 0x3D) {
			g_part.status.on = false;
			g_part.status.detail = "off: the current layer object operand was not where expected";
			return;
		}
		int32_t disp = 0;
		std::memcpy(&disp, cmp + 3, sizeof(disp));
		g_currentObject = cmp + 8 + disp;  // the instruction ends after the 1-byte immediate: 3 + 4 + 1
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, g_part.guard, g_part.hookSize, reinterpret_cast<void*>(&RestoreHook), reinterpret_cast<void**>(&g_original));
		g_hooked = HookOutcome(g_part, hooked, g_original != nullptr);
		if (!g_hooked.load()) {
			return;
		}
		// The map memory needs both: the insert to play cells back, the writer to keep them. Without them the entry skip still works alone.
		ResolvePart(context, overrides, ranges, count, g_insertPart);
		ResolvePart(context, overrides, ranges, count, g_savePart);
		if (!g_insertPart.status.on || !g_savePart.status.on) {
			plog::Info("automap reset: map memory off (save %s, insert %s): a map can show its layer's saved cells after a town trip",
				g_savePart.status.on ? "found" : "missing", g_insertPart.status.on ? "found" : "missing");
			return;
		}
		g_insert = reinterpret_cast<InsertFn>(const_cast<uint8_t*>(g_insertPart.address));
		const uint64_t saveRva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_savePart.address) - context->exeBase);
		const bool saveHooked = context->InstallInlineHook(saveRva, g_savePart.guard, g_savePart.hookSize, reinterpret_cast<void*>(&SaveHook), reinterpret_cast<void**>(&g_saveOriginal));
		g_memoryOn = HookOutcome(g_savePart, saveHooked, g_saveOriginal != nullptr);
	} catch (...) {
		g_hooked = false;
		plog::Error("automap reset: installing the hook failed; a new slot map may show the revealed cells of an earlier one");
	}
}

void AutomapResetArm(uint64_t map, int slotLevel) noexcept {
	if (!g_hooked.load()) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	if (map != 0) {
		if (g_lastMaps[slotLevel] == map) {
			return;  // the same map again: a trip to town and back rebuilds it from the same seed, and its cells (in memory) belong to it
		}
		g_lastMaps[slotLevel] = map;
	}
	try {
		g_memory.NewMap(slotLevel);
	} catch (...) {
	}
	SkipArm(g_skip, CurrentKey(), slotLevel);
	g_keptLogged.clear();
}

void AutomapResetPlayerTick(int level) noexcept {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_playerLevel = level;
	SkipTick(g_skip, level);
}

void AutomapResetDisarm() noexcept {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_skip = {};
	g_keptLogged.clear();
	g_playerLevel = -1;
	g_lastMaps.clear();
	g_memory.Clear();
}

void AutomapResetPartStatuses(std::vector<PartStatus>& out) {
	out.push_back(g_part.status);
	out.push_back(g_savePart.status);
	out.push_back(g_insertPart.status);
}

}  // namespace carto
