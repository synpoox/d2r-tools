#include "automap_reset.h"

#include "core/part_signatures.h"
#include "log.h"

#include <atomic>
#include <cstring>

namespace carto {
namespace {

using RestoreFn = void (*)() noexcept;

Part g_part = {"AutomapRestore", sig::AutomapRestore, 17, false, nullptr, {}, {}};
RestoreFn g_original = nullptr;
const uint8_t* g_currentObject = nullptr;   // the game's "current layer object" global, decoded from the part's own `cmp qword ptr [rip+x], 0`
std::atomic<bool> g_hooked{false};
std::atomic<bool> g_armed{false};
std::atomic<int> g_armedKey{-1};            // the layer object that was current when the skip was armed (the one being left)

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

void RestoreHook() noexcept {
	if (g_armed.load()) {
		const int key = CurrentKey();
		if (key >= 0 && key != g_armedKey.load()) {
			g_armed = false;  // another layer is now current: this is the new map's first read. Skip it; the cells stay empty and are saved empty on leaving.
			plog::Info("automap: the revealed cells of an earlier map in layer %d were not read back for this new map", key);
			return;
		}
	}
	g_original();
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

void AutomapResetArm() noexcept {
	if (!g_hooked.load()) {
		return;
	}
	g_armedKey = CurrentKey();
	g_armed = true;
}

void AutomapResetDisarm() noexcept {
	g_armed = false;
}

auto AutomapResetPartStatus() -> PartStatus {
	return g_part.status;
}

}  // namespace carto
