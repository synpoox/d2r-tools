#include "tomb_growers.h"

#include "core/part_signatures.h"
#include "log.h"
#include "maze_hooks.h"

#include <atomic>

namespace carto {
namespace {

using GrowerFn = void (*)(void* level) noexcept;

enum TombPart { PreGrower, SpecialRooms, TombPartCount };
Part g_parts[TombPartCount] = {
	{"TombPreGrower", sig::MazeTombPreGrower, 15, false, nullptr, {}, {}},
	{"TombSpecialRooms", sig::MazeTombSpecialRooms, 15, false, nullptr, {}, {}},
};
GrowerFn g_original[TombPartCount] = {nullptr, nullptr};
std::atomic<bool> g_hooked{false};

// Both take (level) and return nothing the caller reads. A level that is not a slot build runs the game's own function.
void PreGrowerHook(void* level) noexcept {
	if (g_original[PreGrower] != nullptr && !MazeBuildSkipsTombGrower(level)) {
		g_original[PreGrower](level);
	}
}

void SpecialRoomsHook(void* level) noexcept {
	if (g_original[SpecialRooms] != nullptr && !MazeBuildSkipsTombGrower(level)) {
		g_original[SpecialRooms](level);
	}
}

}  // namespace

void InstallTombGrowerHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	try {
		for (Part& part : g_parts) {
			ResolvePart(context, overrides, ranges, count, part);
		}
		if (!g_parts[PreGrower].status.on || !g_parts[SpecialRooms].status.on) {
			// One grower alone is not enough: the second asserts when the first was skipped. Leave both un-hooked.
			for (Part& part : g_parts) {
				if (part.status.on) {
					part.status.on = false;
					part.status.detail = "off: not hooked, because the other Tombs grower was not found";
				}
			}
			return;
		}
		void* const targets[TombPartCount] = {reinterpret_cast<void*>(&PreGrowerHook), reinterpret_cast<void*>(&SpecialRoomsHook)};
		bool ok = true;
		for (int i = 0; i < TombPartCount && ok; ++i) {
			Part& part = g_parts[i];
			const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
			const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, targets[i], reinterpret_cast<void**>(&g_original[i]));
			ok = HookOutcome(part, hooked, g_original[i] != nullptr);
		}
		g_hooked = ok;
		if (!ok) {
			plog::Warn("tomb growers: a hook was refused (see 'cartographer check'); Tombs maps are refused");
		}
	} catch (...) {
		g_hooked = false;
		plog::Error("tomb growers: installing the hooks failed; Tombs maps are refused");
	}
}

auto TombGrowersHooked() -> bool {
	return g_hooked.load();
}

auto TombGrowerStatuses() -> std::vector<PartStatus> {
	return {g_parts[PreGrower].status, g_parts[SpecialRooms].status};
}

}  // namespace carto
