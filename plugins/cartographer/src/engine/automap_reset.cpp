#include "automap_reset.h"

#include "core/part_signatures.h"
#include "log.h"

#include <atomic>
#include <cstring>

namespace carto {
namespace {

// The restore (0xd5710) reads no register argument itself, but its first call (0xd5450, the map file path) is made with rcx as the caller left it, so the hook passes the
// four argument registers and the result straight on: a hook that took none and called plog first handed the original garbage (a fault in 0xd5450).
using RestoreFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t) noexcept;

Part g_part = {"AutomapRestore", sig::AutomapRestore, 17, false, nullptr, {}, {}};
RestoreFn g_original = nullptr;
const uint8_t* g_currentObject = nullptr;   // the game's "current layer object" global, decoded from the part's own `cmp qword ptr [rip+x], 0`
std::atomic<bool> g_hooked{false};
std::atomic<bool> g_armed{false};
std::atomic<uint64_t> g_lastMap{0};         // the map built last in this game (0: none); building the same map again (the return from a town trip) keeps its cells
std::atomic<int> g_armedKey{-1};
std::atomic<int> g_armedLevel{-1};          // the slot level (levels.txt id) whose layer the skip is for
std::atomic<int> g_playerLevel{-1};         // the local player's level id, from the player's tick (-1 unknown)            // the layer object that was current when the skip was armed (the one being left)

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

auto RestoreHook(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept -> uint64_t {
	if (g_armed.load()) {
		const int key = CurrentKey();
		const int level = g_playerLevel.load();
		if (key >= 0 && key != g_armedKey.load() && level == g_armedLevel.load()) {
			g_armed = false;  // the player stands in the slot level and another layer than the one left is current: this is the new map's own layer, first read. Skip it; the cells stay empty and are saved empty on leaving.
			plog::Info("automap: the revealed cells of an earlier map in layer %d were not read back for this new map (level %d)", key, level);
			return 0;
		}
		plog::Info("automap: read of layer %d kept (the player is in level %d, the new map is level %d)", key, level, g_armedLevel.load());
	}
	return g_original(a, b, c, d);
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
	} catch (...) {
		g_hooked = false;
		plog::Error("automap reset: installing the hook failed; a new slot map may show the revealed cells of an earlier one");
	}
}

void AutomapResetArm(uint64_t map, int slotLevel) noexcept {
	if (!g_hooked.load()) {
		return;
	}
	if (map != 0 && g_lastMap.exchange(map) == map) {
		return;  // the same map again: a trip to town and back rebuilds it from the same seed, and its saved cells belong to it
	}
	g_armedKey = CurrentKey();
	g_armedLevel = slotLevel;
	g_armed = true;
}

void AutomapResetPlayerTick(int level) noexcept {
	g_playerLevel = level;
}

void AutomapResetDisarm() noexcept {
	g_armed = false;
	g_playerLevel = -1;
	g_lastMap = 0;
}

auto AutomapResetPartStatus() -> PartStatus {
	return g_part.status;
}

}  // namespace carto
